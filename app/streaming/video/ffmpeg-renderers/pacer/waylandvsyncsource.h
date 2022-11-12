#pragma once

#include "pacer.h"

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

class WaylandVsyncSource : public IVsyncSource
{
public:
    WaylandVsyncSource(Pacer* pacer);

    virtual ~WaylandVsyncSource();

    virtual bool initialize(SDL_Window* window, int displayFps);

private:
    static void frameDone(void* data, struct wl_callback* oldCb, uint32_t time);

    static const struct wl_callback_listener s_FrameListener;

    Pacer* m_Pacer;
    int m_DisplayFps;
    wl_display* m_Display;
    wl_surface* m_Surface;
    wl_callback* m_Callback;
    uint32_t m_LastFrameTime;
};

