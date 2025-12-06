#include <QtGlobal>
#include <QDir>

#include "utils.h"

#include "SDL_compat.h"

#ifdef HAS_X11
#include <X11/Xlib.h>
#endif

#ifdef HAS_WAYLAND
#include <wayland-client.h>
#endif

#ifdef HAVE_DRM
#include <xf86drm.h>
#include <xf86drmMode.h>
#endif

#ifdef HAVE_EGL
#include <EGL/egl.h>
#endif

#define VALUE_SET 0x01
#define VALUE_TRUE 0x02

bool WMUtils::isRunningX11()
{
#ifdef HAS_X11
    static SDL_atomic_t isRunningOnX11;

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
#else
    return false;
#endif
}

bool WMUtils::isRunningNvidiaProprietaryDriver()
{
#ifdef HAVE_EGL
    static SDL_atomic_t isRunningOnNvidiaDriver;

    // If the value is not set yet, populate it now.
    int val = SDL_AtomicGet(&isRunningOnNvidiaDriver);
    if (!(val & VALUE_SET)) {
        bool nvidiaDriver = false;

        EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr)) {
            const char* vendorString = eglQueryString(display, EGL_VENDOR);
            nvidiaDriver = vendorString && strstr(vendorString, "NVIDIA") != NULL;
            eglTerminate(display);
        }

        // Populate the value to return and have for next time.
        // This can race with another thread populating the same data,
        // but that's no big deal.
        val = VALUE_SET | (nvidiaDriver ? VALUE_TRUE : 0);
        SDL_AtomicSet(&isRunningOnNvidiaDriver, val);
    }

    return !!(val & VALUE_TRUE);
#else
    return false;
#endif
}

bool WMUtils::isRunningWayland()
{
#ifdef HAS_WAYLAND
    static SDL_atomic_t isRunningOnWayland;

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
#else
    return false;
#endif
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

QString WMUtils::getDrmCardOverride()
{
#ifdef HAVE_DRM
    QDir dir("/dev/dri");
    QStringList cardList = dir.entryList(QStringList("card*"), QDir::Files | QDir::System);
    if (cardList.length() == 0) {
        return QString();
    }

    bool needsOverride = false;
    for (const QString& card : cardList) {
        QFile cardFd(dir.filePath(card));
        if (!cardFd.open(QFile::ReadOnly)) {
            continue;
        }

        auto resources = drmModeGetResources(cardFd.handle());
        if (resources == nullptr) {
            // If we find a card that doesn't have a display before a card that
            // has one, we'll need to override Qt's EGLFS config because they
            // don't properly handle cards without displays.
            needsOverride = true;
        }
        else {
            // We found a card with a display
            drmModeFreeResources(resources);
            if (needsOverride) {
                // Override the default card with this one
                return dir.filePath(card);
            }
            else {
                return QString();
            }
        }
    }
#endif

    return QString();
}

bool WMUtils::isEGLSafe()
{
    // We can use EGL if:
    // a) We're not using X11/Wayland (EGLFS requires it)
    // b) We're using Wayland (even XWayland is fine)
    // c) We're using X11 but not the Nvidia proprietary driver
    //
    // https://github.com/moonlight-stream/moonlight-qt/issues/1751
    return !WMUtils::isRunningWindowManager() || WMUtils::isRunningWayland() || !WMUtils::isRunningNvidiaProprietaryDriver();
}
