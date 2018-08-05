#pragma once

#include <SDL.h>

class StreamUtils
{
public:
    static
    void scaleSourceToDestinationSurface(SDL_Rect* src, SDL_Rect* dst);
};
