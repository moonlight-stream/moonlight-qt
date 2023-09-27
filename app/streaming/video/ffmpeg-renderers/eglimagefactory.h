#pragma once

#include "renderer.h"

#define SDL_USE_BUILTIN_OPENGL_DEFINITIONS 1
#include <SDL_egl.h>

#ifdef HAVE_LIBVA
#include <va/va_drmcommon.h>
#endif

class EglImageFactory
{
public:
    EglImageFactory(IFFmpegRenderer* renderer);
    bool initializeEGL(EGLDisplay, const EGLExtensions &ext);
    ssize_t exportDRMImages(AVFrame* frame, AVDRMFrameDescriptor* drmFrame, EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES]);

#ifdef HAVE_LIBVA
    ssize_t exportVAImages(AVFrame* frame, VADRMPRIMESurfaceDescriptor* vaFrame, EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES]);
#endif

    void freeEGLImages(EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES]);

private:
    IFFmpegRenderer* m_Renderer;
    bool m_EGLExtDmaBuf;
    PFNEGLCREATEIMAGEPROC m_eglCreateImage;
    PFNEGLDESTROYIMAGEPROC m_eglDestroyImage;
    PFNEGLCREATEIMAGEKHRPROC m_eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC m_eglDestroyImageKHR;
};
