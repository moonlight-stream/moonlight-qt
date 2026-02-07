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

#ifndef EGL_PLATFORM_X11_KHR
#define EGL_PLATFORM_X11_KHR 0x31D5
#endif

#ifndef EGL_PLATFORM_GBM_KHR
#define EGL_PLATFORM_GBM_KHR 0x31D7
#endif
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

bool WMUtils::isRunningNvidiaProprietaryDriverX11()
{
#ifdef HAVE_EGL
    static SDL_atomic_t isRunningOnNvidiaDriver;

    // If the value is not set yet, populate it now.
    int val = SDL_AtomicGet(&isRunningOnNvidiaDriver);
    if (!(val & VALUE_SET)) {
        bool nvidiaDriver = false;

        // Open the default X11 display. This is critical for accurate detection of the
        // Nvidia driver under XWayland because eglGetDisplay(EGL_DEFAULT_DISPLAY) will
        // return the Wayland display but Qt will use the X11 display.
        EGLDisplay display = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, EGL_DEFAULT_DISPLAY, nullptr);
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

bool WMUtils::supportsDesktopGLWithEGL()
{
#ifdef HAVE_EGL
    static SDL_atomic_t supportsDesktopGL;

    // If the value is not set yet, populate it now.
    int val = SDL_AtomicGet(&supportsDesktopGL);
    if (!(val & VALUE_SET)) {
        // Assume it does if we can't confirm
        bool desktopGL = true;

        // Prefer GBM as some drivers (pvr) use swrast/llvmpipe for X11/XWayland,
        // so we'll get a different (and incorrect) result if we query X11.
        EGLDisplay display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, EGL_DEFAULT_DISPLAY, nullptr);
        if (display == EGL_NO_DISPLAY) {
            display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        }
        if (display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr)) {
            EGLint matchingConfigs = 0;
            EGLint const attribs[] =
            {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                EGL_NONE
            };

            desktopGL = eglChooseConfig(display, attribs, nullptr, 0, &matchingConfigs) == EGL_TRUE &&
                        matchingConfigs > 0;
            eglTerminate(display);
        }

        // Populate the value to return and have for next time.
        // This can race with another thread populating the same data,
        // but that's no big deal.
        val = VALUE_SET | (desktopGL ? VALUE_TRUE : 0);
        SDL_AtomicSet(&supportsDesktopGL, val);
    }

    return !!(val & VALUE_TRUE);
#else
    // Assume it does if we can't check ourselves
    return true;
#endif
}

bool WMUtils::isRunningWayland()
{
#ifdef HAS_WAYLAND
    static SDL_atomic_t isRunningOnWayland;

    // If the value is not set yet, populate it now.
    int val = SDL_AtomicGet(&isRunningOnWayland);
    if (!(val & VALUE_SET)) {
        struct wl_display* display = nullptr;

        // We need to avoid the default fallback to wayland-0 that wl_display_connect()
        // will try for cases where we might be running from a TTY with a Wayland
        // compositor running in another VT that happens to use the wayland-0 name.
        if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") ||
            !qEnvironmentVariableIsEmpty("WAYLAND_SOCKET") ||
            qgetenv("XDG_SESSION_TYPE") == "wayland") {

            // This looks like it might be a Wayland environment, so give it a shot
            display = wl_display_connect(nullptr);
            if (display != nullptr) {
                wl_display_disconnect(display);
            }
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
    bool value;
    if (Utils::getEnvironmentVariableOverride("HAS_DESKTOP_ENVIRONMENT", &value)) {
        return value;
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

bool WMUtils::isGpuSlow()
{
    bool ret;

    if (!Utils::getEnvironmentVariableOverride("GL_IS_SLOW", &ret)) {
#if defined(GL_IS_SLOW) || (!defined(Q_PROCESSOR_X86) && !defined(Q_OS_DARWIN) && !defined(Q_OS_WIN))
        // We currently assume GPUs on non-x86 hardware are slow by default
        ret = true;
#else
        ret = false;
#endif
    }

    return ret;
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
