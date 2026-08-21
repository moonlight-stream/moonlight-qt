#include "windowswindowchrome.h"

#include <QCoreApplication>
#include <QScreen>
#include <QTimer>

#ifdef Q_OS_WIN32
#include <windows.h>
#include <windowsx.h>

namespace {
const char* visibilityName(QWindow::Visibility visibility)
{
    switch (visibility) {
    case QWindow::Hidden:     return "hidden";
    case QWindow::AutomaticVisibility: return "automatic";
    case QWindow::Windowed:   return "windowed";
    case QWindow::Minimized:  return "minimized";
    case QWindow::Maximized:  return "maximized";
    case QWindow::FullScreen: return "fullscreen";
    }
    return "unknown";
}

QString nativeRectText(const RECT& rect)
{
    return QStringLiteral("[%1,%2 %3x%4]")
            .arg(rect.left)
            .arg(rect.top)
            .arg(rect.right - rect.left)
            .arg(rect.bottom - rect.top);
}

QString qtRectText(const QRect& rect)
{
    return QStringLiteral("[%1,%2 %3x%4]")
            .arg(rect.x())
            .arg(rect.y())
            .arg(rect.width())
            .arg(rect.height());
}
}
#endif

WindowsWindowChrome::WindowsWindowChrome(QObject* parent)
    : QObject(parent)
{
}

