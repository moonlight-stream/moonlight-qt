#pragma once

#include "SDL_compat.h"

class StreamUtils
{
public:
    static
    Uint32 getPlatformWindowFlags();

    static
    SDL_Window* createTestWindow();

    static
    void scaleSourceToDestinationSurface(SDL_Rect* src, SDL_Rect* dst);

    // Fit-width-pan-Y mode: viewer-only zoom that fills window width and pans
    // vertically based on local cursor position. Set by mouse motion handler;
    // read every frame by scaleSourceToDestinationSurface when the pref is on.
    static
    int getFitWidthPanYOffset();

    static
    void setFitWidthPanYOffset(int offset);

    // Returns true and writes the dst rect for fit-width mode if the pref is on
    // and the stream is taller than the window aspect (i.e. would otherwise
    // letterbox left/right). Used by mouse.cpp to detect when the special path
    // is active and clamp confinement to the window rather than the offscreen rect.
    static
    bool isFitWidthPanYActive(const SDL_Rect* src, const SDL_Rect* dst);

    static
    void screenSpaceToNormalizedDeviceCoords(SDL_FRect* rect, int viewportWidth, int viewportHeight);

    static
    void screenSpaceToNormalizedDeviceCoords(SDL_Rect* src, SDL_FRect* dst, int viewportWidth, int viewportHeight);

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
