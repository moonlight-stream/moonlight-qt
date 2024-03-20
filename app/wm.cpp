#include <QtGlobal>

#include "utils.h"

#if HAVE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#ifdef HAS_X11
#include <X11/Xlib.h>
#endif

#ifdef HAS_WAYLAND
#include <wayland-client.h>
#endif

#define VALUE_SET 0x01
#define VALUE_TRUE 0x02

bool WMUtils::isRunningX11()
{
#ifdef HAS_X11
#if SDL_VERSION_ATLEAST(3, 0, 0)
    static SDL_AtomicInt isRunningOnX11;
#else
    static SDL_atomic_t isRunningOnX11;
#endif

    // If the value is not set yet, populate it now.
    int val = SDL_AtomicGet(&isRunningOnX11);
    if (!(val & VALUE_SET)) {
        Display* display = XOpenDisplay(nullptr);
        if (display != nullptr) {
            XCloseDisplay(display);
        }

        // Populate the value to return and have for next time.
        // This can race with another thread populating the same data,
        // but that's no big deal.
        val = VALUE_SET | ((display != nullptr) ? VALUE_TRUE : 0);
        SDL_AtomicSet(&isRunningOnX11, val);
    }

    return !!(val & VALUE_TRUE);
#endif

    return false;
}

bool WMUtils::isRunningWayland()
{
#ifdef HAS_WAYLAND
#if SDL_VERSION_ATLEAST(3, 0, 0)
    static SDL_AtomicInt isRunningOnWayland;
#else
    static SDL_atomic_t isRunningOnWayland;
#endif

    // If the value is not set yet, populate it now.
    int val = SDL_AtomicGet(&isRunningOnWayland);
    if (!(val & VALUE_SET)) {
        struct wl_display* display = wl_display_connect(nullptr);
        if (display != nullptr) {
            wl_display_disconnect(display);
        }

        // Populate the value to return and have for next time.
        // This can race with another thread populating the same data,
        // but that's no big deal.
        val = VALUE_SET | ((display != nullptr) ? VALUE_TRUE : 0);
        SDL_AtomicSet(&isRunningOnWayland, val);
    }

    return !!(val & VALUE_TRUE);
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

bool WMUtils::isRunningDesktopEnvironment()
{
    if (qEnvironmentVariableIsSet("HAS_DESKTOP_ENVIRONMENT")) {
        return qEnvironmentVariableIntValue("HAS_DESKTOP_ENVIRONMENT");
    }

#if defined(Q_OS_WIN) || defined(Q_OS_DARWIN)
    // Windows and macOS are always running a desktop environment
    return true;
#elif defined(EMBEDDED_BUILD)
    // Embedded systems don't run desktop environments
    return false;
#else
    // On non-embedded systems, assume we have a desktop environment
    // if we have a WM running.
    return isRunningWindowManager();
#endif
}
