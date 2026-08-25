#include "streaming/video/overlaymenubutton.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QtGlobal>

#ifdef Q_OS_WIN32
#include <windows.h>
#endif

namespace {
void require(bool condition, const char* message)
{
    if (!condition) {
        qFatal("%s", message);
    }
}

void settleQtEvents(OverlayMenuButton& button)
{
    for (int pass = 0; pass < 16 && button.needsEventProcessing(); ++pass) {
        button.beginEventProcessing();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
    require(!button.needsEventProcessing(),
            "overlay button event processing must settle");
}
}

int main(int argc, char* argv[])
{
#ifndef Q_OS_WIN32
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    return 0;
#else
    QTemporaryDir settingsDirectory;
    require(settingsDirectory.isValid(), "temporary settings directory must be available");
    qputenv("MOONLIGHT_DEVICE_LOCAL_SETTINGS_DIR",
            settingsDirectory.path().toLocal8Bit());

    QGuiApplication app(argc, argv);
    OverlayMenuButton button;
    int wakeCount = 0;
    button.setEventWakeCallback([&wakeCount]() { wakeCount++; });
    button.showButton(100, 100, 800, 600);
    settleQtEvents(button);

    const int settledWakeCount = wakeCount;
    HWND hwnd = reinterpret_cast<HWND>(button.winId());
    require(hwnd != nullptr, "overlay button must have a native window");

    constexpr int nativeMessageCount = 100000;
    for (int i = 0; i < nativeMessageCount; ++i) {
        require(PostMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(i % 32, i % 32)) != FALSE,
                "native pointer pressure message must be queued");

        // Keep the Windows thread queue below its PostMessage limit while still
        // presenting large bursts to the wake coalescer.
        if ((i + 1) % 1000 == 0) {
            MSG message;
            while (PeekMessageW(&message, hwnd, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            // Match the production loop: consume the wake edge and dispatch
            // the Qt events before accepting the next native-message burst.
            settleQtEvents(button);
        }
    }

    require(PostMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(1, 1)) != FALSE,
            "final pointer message must be queued");
    MSG finalMessage;
    while (PeekMessageW(&finalMessage, hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&finalMessage);
        DispatchMessageW(&finalMessage);
    }

    require(button.needsEventProcessing(),
            "native button input must request Qt event processing");
    require(wakeCount - settledWakeCount >= nativeMessageCount / 1000,
            "each consumed native input burst must wake the owner loop");

    // A burst is represented by a bounded number of wake edges instead of one
    // SDL wake event per native message.
    require(wakeCount - settledWakeCount < nativeMessageCount / 100,
            "native input wakeups must be coalesced under pressure");

    settleQtEvents(button);
    button.hideButton();
    require(!button.needsEventProcessing(),
            "hidden button must not keep the Qt event pump active");

    qunsetenv("MOONLIGHT_DEVICE_LOCAL_SETTINGS_DIR");
    return 0;
#endif
}
