#pragma once

#include <SDL.h>

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
    bool getRealDesktopMode(int displayIndex, SDL_DisplayMode* mode);

    static
    int getDisplayRefreshRate(SDL_Window* window);
};
