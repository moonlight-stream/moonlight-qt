#pragma once

#include "renderer.h"
#include "swframemapper.h"

#ifdef HAVE_CUDA
#include "cuda.h"
#endif

class SdlRenderer : public IFFmpegRenderer {
public:
    SdlRenderer();
    virtual ~SdlRenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) override;
    virtual void prepareToRender() override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual bool isRenderThreadSupported() override;
    virtual bool isPixelFormatSupported(int videoFormat, enum AVPixelFormat pixelFormat) override;
    virtual bool testRenderFrame(AVFrame* frame) override;
    virtual bool notifyWindowChanged(PWINDOW_STATE_CHANGE_INFO) override;

private:
    void renderOverlay(Overlay::OverlayType type);

    int m_VideoFormat;
    SDL_Renderer* m_Renderer;
    SDL_Texture* m_Texture;
    int m_ColorSpace;
    SDL_Texture* m_OverlayTextures[Overlay::OverlayMax];
    SDL_Rect m_OverlayRects[Overlay::OverlayMax];

    SwFrameMapper m_SwFrameMapper;

#ifdef HAVE_CUDA
    CUDAGLInteropHelper* m_CudaGLHelper;
#endif
};

