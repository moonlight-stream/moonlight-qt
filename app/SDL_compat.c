#include "SDL_compat.h"

static SDL_PropertiesID getWindowProperties(SDL_Window* window)
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    if (!properties) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowProperties() failed: %s",
                     SDL_GetError());
    }
    return properties;
}

void* SDLC_Win32_GetHwnd(SDL_Window* window)
{
    return SDL_GetPointerProperty(getWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
}

void* SDLC_MacOS_GetWindow(SDL_Window* window)
{
    return SDL_GetPointerProperty(getWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
}

void* SDLC_X11_GetDisplay(SDL_Window* window)
{
    return SDL_GetPointerProperty(getWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
}

unsigned long SDLC_X11_GetWindow(SDL_Window* window)
{
    return (unsigned long)SDL_GetNumberProperty(getWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
}

void* SDLC_Wayland_GetDisplay(SDL_Window* window)
{
    return SDL_GetPointerProperty(getWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
}

void* SDLC_Wayland_GetSurface(SDL_Window* window)
{
    return SDL_GetPointerProperty(getWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
}

int SDLC_KMSDRM_GetFd(SDL_Window* window)
{
    return (int)SDL_GetNumberProperty(getWindowProperties(window), SDL_PROP_WINDOW_KMSDRM_DRM_FD_NUMBER, -1);
}

int SDLC_KMSDRM_GetDevIndex(SDL_Window* window)
{
    return (int)SDL_GetNumberProperty(getWindowProperties(window), SDL_PROP_WINDOW_KMSDRM_DEVICE_INDEX_NUMBER, -1);
}

SDLC_VideoDriver SDLC_GetVideoDriver(void)
{
    const char* videoDriver = SDL_GetCurrentVideoDriver();
    if (!videoDriver) {
        return SDLC_VIDEO_UNKNOWN;
    }
    if (SDL_strcmp(videoDriver, "windows") == 0) {
        return SDLC_VIDEO_WIN32;
    }
    if (SDL_strcmp(videoDriver, "cocoa") == 0) {
        return SDLC_VIDEO_MACOS;
    }
    if (SDL_strcmp(videoDriver, "x11") == 0) {
        return SDLC_VIDEO_X11;
    }
    if (SDL_strcmp(videoDriver, "wayland") == 0) {
        return SDLC_VIDEO_WAYLAND;
    }
    if (SDL_strcmp(videoDriver, "kmsdrm") == 0) {
        return SDLC_VIDEO_KMSDRM;
    }
    return SDLC_VIDEO_UNKNOWN;
}

bool SDLC_IsFullscreen(SDL_Window* window)
{
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

bool SDLC_IsFullscreenExclusive(SDL_Window* window)
{
    return SDLC_IsFullscreen(window) && SDL_GetWindowFullscreenMode(window) != NULL;
}

bool SDLC_IsFullscreenDesktop(SDL_Window* window)
{
    return SDLC_IsFullscreen(window) && SDL_GetWindowFullscreenMode(window) == NULL;
}

void SDLC_EnterFullscreen(SDL_Window* window, bool exclusive)
{
    if (!exclusive) {
        SDL_SetWindowFullscreenMode(window, NULL);
    }
    SDL_SetWindowFullscreen(window, true);
}

void SDLC_LeaveFullscreen(SDL_Window* window)
{
    SDL_SetWindowFullscreen(window, false);
}

SDL_Window* SDLC_CreateWindowWithFallback(const char* title,
                                          int x, int y, int w, int h,
                                          SDL_WindowFlags requiredFlags,
                                          SDL_WindowFlags optionalFlags)
{
    SDL_Window* window = SDL_CreateWindow(title, w, h, requiredFlags | optionalFlags);
    if (!window) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to create window with optional flags: %s",
                    SDL_GetError());
        window = SDL_CreateWindow(title, w, h, requiredFlags);
    }

    if (window) {
        SDL_SetWindowPosition(window, x, y);
    }
    return window;
}

void SDLC_FlushWindowEvents(void)
{
    SDL_FlushEvents(SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST);
}

void SDLC_SetCursorVisible(bool visible)
{
    if (visible) {
        SDL_ShowCursor();
    }
    else {
        SDL_HideCursor();
    }
}

int SDLC_GetDisplayCount(void)
{
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    SDL_free(displays);
    return count;
}
