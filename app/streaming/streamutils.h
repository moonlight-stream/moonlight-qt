#pragma once

#if HAVE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

// SDL_FRect wasn't added until 2.0.10
#if !SDL_VERSION_ATLEAST(2, 0, 10)
typedef struct SDL_FRect
{
    float x;
    float y;
    float w;
    float h;
} SDL_FRect;
#endif

class StreamUtils
{
public:
    static
    Uint32 getPlatformWindowFlags();

    static
    void scaleSourceToDestinationSurface(SDL_Rect* src, SDL_Rect* dst);

    static
    void screenSpaceToNormalizedDeviceCoords(SDL_FRect* rect, int viewportWidth, int viewportHeight);

    static
    void screenSpaceToNormalizedDeviceCoords(SDL_Rect* src, SDL_FRect* dst, int viewportWidth, int viewportHeight);

    static
#if SDL_VERSION_ATLEAST(3, 0, 0)
    bool getNativeDesktopMode(SDL_DisplayID displayID, SDL_DisplayMode* mode);
#else
    bool getNativeDesktopMode(int displayIndex, SDL_DisplayMode* mode);
#endif

    static
    int getDisplayRefreshRate(SDL_Window* window);

    static
    bool hasFastAes();
};
