#include "waylandvsyncsource.h"

#include <SDL_syswm.h>

const struct wl_callback_listener WaylandVsyncSource::s_FrameListener = {
    .done = WaylandVsyncSource::frameDone,
};

WaylandVsyncSource::WaylandVsyncSource(Pacer* pacer)
    : m_Pacer(pacer),
      m_Display(nullptr),
      m_Surface(nullptr),
      m_Callback(nullptr),
      m_LastFrameTime(0)
{

}

WaylandVsyncSource::~WaylandVsyncSource()
{
    if (m_Callback != nullptr) {
        wl_callback_destroy(m_Callback);
        wl_display_roundtrip(m_Display);
    }
}

bool WaylandVsyncSource::initialize(SDL_Window* window, int displayFps)
{
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);

    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return false;
    }

    // Pacer shoud not create us for non-Wayland windows
    SDL_assert(info.subsystem == SDL_SYSWM_WAYLAND);

    m_DisplayFps = displayFps;
    m_Display = info.info.wl.display;
    m_Surface = info.info.wl.surface;

    // Enqueue our first frame callback
    m_Callback = wl_surface_frame(m_Surface);
    wl_callback_add_listener(m_Callback, &s_FrameListener, this);
    wl_surface_commit(m_Surface);

    return true;
}

void WaylandVsyncSource::frameDone(void* data, struct wl_callback* oldCb, uint32_t time)
{
    auto me = (WaylandVsyncSource*)data;

    // Free this callback
    SDL_assert(oldCb == me->m_Callback);
    wl_callback_destroy(oldCb);

    // Register for another callback before invoking Pacer to ensure we don't miss
    // a frame callback if Pacer takes too long.
    me->m_Callback = wl_surface_frame(me->m_Surface);
    wl_callback_add_listener(me->m_Callback, &s_FrameListener, data);
    wl_surface_commit(me->m_Surface);
    wl_display_flush(me->m_Display);

    if (me->m_LastFrameTime != 0) {
        // Assuming that the time until the next V-Sync will usually be equal to the
        // time from last callback to this one but cap it at 2x the expected frame period.
        me->m_Pacer->vsyncCallback(SDL_min(time - me->m_LastFrameTime, 2000 / me->m_DisplayFps));
    }

    me->m_LastFrameTime = time;
}
