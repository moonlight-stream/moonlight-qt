#include "utils.h"

#ifdef HAS_X11
#include <X11/Xlib.h>
#endif

#ifdef HAS_WAYLAND
#include <wayland-client.h>
#endif

bool WMUtils::isRunningX11()
{
#ifdef HAS_X11
    Display* display = XOpenDisplay(nullptr);
    if (display != nullptr) {
        XCloseDisplay(display);
        return true;
    }
#endif

    return false;
}

bool WMUtils::isRunningWayland()
{
#ifdef HAS_WAYLAND
    struct wl_display* display = wl_display_connect(nullptr);
    if (display != nullptr) {
        wl_display_disconnect(display);
        return true;
    }
#endif

    return false;
}

bool WMUtils::isRunningWindowManager()
{
#if defined(Q_OS_WIN) || defined(Q_OS_DARWIN)
    // Windows and macOS are always running a window manager
    return true;
#else
    // On Unix OSes, look for Wayland or X
    return WMUtils::isRunningWayland() || WMUtils::isRunningX11();
#endif
}
