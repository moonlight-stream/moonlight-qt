#include "dxvsyncsource.h"

#include <dwmapi.h>

DxVsyncSource::DxVsyncSource(Pacer* pacer) :
    m_Pacer(pacer),
    m_Thread(nullptr)
{
    SDL_AtomicSet(&m_Stopping, 0);
}

DxVsyncSource::~DxVsyncSource()
{
    if (m_Thread != nullptr) {
        SDL_AtomicSet(&m_Stopping, 1);
        SDL_WaitThread(m_Thread, nullptr);
    }
}

bool DxVsyncSource::initialize(SDL_Window* window)
{
    m_Thread = SDL_CreateThread(vsyncThread, "DX Vsync Thread", this);
    if (m_Thread == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to create DX V-sync thread: %s",
                     SDL_GetError());
        return false;
    }

    return true;
}

int DxVsyncSource::vsyncThread(void* context)
{
    DxVsyncSource* me = reinterpret_cast<DxVsyncSource*>(context);

    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

    while (SDL_AtomicGet(&me->m_Stopping) == 0) {
        // FIXME: We should really use D3DKMTWaitForVerticalBlankEvent() instead!
        // https://bugs.chromium.org/p/chromium/issues/detail?id=467617
        // https://chromium.googlesource.com/chromium/src.git/+/c564f2fe339b2b2abb0c8773c90c83215670ea71/gpu/ipc/service/gpu_vsync_provider_win.cc
        DwmFlush();

        me->m_Pacer->vsyncCallback();
    }

    return 0;
}