WindowsWindowChrome::~WindowsWindowChrome()
{
    if (m_Installed && QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

QWindow* WindowsWindowChrome::window() const
{
    return m_Window;
}

void WindowsWindowChrome::setWindow(QWindow* window)
{
    if (m_Window == window) {
        return;
    }

    if (m_Window) {
        disconnect(m_Window, nullptr, this, nullptr);
    }

    m_Window = window;
    m_WindowId = 0;
    m_Maximized = false;
    m_StateUpdatePending = false;
    if (m_Window) {
        connect(m_Window, &QWindow::visibilityChanged,
                this, &WindowsWindowChrome::scheduleNativeStateUpdate);
    }
    emit windowChanged();
}

QQuickItem* WindowsWindowChrome::titleBar() const
{
    return m_TitleBar;
}

void WindowsWindowChrome::setTitleBar(QQuickItem* titleBar)
{
    if (m_TitleBar == titleBar) {
        return;
    }

    m_TitleBar = titleBar;
    emit titleBarChanged();
}

bool WindowsWindowChrome::isMaximized() const
{
#ifdef Q_OS_WIN32
    return m_WindowId && IsZoomed(reinterpret_cast<HWND>(m_WindowId));
#else
    return m_Window && m_Window->visibility() == QWindow::Maximized;
#endif
}

void WindowsWindowChrome::activate()
{
#ifdef Q_OS_WIN32
    if (!m_Window) {
        return;
    }

    m_WindowId = m_Window->winId();
    if (!m_Installed && QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
        m_Installed = true;
    }

    const HWND nativeWindow = reinterpret_cast<HWND>(m_WindowId);
    const LONG_PTR style = GetWindowLongPtrW(nativeWindow, GWL_STYLE);
    SetLastError(ERROR_SUCCESS);
    if (SetWindowLongPtrW(nativeWindow,
                          GWL_STYLE,
                          (style & ~WS_POPUP) | WS_OVERLAPPEDWINDOW) == 0) {
        const DWORD error = GetLastError();
        if (error != ERROR_SUCCESS) {
            qWarning() << "Failed to apply native window styles:" << error;
        }
    }

    if (!SetWindowPos(nativeWindow,
                      nullptr,
                      0,
                      0,
                      0,
                      0,
                      SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                              SWP_NOZORDER | SWP_NOACTIVATE)) {
        const DWORD error = GetLastError();
        qWarning() << "Failed to refresh native window frame:" << error;
    }

    m_Maximized = IsZoomed(nativeWindow);
#endif
}

void WindowsWindowChrome::minimize()
{
#ifdef Q_OS_WIN32
    postSystemCommand(SC_MINIMIZE, QStringLiteral("minimize"));
#else
    if (m_Window) {
        m_Window->showMinimized();
    }
#endif
}

void WindowsWindowChrome::toggleMaximized()
{
#ifdef Q_OS_WIN32
    if (isMaximized()) {
        postSystemCommand(SC_RESTORE, QStringLiteral("restore"));
    }
    else {
        postSystemCommand(SC_MAXIMIZE, QStringLiteral("maximize"));
    }
#else
    if (m_Window) {
        if (m_Window->visibility() == QWindow::Maximized) {
            m_Window->showNormal();
        }
        else {
            m_Window->showMaximized();
        }
    }
#endif
}

void WindowsWindowChrome::close()
{
#ifdef Q_OS_WIN32
    postSystemCommand(SC_CLOSE, QStringLiteral("close"));
#else
    if (m_Window) {
        m_Window->close();
    }
#endif
}

void WindowsWindowChrome::postSystemCommand(unsigned int command, const QString& action)
{
#ifdef Q_OS_WIN32
    if (!m_WindowId) {
        return;
    }

    const HWND nativeWindow = reinterpret_cast<HWND>(m_WindowId);
    logWindowState(action, "button-before");
    if (!PostMessageW(nativeWindow, WM_SYSCOMMAND, command, 0)) {
        const DWORD error = GetLastError();
        qWarning() << "Failed to post native window command" << action << error;
        return;
    }

    // WM_SYSCOMMAND is asynchronous. Log once the resulting window-state
    // messages have had a chance to update both the native and Qt state.
    QTimer::singleShot(100, this, [this, action]() {
        scheduleNativeStateUpdate();
        logWindowState(action, "button-after");
    });
#else
    Q_UNUSED(command)
    Q_UNUSED(action)
#endif
}

void WindowsWindowChrome::logWindowState(const QString& action, const char* phase) const
{
#ifdef Q_OS_WIN32
    if (!m_Window || !m_WindowId) {
        return;
    }

    const HWND nativeWindow = reinterpret_cast<HWND>(m_WindowId);
    RECT windowRect = {};
    GetWindowRect(nativeWindow, &windowRect);

    WINDOWPLACEMENT placement = { sizeof(placement) };
    GetWindowPlacement(nativeWindow, &placement);

    MONITORINFOEXW monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    const HMONITOR monitor = MonitorFromWindow(nativeWindow, MONITOR_DEFAULTTONEAREST);
    if (monitor) {
        GetMonitorInfoW(monitor, reinterpret_cast<LPMONITORINFO>(&monitorInfo));
    }

    qInfo().noquote()
            << QStringLiteral("Window action=%1 phase=%2 qtVisibility=%3 nativeMaximized=%4 "
                              "nativeMinimized=%5 window=%6 normalWorkspace=%7 qtGeometry=%8 "
                              "dpr=%9 placementFlags=0x%10 showCmd=%11 monitor=%12 "
                              "monitorRect=%13 workRect=%14 qtScreen=%15")
                       .arg(action,
                            QString::fromLatin1(phase),
                            QString::fromLatin1(visibilityName(m_Window->visibility())))
                       .arg(IsZoomed(nativeWindow) ? 1 : 0)
                       .arg(IsIconic(nativeWindow) ? 1 : 0)
                       .arg(nativeRectText(windowRect),
                            nativeRectText(placement.rcNormalPosition),
                            qtRectText(m_Window->geometry()))
                       .arg(m_Window->devicePixelRatio())
                       .arg(placement.flags, 0, 16)
                       .arg(placement.showCmd)
                       .arg(QString::fromWCharArray(monitorInfo.szDevice),
                            nativeRectText(monitorInfo.rcMonitor),
                            nativeRectText(monitorInfo.rcWork),
                            m_Window->screen() ? m_Window->screen()->name() : QStringLiteral("unknown"));
#else
    Q_UNUSED(action)
    Q_UNUSED(phase)
#endif
}

void WindowsWindowChrome::scheduleNativeStateUpdate()
{
    if (m_StateUpdatePending) {
        return;
    }

    m_StateUpdatePending = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_StateUpdatePending = false;
        const bool maximized = isMaximized();
        if (m_Maximized != maximized) {
            m_Maximized = maximized;
            emit maximizedChanged();
        }
    }, Qt::QueuedConnection);
}

