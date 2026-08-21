#include "windowsdisplaygeometry.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#if defined(Q_OS_WIN32)
#include <windows.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtGui/qscreen_platform.h>
#endif
#endif

namespace WindowsDisplayGeometry {

bool Monitor::isValid() const
{
    return bounds.isValid() && workArea.isValid() && !name.isEmpty();
}

QScreen* screenForName(const QString& name)
{
    const auto screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen->name().compare(name, Qt::CaseInsensitive) == 0) {
            return screen;
        }
    }

    return nullptr;
}

#if defined(Q_OS_WIN32)
namespace {
QRect fromNativeRect(const RECT& rect)
{
    return QRect(rect.left,
                 rect.top,
                 rect.right - rect.left,
                 rect.bottom - rect.top);
}

bool populateMonitor(HMONITOR nativeMonitor, Monitor& monitor)
{
    if (!nativeMonitor) {
        return false;
    }

    MONITORINFOEXW monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(nativeMonitor, reinterpret_cast<MONITORINFO*>(&monitorInfo))) {
        return false;
    }

    Monitor result;
    result.bounds = fromNativeRect(monitorInfo.rcMonitor);
    result.workArea = fromNativeRect(monitorInfo.rcWork);
    result.name = QString::fromWCharArray(monitorInfo.szDevice);
    if (!result.isValid()) {
        return false;
    }

    monitor = result;
    return true;
}

struct MonitorSearchContext
{
    QString name;
    Monitor monitor;
};

BOOL CALLBACK findMonitorByName(HMONITOR nativeMonitor, HDC, LPRECT, LPARAM data)
{
    auto* context = reinterpret_cast<MonitorSearchContext*>(data);
    Monitor candidate;
    if (populateMonitor(nativeMonitor, candidate) &&
            candidate.name.compare(context->name, Qt::CaseInsensitive) == 0) {
        context->monitor = candidate;
        return FALSE;
    }

    return TRUE;
}

bool monitorForNameExact(const QString& name, Monitor& monitor)
{
    if (name.isEmpty()) {
        return false;
    }

    MonitorSearchContext context { name, {} };
    EnumDisplayMonitors(nullptr,
                        nullptr,
                        findMonitorByName,
                        reinterpret_cast<LPARAM>(&context));
    if (!context.monitor.isValid()) {
        return false;
    }

    monitor = context.monitor;
    return true;
}

bool monitorForScreenExact(QScreen* screen, Monitor& monitor)
{
    if (!screen) {
        return false;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (auto* nativeScreen = screen->nativeInterface<QNativeInterface::QWindowsScreen>()) {
        if (populateMonitor(nativeScreen->handle(), monitor)) {
            return true;
        }
    }
#endif

    return monitorForNameExact(screen->name(), monitor);
}

HWND windowHandle(QWindow* window)
{
    return window ? reinterpret_cast<HWND>(window->winId()) : nullptr;
}

bool sameMonitor(const Monitor& first, const Monitor& second)
{
    return first.isValid() && second.isValid() &&
            (first.name.compare(second.name, Qt::CaseInsensitive) == 0 ||
             first.bounds == second.bounds);
}
} // namespace
#endif

bool monitorForName(const QString& name, Monitor& monitor)
{
#if defined(Q_OS_WIN32)
    return monitorForNameExact(name, monitor);
#else
    Q_UNUSED(name)
    Q_UNUSED(monitor)
#endif

    return false;
}

bool monitorForScreen(QScreen* screen, Monitor& monitor)
{
#if defined(Q_OS_WIN32)
    if (!screen) {
        return false;
    }

    if (monitorForScreenExact(screen, monitor)) {
        return true;
    }

    const QPoint center = screen->geometry().center();
    const POINT nativeCenter { center.x(), center.y() };
    return populateMonitor(
            MonitorFromPoint(nativeCenter, MONITOR_DEFAULTTONEAREST), monitor);
#else
    Q_UNUSED(screen)
    Q_UNUSED(monitor)
    return false;
#endif
}

bool monitorForWindow(QWindow* window, Monitor& monitor)
{
#if defined(Q_OS_WIN32)
    const HWND nativeWindow = windowHandle(window);
    return nativeWindow && populateMonitor(
            MonitorFromWindow(nativeWindow, MONITOR_DEFAULTTONEAREST), monitor);
#else
    Q_UNUSED(window)
    Q_UNUSED(monitor)
    return false;
#endif
}

