#pragma once

#include "renderer.h"

// Avoid X11 if SDL was built without it
#ifndef SDL_VIDEO_DRIVER_X11
#warning Unable to use libva-x11 without SDL support
#undef HAVE_LIBVA_X11
#endif

// Avoid Wayland if SDL was built without it
#ifndef SDL_VIDEO_DRIVER_WAYLAND
#warning Unable to use libva-wayland without SDL support
#undef HAVE_LIBVA_WAYLAND
#endif

extern "C" {
#include <va/va.h>
#ifdef HAVE_LIBVA_X11
#include <va/va_x11.h>
#endif
#ifdef HAVE_LIBVA_WAYLAND
#include <va/va_wayland.h>
#endif
#ifdef HAVE_LIBVA_DRM
#include <va/va_drm.h>
#endif
#include <libavutil/hwcontext_vaapi.h>
#ifdef HAVE_EGL
#include <va/va_drmcommon.h>
#endif
}

#ifdef HAVE_EGL
#include <SDL_egl.h>

#ifndef EGL_VERSION_1_5
typedef intptr_t EGLAttrib;
typedef EGLImage (EGLAPIENTRYP PFNEGLCREATEIMAGEPROC) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEPROC) (EGLDisplay dpy, EGLImage image);
#endif

#ifndef EGL_KHR_image
// EGL_KHR_image technically uses EGLImageKHR instead of EGLImage, but they're compatible
// so we swap them here to avoid mixing them all over the place
typedef EGLImage (EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay dpy, EGLImage image);
#endif

#ifndef EGL_EXT_image_dma_buf_import
#define EGL_LINUX_DMA_BUF_EXT             0x3270
#define EGL_LINUX_DRM_FOURCC_EXT          0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT         0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT     0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT      0x3274
#endif

#ifndef EGL_EXT_image_dma_buf_import_modifiers
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT 0x3443
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT 0x3444
#endif
#endif

class VAAPIRenderer : public IFFmpegRenderer
{
public:
    VAAPIRenderer();
    virtual ~VAAPIRenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual bool needsTestFrame() override;
    virtual bool isDirectRenderingSupported() override;
    virtual int getDecoderColorspace() override;
#ifdef HAVE_EGL
    virtual bool canExportEGL() override;
    virtual bool initializeEGL(EGLDisplay dpy, const EGLExtensions &ext) override;
    virtual ssize_t exportEGLImages(AVFrame *frame, EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES]) override;
    virtual void freeEGLImages(EGLDisplay dpy, EGLImage[EGL_MAX_PLANES]) override;
#endif

private:
    VADisplay openDisplay(SDL_Window* window);

    int m_WindowSystem;
    AVBufferRef* m_HwContext;
    bool m_BlacklistedForDirectRendering;

#ifdef HAVE_LIBVA_X11
    Window m_XWindow;
#endif

    int m_VideoWidth;
    int m_VideoHeight;
    int m_DisplayWidth;
    int m_DisplayHeight;

#ifdef HAVE_EGL
    VADRMPRIMESurfaceDescriptor m_PrimeDescriptor;
    bool m_EGLExtDmaBuf;
    PFNEGLCREATEIMAGEPROC m_eglCreateImage;
    PFNEGLDESTROYIMAGEPROC m_eglDestroyImage;
    PFNEGLCREATEIMAGEKHRPROC m_eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC m_eglDestroyImageKHR;
#endif
};
