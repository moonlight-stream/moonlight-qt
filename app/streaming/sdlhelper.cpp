#include "sdlhelper.h"

SDL_cond* SDLHelper::s_EventCond;
SDL_mutex* SDLHelper::s_EventMutex;

void SDLHelper::initializeIfNeeded()
{
    if (!s_EventCond) {
        s_EventCond = SDL_CreateCond();
    }
    if (!s_EventMutex) {
        s_EventMutex = SDL_CreateMutex();
    }
}

void SDLHelper::waitEvent(SDL_Event* event)
{
    initializeIfNeeded();

    // We explicitly use SDL_PollEvent() and SDL_Delay() because
    // SDL_WaitEventTimeout() has an internal SDL_Delay(10) inside which
    // blocks this thread too long for high polling rate mice and high
    // refresh rate displays.
    while (!SDL_PollEvent(event)) {
        // Sleep for up to 1 ms or until we are woken up by
        // SDLHelper::pushEvent().
        // FIXME: SDL should do this internally
        SDL_LockMutex(s_EventMutex);
        SDL_CondWaitTimeout(s_EventCond, s_EventMutex, 1);
        SDL_UnlockMutex(s_EventMutex);
        continue;
    }
}

void SDLHelper::pushEvent(SDL_Event* event)
{
    initializeIfNeeded();

    SDL_PushEvent(event);

    // Immediately wake up the other waiting thread
    SDL_LockMutex(s_EventMutex);
    SDL_CondSignal(s_EventCond);
    SDL_UnlockMutex(s_EventMutex);
}