bool WindowsWindowChrome::nativeEventFilter(const QByteArray& eventType,
                                            void* message,
                                            WindowsWindowChrome::NativeEventResult* result)
{
#ifdef Q_OS_WIN32
    if (eventType != "windows_generic_MSG" || !m_Window || !m_TitleBar || !m_WindowId) {
        return false;
    }

    auto* nativeMessage = static_cast<MSG*>(message);
    if (nativeMessage->hwnd != reinterpret_cast<HWND>(m_WindowId)) {
        return false;
    }

    if (nativeMessage->message == WM_NCCALCSIZE) {
        if (IsZoomed(nativeMessage->hwnd)) {
            const HMONITOR monitor = MonitorFromWindow(nativeMessage->hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitorInfo = { sizeof(monitorInfo) };
            if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
                if (nativeMessage->wParam) {
                    auto* parameters = reinterpret_cast<NCCALCSIZE_PARAMS*>(nativeMessage->lParam);
                    parameters->rgrc[0] = monitorInfo.rcWork;
                }
                else {
                    *reinterpret_cast<RECT*>(nativeMessage->lParam) = monitorInfo.rcWork;
                }
            }
        }
        *result = 0;
        return true;
    }

    if (nativeMessage->message == WM_SIZE ||
            nativeMessage->message == WM_WINDOWPOSCHANGED ||
            nativeMessage->message == WM_SHOWWINDOW) {
        scheduleNativeStateUpdate();
    }

    if (nativeMessage->message == WM_GETMINMAXINFO) {
        const HMONITOR monitor = MonitorFromWindow(nativeMessage->hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = { sizeof(monitorInfo) };
        if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
            auto* minMaxInfo = reinterpret_cast<MINMAXINFO*>(nativeMessage->lParam);
            minMaxInfo->ptMaxPosition.x = monitorInfo.rcWork.left - monitorInfo.rcMonitor.left;
            minMaxInfo->ptMaxPosition.y = monitorInfo.rcWork.top - monitorInfo.rcMonitor.top;
            minMaxInfo->ptMaxSize.x = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
            minMaxInfo->ptMaxSize.y = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
            *result = 0;
            return true;
        }
    }

    if (nativeMessage->message != WM_NCHITTEST ||
            m_Window->visibility() == QWindow::FullScreen ||
            !m_TitleBar->isVisible()) {
        return false;
    }

    const POINT screenPoint = {
        GET_X_LPARAM(nativeMessage->lParam),
        GET_Y_LPARAM(nativeMessage->lParam)
    };

    if (!IsZoomed(nativeMessage->hwnd) && !IsIconic(nativeMessage->hwnd)) {
        RECT windowRect;
        if (GetWindowRect(nativeMessage->hwnd, &windowRect)) {
            const UINT dpi = GetDpiForWindow(nativeMessage->hwnd);
            const int horizontalBorder = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) +
                                         GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
            const int verticalBorder = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
                                       GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

            const bool onLeft = screenPoint.x < windowRect.left + horizontalBorder;
            const bool onRight = screenPoint.x >= windowRect.right - horizontalBorder;
            const bool onTop = screenPoint.y < windowRect.top + verticalBorder;
            const bool onBottom = screenPoint.y >= windowRect.bottom - verticalBorder;

            if (onTop && onLeft) {
                *result = HTTOPLEFT;
                return true;
            }
            if (onTop && onRight) {
                *result = HTTOPRIGHT;
                return true;
            }
            if (onBottom && onLeft) {
                *result = HTBOTTOMLEFT;
                return true;
            }
            if (onBottom && onRight) {
                *result = HTBOTTOMRIGHT;
                return true;
            }
            if (onLeft) {
                *result = HTLEFT;
                return true;
            }
            if (onRight) {
                *result = HTRIGHT;
                return true;
            }
            if (onTop) {
                *result = HTTOP;
                return true;
            }
            if (onBottom) {
                *result = HTBOTTOM;
                return true;
            }
        }
    }

    POINT clientPoint = screenPoint;
    if (!ScreenToClient(nativeMessage->hwnd, &clientPoint)) {
        return false;
    }

    const qreal scale = m_Window->devicePixelRatio();
    const QPointF scenePoint(clientPoint.x / scale, clientPoint.y / scale);
    const QRectF titleBarRect = m_TitleBar->mapRectToScene(
            QRectF(0, 0, m_TitleBar->width(), m_TitleBar->height()));
    if (titleBarRect.contains(scenePoint)) {
        *result = HTCAPTION;
        return true;
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif

    return false;
}
