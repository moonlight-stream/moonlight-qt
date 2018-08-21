#include "nullthreadedvsyncsource.h"

NullThreadedVsyncSource::NullThreadedVsyncSource(Pacer* pacer) :
    m_Pacer(pacer),
    m_Thread(nullptr)
{
    SDL_AtomicSet(&m_Stopping, 0);
}

NullThreadedVsyncSource::~NullThreadedVsyncSource()
{
    if (m_Thread != nullptr) {
        SDL_AtomicSet(&m_Stopping, 1);
        SDL_WaitThread(m_Thread, nullptr);
    }
}

bool NullThreadedVsyncSource::initialize(SDL_Window*, int displayFps)
{
    m_DisplayFps = displayFps;
    m_Thread = SDL_CreateThread(vsyncThread, "Null Vsync Thread", this);
    if (m_Thread == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to create DX V-sync thread: %s",
                     SDL_GetError());
        return false;
    }

    return true;
}

int NullThreadedVsyncSource::vsyncThread(void* context)
{
    NullThreadedVsyncSource* me = reinterpret_cast<NullThreadedVsyncSource*>(context);

    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

    while (SDL_AtomicGet(&me->m_Stopping) == 0) {
        me->m_Pacer->vsyncCallback(1000 / me->m_DisplayFps);
    }

    return 0;
}
