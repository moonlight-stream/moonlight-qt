#include "streaming/video/overlaymenubutton.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QTemporaryDir>

#import <AppKit/AppKit.h>

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

void sendPointerEvent(NSInteger windowNumber)
{
    NSEvent* event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                        location:NSMakePoint(1, 1)
                                   modifierFlags:0
                                       timestamp:NSProcessInfo.processInfo.systemUptime
                                    windowNumber:windowNumber
                                         context:nil
                                     eventNumber:0
                                      clickCount:0
                                        pressure:0];
    [NSApp sendEvent:event];
}
}

int main(int argc, char* argv[])
{
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

    NSView* nativeView = static_cast<NSView*>(reinterpret_cast<void*>(button.winId()));
    require(nativeView.window != nil, "overlay button must have a native macOS window");
    const NSInteger windowNumber = nativeView.window.windowNumber;

    const int settledWakeCount = wakeCount;
    sendPointerEvent(0);
    require(!button.needsEventProcessing(),
            "pointer input for another macOS window must not wake the overlay");
    require(wakeCount == settledWakeCount,
            "unrelated native pointer input must leave the owner loop idle");

    sendPointerEvent(windowNumber);
    require(button.needsEventProcessing(),
            "native macOS pointer input must request Qt event processing");
    require(wakeCount == settledWakeCount + 1,
            "the first native pointer event must wake the owner loop");

    constexpr int nativeEventCount = 100000;
    for (int i = 0; i < nativeEventCount; ++i) {
        sendPointerEvent(windowNumber);
    }
    require(wakeCount == settledWakeCount + 1,
            "native macOS pointer bursts must be coalesced while pending");

    settleQtEvents(button);
    require(wakeCount == settledWakeCount + 1,
            "settling native pointer input must not create an idle wake loop");

    button.hideButton();
    sendPointerEvent(windowNumber);
    require(!button.needsEventProcessing(),
            "hidden button must ignore native macOS pointer events");

    qunsetenv("MOONLIGHT_DEVICE_LOCAL_SETTINGS_DIR");
    return 0;
}
