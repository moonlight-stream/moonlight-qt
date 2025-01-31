#include "SDL_compat.h"

#include <SDL_syswm.h>

void* SDLC_Win32_GetHwnd(SDL_Window* window)
{
#ifdef SDL_VIDEO_DRIVER_WINDOWS
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return NULL;
    }

    if (info.subsystem == SDL_SYSWM_WINDOWS) {
        return info.info.win.window;
    }
#else
    (void)window;
#endif

    return NULL;
}

void* SDLC_MacOS_GetWindow(SDL_Window* window)
{
#ifdef SDL_VIDEO_DRIVER_COCOA
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return NULL;
    }

    if (info.subsystem == SDL_SYSWM_COCOA) {
        return info.info.cocoa.window;
    }
#else
    (void)window;
#endif

    return NULL;
}

void* SDLC_X11_GetDisplay(SDL_Window* window)
{
#ifdef SDL_VIDEO_DRIVER_X11
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return NULL;
    }

    if (info.subsystem == SDL_SYSWM_X11) {
        return info.info.x11.display;
    }
#else
    (void)window;
#endif

    return NULL;
}

unsigned long SDLC_X11_GetWindow(SDL_Window* window)
{
#ifdef SDL_VIDEO_DRIVER_X11
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return 0;
    }

    if (info.subsystem == SDL_SYSWM_X11) {
        return info.info.x11.window;
    }
#else
    (void)window;
#endif

    return 0;
}

void* SDLC_Wayland_GetDisplay(SDL_Window* window)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return NULL;
    }

    if (info.subsystem == SDL_SYSWM_WAYLAND) {
        return info.info.wl.display;
    }
#else
    (void)window;
#endif

    return NULL;
}

void* SDLC_Wayland_GetSurface(SDL_Window* window)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return NULL;
    }

    if (info.subsystem == SDL_SYSWM_WAYLAND) {
        return info.info.wl.surface;
    }
#else
    (void)window;
#endif

    return NULL;
}

int SDLC_KMSDRM_GetFd(SDL_Window* window)
{
#if defined(SDL_VIDEO_DRIVER_KMSDRM) && SDL_VERSION_ATLEAST(2, 0, 15)
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return -1;
    }

    if (info.subsystem == SDL_SYSWM_KMSDRM) {
        return info.info.kmsdrm.drm_fd;
    }
#else
    (void)window;
#endif

    return -1;
}

int SDLC_KMSDRM_GetDevIndex(SDL_Window* window)
{
#if defined(SDL_VIDEO_DRIVER_KMSDRM) && SDL_VERSION_ATLEAST(2, 0, 15)
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return -1;
    }

    if (info.subsystem == SDL_SYSWM_KMSDRM) {
        return info.info.kmsdrm.dev_index;
    }
#else
    (void)window;
#endif

    return -1;
}

SDLC_VideoDriver SDLC_GetVideoDriver(void)
{
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
    return SDLC_VIDEO_WIN32;
#elif defined(SDL_VIDEO_DRIVER_COCOA)
    return SDLC_VIDEO_MACOS;
#else
    const char* videoDriver = SDL_GetCurrentVideoDriver();
    if (SDL_strcmp(videoDriver, "x11") == 0) {
        return SDLC_VIDEO_X11;
    }
    else if (SDL_strcmp(videoDriver, "wayland") == 0) {
        return SDLC_VIDEO_WAYLAND;
    }
    else if (SDL_strcmp(videoDriver, "kmsdrm") == 0) {
        return SDLC_VIDEO_KMSDRM;
    }
    else {
        SDL_assert(0);
        return SDLC_VIDEO_UNKNOWN;
    }
#endif
}

bool SDLC_IsFullscreen(SDL_Window* window)
{
    return !!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN);
}

bool SDLC_IsFullscreenExclusive(SDL_Window* window)
{
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN;
}

bool SDLC_IsFullscreenDesktop(SDL_Window* window)
{
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
}

void SDLC_EnterFullscreen(SDL_Window* window, bool exclusive)
{
    SDL_SetWindowFullscreen(window, exclusive ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void SDLC_LeaveFullscreen(SDL_Window* window)
{
    SDL_SetWindowFullscreen(window, 0);
}
