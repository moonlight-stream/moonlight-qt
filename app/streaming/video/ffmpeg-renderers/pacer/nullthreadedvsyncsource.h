#pragma once

#include "pacer.h"

class NullThreadedVsyncSource : public IVsyncSource
{
public:
    NullThreadedVsyncSource(Pacer* pacer);

    virtual ~NullThreadedVsyncSource();

    virtual bool initialize(SDL_Window* window, int displayFps);

private:
    static int vsyncThread(void* context);

    Pacer* m_Pacer;
    SDL_Thread* m_Thread;
    SDL_atomic_t m_Stopping;
    int m_DisplayFps;
};
