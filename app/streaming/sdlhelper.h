#pragma once

#include <SDL.h>

class SDLHelper
{
public:
    static void waitEvent(SDL_Event* event);
    static void pushEvent(SDL_Event* event);

private:
    static void initializeIfNeeded();

    static SDL_cond* s_EventCond;
    static SDL_mutex* s_EventMutex;
};
