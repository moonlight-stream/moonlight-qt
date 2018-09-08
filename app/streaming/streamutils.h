#pragma once

#include <SDL.h>

class StreamUtils
{
public:
    static
    void scaleSourceToDestinationSurface(SDL_Rect* src, SDL_Rect* dst);

    static
    bool getRealDesktopMode(int displayIndex, SDL_DisplayMode* mode);

    static
    int getDisplayRefreshRate(SDL_Window* window);
};