bool monitorForRect(const QRect& rect, Monitor& monitor)
{
#if defined(Q_OS_WIN32)
    if (!rect.isValid()) {
        return false;
    }

    RECT nativeRect {
        rect.left(),
        rect.top(),
        rect.right() + 1,
        rect.bottom() + 1,
    };
    return populateMonitor(
            MonitorFromRect(&nativeRect, MONITOR_DEFAULTTONEAREST), monitor);
#else
    Q_UNUSED(rect)
    Q_UNUSED(monitor)
    return false;
#endif
}

QScreen* screenForMonitor(const Monitor& monitor)
{
    if (!monitor.isValid()) {
        return nullptr;
    }

    if (QScreen* screen = screenForName(monitor.name)) {
        return screen;
    }

#if defined(Q_OS_WIN32)
    const auto screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        Monitor candidate;
        if (monitorForScreenExact(screen, candidate) && sameMonitor(candidate, monitor)) {
            return screen;
        }
    }
#endif

    return nullptr;
}

QRect windowRect(QWindow* window)
{
#if defined(Q_OS_WIN32)
    RECT nativeRect = {};
    const HWND nativeWindow = windowHandle(window);
    if (nativeWindow && GetWindowRect(nativeWindow, &nativeRect)) {
        return fromNativeRect(nativeRect);
    }
#else
    Q_UNUSED(window)
#endif

    return {};
}

bool clientRectForOverlappedWindow(QWindow* referenceWindow,
                                   const QRect& frameRect,
                                   QRect& clientRect)
{
#if defined(Q_OS_WIN32)
    const HWND nativeWindow = windowHandle(referenceWindow);
    if (!nativeWindow || !frameRect.isValid()) {
        return false;
    }

    RECT frameAdjustment = {};
    const UINT dpi = GetDpiForWindow(nativeWindow);
    if (!AdjustWindowRectExForDpi(&frameAdjustment,
                                  WS_OVERLAPPEDWINDOW,
                                  FALSE,
                                  0,
                                  dpi ? dpi : USER_DEFAULT_SCREEN_DPI)) {
        return false;
    }

    const int frameWidth = frameAdjustment.right - frameAdjustment.left;
    const int frameHeight = frameAdjustment.bottom - frameAdjustment.top;
    const int clientWidth = frameRect.width() - frameWidth;
    const int clientHeight = frameRect.height() - frameHeight;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return false;
    }

    clientRect = QRect(frameRect.x() - frameAdjustment.left,
                       frameRect.y() - frameAdjustment.top,
                       clientWidth,
                       clientHeight);
    return true;
#else
    Q_UNUSED(referenceWindow)
    Q_UNUSED(frameRect)
    Q_UNUSED(clientRect)
    return false;
#endif
}

bool setWindowRect(QWindow* window, const QRect& geometry, quint32* errorCode)
{
    if (errorCode) {
        *errorCode = 0;
    }

#if defined(Q_OS_WIN32)
    const HWND nativeWindow = windowHandle(window);
    if (!nativeWindow || !geometry.isValid()) {
        if (errorCode) {
            *errorCode = nativeWindow ? ERROR_INVALID_PARAMETER : ERROR_INVALID_WINDOW_HANDLE;
        }
        return false;
    }

    SetLastError(ERROR_SUCCESS);
    if (SetWindowPos(nativeWindow,
                     nullptr,
                     geometry.x(),
                     geometry.y(),
                     geometry.width(),
                     geometry.height(),
                     SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER)) {
        return true;
    }

    if (errorCode) {
        *errorCode = GetLastError();
    }
#else
    Q_UNUSED(window)
    Q_UNUSED(geometry)
#endif

    return false;
}

bool isNormalWindow(QWindow* window)
{
#if defined(Q_OS_WIN32)
    const HWND nativeWindow = windowHandle(window);
    return nativeWindow && !IsIconic(nativeWindow) && !IsZoomed(nativeWindow);
#else
    Q_UNUSED(window)
    return false;
#endif
}

qreal scaleFactor(const Monitor& monitor, QScreen* preferredScreen)
{
    if (!monitor.isValid()) {
        return 1.0;
    }

#if defined(Q_OS_WIN32)
    QScreen* screen = preferredScreen;
    Monitor screenMonitor;
    if (!screen || !monitorForScreenExact(screen, screenMonitor) ||
            !sameMonitor(screenMonitor, monitor)) {
        screen = screenForMonitor(monitor);
    }

    if (screen && screen->geometry().width() > 0) {
        return static_cast<qreal>(monitor.bounds.width()) / screen->geometry().width();
    }
#else
    Q_UNUSED(preferredScreen)
#endif

    return 1.0;
}

} // namespace WindowsDisplayGeometry
