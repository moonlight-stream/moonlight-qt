#pragma once

#include "renderer.h"

#ifdef HAVE_LIBVA
#include <va/va_drmcommon.h>
#endif

#include <optional>

class EglImageFactory
{
    class EglImageContext {
    public:
        EglImageContext(EGLDisplay display, PFNEGLDESTROYIMAGEPROC fn_eglDestroyImage, PFNEGLDESTROYIMAGEKHRPROC fn_eglDestroyImageKHR) :
            m_Display(display),
            m_eglDestroyImage(fn_eglDestroyImage),
            m_eglDestroyImageKHR(fn_eglDestroyImageKHR) {}

        EglImageContext(const EglImageContext& other) = delete;

        EglImageContext(EglImageContext&& other) {
            // Copy the basic EGL state
            m_Display = other.m_Display;
            m_eglDestroyImage = other.m_eglDestroyImage;
            m_eglDestroyImageKHR = other.m_eglDestroyImageKHR;

            // Transfer ownership of the EGLImages
            memcpy(images, other.images, other.count * sizeof(EGLImage));
            count = other.count;
            other.count = 0;
        }

        ~EglImageContext() {
            for (ssize_t i = 0; i < count; ++i) {
                if (m_eglDestroyImage) {
                    m_eglDestroyImage(m_Display, images[i]);
                }
                else {
                    m_eglDestroyImageKHR(m_Display, images[i]);
                }
            }
        }

        EGLImage images[EGL_MAX_PLANES] {};
        ssize_t count = 0;

    private:
        EGLDisplay m_Display;
        PFNEGLDESTROYIMAGEPROC m_eglDestroyImage;
        PFNEGLDESTROYIMAGEKHRPROC m_eglDestroyImageKHR;
    };

public:
    EglImageFactory(IFFmpegRenderer* renderer);
    bool initializeEGL(EGLDisplay, const EGLExtensions &ext);
    void resetCache();

#ifdef HAVE_DRM
    ssize_t exportDRMImages(AVFrame* frame, EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES]);
#endif

#ifdef HAVE_LIBVA
    ssize_t exportVAImages(AVFrame* frame, uint32_t exportFlags, EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES]);
#endif

    void freeEGLImages();

    bool supportsImportingFormat(EGLDisplay dpy, EGLint format);
    bool supportsImportingModifier(EGLDisplay dpy, EGLint format, EGLuint64KHR modifier);

private:
    IFFmpegRenderer* m_Renderer;
    std::optional<EglImageContext> m_LastImageCtx;
    bool m_EGLExtDmaBuf;
    PFNEGLCREATEIMAGEPROC m_eglCreateImage;
    PFNEGLDESTROYIMAGEPROC m_eglDestroyImage;
    PFNEGLCREATEIMAGEKHRPROC m_eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC m_eglDestroyImageKHR;
    PFNEGLQUERYDMABUFFORMATSEXTPROC m_eglQueryDmaBufFormatsEXT;
    PFNEGLQUERYDMABUFMODIFIERSEXTPROC m_eglQueryDmaBufModifiersEXT;
};
