#pragma once

#include "renderer.h"

extern "C" {
#include <va/va.h>
#include <va/va_x11.h>
#include <libavutil/hwcontext_vaapi.h>
}

class VAAPIRenderer : public IFFmpegRenderer
{
    typedef VAStatus (*vaPutSurface_t)(
        VADisplay dpy,
        VASurfaceID surface,
        Drawable draw, /* X Drawable */
        short srcx,
        short srcy,
        unsigned short srcw,
        unsigned short srch,
        short destx,
        short desty,
        unsigned short destw,
        unsigned short desth,
        VARectangle *cliprects,
        unsigned int number_cliprects,
        unsigned int flags
    );

public:
    VAAPIRenderer();
    virtual ~VAAPIRenderer();
    virtual bool initialize(SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height);
    virtual bool prepareDecoderContext(AVCodecContext* context);
    virtual void renderFrame(AVFrame* frame);

private:
    Window m_XWindow;
    AVBufferRef* m_HwContext;
    void* m_X11VaLibHandle;
    vaPutSurface_t m_vaPutSurface;
    int m_VideoWidth;
    int m_VideoHeight;
    int m_DisplayWidth;
    int m_DisplayHeight;
};
