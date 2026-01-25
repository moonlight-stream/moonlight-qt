#pragma once

#include "SDL_compat.h"

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
    void getVideoRegionInWindow(int streamWidth, int streamHeight, int windowWidth, int windowHeight, SDL_Rect* dst);

    static
    bool windowPointToNormalizedVideo(int streamWidth, int streamHeight, int windowWidth, int windowHeight,
                                      float windowX, float windowY, float* outX, float* outY);

    static
    bool getNativeDesktopMode(int displayIndex, SDL_DisplayMode* mode, SDL_Rect* safeArea);

    static
    int getDisplayRefreshRate(SDL_Window* window);

    static
    bool hasFastAes();

    static
    int getDrmFdForWindow(SDL_Window* window, bool* needsClose);

    static
    int getDrmFd(bool preferRenderNode);

    static
    void enterAsyncLoggingMode();

    static
    void exitAsyncLoggingMode();
};
