#pragma once

#include "renderer.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

class DrmRenderer : public IFFmpegRenderer {
public:
    DrmRenderer();
    virtual ~DrmRenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual enum AVPixelFormat getPreferredPixelFormat(int videoFormat) override;
    virtual int getRendererAttributes() override;
    virtual bool needsTestFrame() override;
    virtual bool isDirectRenderingSupported() override;
#ifdef HAVE_EGL
    virtual bool canExportEGL() override;
    virtual AVPixelFormat getEGLImagePixelFormat() override;
    virtual bool initializeEGL(EGLDisplay dpy, const EGLExtensions &ext) override;
    virtual ssize_t exportEGLImages(AVFrame *frame, EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES]) override;
    virtual void freeEGLImages(EGLDisplay dpy, EGLImage[EGL_MAX_PLANES]) override;
#endif

private:
    const char* getDrmColorEncodingValue(AVFrame* frame);
    const char* getDrmColorRangeValue(AVFrame* frame);

    AVBufferRef* m_HwContext;
    int m_DrmFd;
    bool m_SdlOwnsDrmFd;
    bool m_SupportsDirectRendering;
    uint32_t m_ConnectorId;
    uint32_t m_EncoderId;
    uint32_t m_CrtcId;
    uint32_t m_PlaneId;
    uint32_t m_CurrentFbId;
    AVColorRange m_LastColorRange;
    AVColorSpace m_LastColorSpace;
    drmModePropertyPtr m_ColorEncodingProp;
    drmModePropertyPtr m_ColorRangeProp;
    drmModePropertyPtr m_HdrOutputMetadataProp;
    SDL_Rect m_OutputRect;

#ifdef HAVE_EGL
    bool m_EGLExtDmaBuf;
    PFNEGLCREATEIMAGEPROC m_eglCreateImage;
    PFNEGLDESTROYIMAGEPROC m_eglDestroyImage;
    PFNEGLCREATEIMAGEKHRPROC m_eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC m_eglDestroyImageKHR;
#endif
};

