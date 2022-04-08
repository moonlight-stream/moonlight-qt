#pragma once

#include <SDL.h>

#include "streaming/video/decoder.h"
#include "streaming/video/overlaymanager.h"

extern "C" {
#include <libavcodec/avcodec.h>

#ifdef HAVE_DRM
#include <libavutil/hwcontext_drm.h>
#endif
}

#ifdef HAVE_EGL
#define MESA_EGL_NO_X11_HEADERS
#define EGL_NO_X11
#include <SDL_egl.h>

#ifndef EGL_VERSION_1_5
typedef intptr_t EGLAttrib;
typedef void *EGLImage;
#endif

#if !defined(EGL_VERSION_1_5) || !defined(EGL_EGL_PROTOTYPES)
typedef EGLImage (EGLAPIENTRYP PFNEGLCREATEIMAGEPROC) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEPROC) (EGLDisplay dpy, EGLImage image);
typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYPROC) (EGLenum platform, void *native_display, const EGLAttrib *attrib_list);
#endif

#if !defined(EGL_KHR_image) || !defined(EGL_EGLEXT_PROTOTYPES)
// EGL_KHR_image technically uses EGLImageKHR instead of EGLImage, but they're compatible
// so we swap them here to avoid mixing them all over the place
typedef EGLImage (EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay dpy, EGLImage image);
#endif

#if !defined(EGL_EXT_platform_base) || !defined(EGL_EGLEXT_PROTOTYPES)
typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXTPROC) (EGLenum platform, void *native_display, const EGLint *attrib_list);
#endif

#ifndef EGL_EXT_image_dma_buf_import
#define EGL_LINUX_DMA_BUF_EXT             0x3270
#define EGL_LINUX_DRM_FOURCC_EXT          0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT         0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT     0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT      0x3274
#define EGL_DMA_BUF_PLANE1_FD_EXT         0x3275
#define EGL_DMA_BUF_PLANE1_OFFSET_EXT     0x3276
#define EGL_DMA_BUF_PLANE1_PITCH_EXT      0x3277
#define EGL_DMA_BUF_PLANE2_FD_EXT         0x3278
#define EGL_DMA_BUF_PLANE2_OFFSET_EXT     0x3279
#define EGL_DMA_BUF_PLANE2_PITCH_EXT      0x327A
#define EGL_YUV_COLOR_SPACE_HINT_EXT      0x327B
#define EGL_SAMPLE_RANGE_HINT_EXT         0x327C
#define EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT 0x327D
#define EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT 0x327E
#define EGL_ITU_REC601_EXT                0x327F
#define EGL_ITU_REC709_EXT                0x3280
#define EGL_ITU_REC2020_EXT               0x3281
#define EGL_YUV_FULL_RANGE_EXT            0x3282
#define EGL_YUV_NARROW_RANGE_EXT          0x3283
#define EGL_YUV_CHROMA_SITING_0_EXT       0x3284
#define EGL_YUV_CHROMA_SITING_0_5_EXT     0x3285
#endif

#ifndef EGL_EXT_image_dma_buf_import_modifiers
#define EGL_DMA_BUF_PLANE3_FD_EXT         0x3440
#define EGL_DMA_BUF_PLANE3_OFFSET_EXT     0x3441
#define EGL_DMA_BUF_PLANE3_PITCH_EXT      0x3442
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT 0x3443
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT 0x3444
#define EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT 0x3445
#define EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT 0x3446
#define EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT 0x3447
#define EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT 0x3448
#define EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT 0x3449
#define EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT 0x344A
#endif

#define EGL_MAX_PLANES 4

class EGLExtensions {
public:
    EGLExtensions(EGLDisplay dpy);
    ~EGLExtensions() {}
    bool isSupported(const QString &extension) const;
private:
    const QStringList m_Extensions;
};

#endif

#define RENDERER_ATTRIBUTE_FULLSCREEN_ONLY 0x01
#define RENDERER_ATTRIBUTE_1080P_MAX 0x02
#define RENDERER_ATTRIBUTE_HDR_SUPPORT 0x04

class IFFmpegRenderer : public Overlay::IOverlayRenderer {
public:
    virtual bool initialize(PDECODER_PARAMETERS params) = 0;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) = 0;
    virtual void renderFrame(AVFrame* frame) = 0;

    // Called for threaded renderers to allow them to wait prior to us latching
    // the next frame for rendering (as opposed to waiting on buffer swap with
    // an older frame already queued for display).
    virtual void waitToRender() {
        // Don't wait by default
    }

    // Called on the same thread as renderFrame() during destruction of the renderer
    virtual void cleanupRenderContext() {
        // Nothing
    }

    virtual bool testRenderFrame(AVFrame*) {
        // If the renderer doesn't provide an explicit test routine,
        // we will always assume that any returned AVFrame can be
        // rendered successfully.
        return true;
    }

    virtual bool needsTestFrame() {
        // No test frame required by default
        return false;
    }

    virtual int getDecoderCapabilities() {
        // No special capabilities by default
        return 0;
    }

    virtual int getRendererAttributes() {
        // No special attributes by default
        return 0;
    }

    virtual int getDecoderColorspace() {
        // Rec 601 is default
        return COLORSPACE_REC_601;
    }

    virtual bool isRenderThreadSupported() {
        // Render thread is supported by default
        return true;
    }

    virtual bool isDirectRenderingSupported() {
        // The renderer can render directly to the display
        return true;
    }

    virtual AVPixelFormat getPreferredPixelFormat(int videoFormat) {
        if (videoFormat == VIDEO_FORMAT_H265_MAIN10) {
            // 10-bit YUV 4:2:0
            return AV_PIX_FMT_P010;
        }
        else {
            // Planar YUV 4:2:0
            return AV_PIX_FMT_YUV420P;
        }
    }

    virtual bool isPixelFormatSupported(int videoFormat, AVPixelFormat pixelFormat) {
        // By default, we only support the preferred pixel format
        return getPreferredPixelFormat(videoFormat) == pixelFormat;
    }

    virtual void setHdrMode(bool) {
        // Nothing
    }

    virtual bool prepareDecoderContextInGetFormat(AVCodecContext*, AVPixelFormat) {
        // Assume no further initialization is required
        return true;
    }

    // IOverlayRenderer
    virtual void notifyOverlayUpdated(Overlay::OverlayType) override {
        // Nothing
    }

#ifdef HAVE_EGL
    // By default we can't do EGL
    virtual bool canExportEGL() {
        return false;
    }

    virtual AVPixelFormat getEGLImagePixelFormat() {
        return AV_PIX_FMT_NONE;
    }

    virtual bool initializeEGL(EGLDisplay,
                               const EGLExtensions &) {
        return false;
    }

    virtual ssize_t exportEGLImages(AVFrame *,
                                    EGLDisplay,
                                    EGLImage[EGL_MAX_PLANES]) {
        return -1;
    }

    // Free the ressources allocated during the last `exportEGLImages` call
    virtual void freeEGLImages(EGLDisplay, EGLImage[EGL_MAX_PLANES]) {}
#endif

#ifdef HAVE_DRM
    // By default we can't do DRM PRIME export
    virtual bool canExportDrmPrime() {
        return false;
    }

    virtual bool mapDrmPrimeFrame(AVFrame*, AVDRMFrameDescriptor*) {
        return false;
    }

    virtual void unmapDrmPrimeFrame(AVDRMFrameDescriptor*) {}
#endif
};
