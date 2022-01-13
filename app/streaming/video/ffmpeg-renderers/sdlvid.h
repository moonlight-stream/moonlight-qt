#pragma once

#include "renderer.h"

#ifdef HAVE_CUDA
#include "cuda.h"
#endif

class SdlRenderer : public IFFmpegRenderer {
public:
    SdlRenderer();
    virtual ~SdlRenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual bool isRenderThreadSupported() override;
    virtual bool isPixelFormatSupported(int videoFormat, enum AVPixelFormat pixelFormat) override;
    virtual bool testRenderFrame(AVFrame* frame) override;

private:
    void renderOverlay(Overlay::OverlayType type);
    bool initializeReadBackFormat(AVBufferRef* hwFrameCtxRef, AVFrame* testFrame);
    AVFrame* getSwFrameFromHwFrame(AVFrame* hwFrame);

    int m_VideoFormat;
    SDL_Renderer* m_Renderer;
    SDL_Texture* m_Texture;
    enum AVPixelFormat m_SwPixelFormat;
    enum AVColorSpace m_ColorSpace;
    bool m_MapFrame;
    SDL_Texture* m_OverlayTextures[Overlay::OverlayMax];
    SDL_Rect m_OverlayRects[Overlay::OverlayMax];

#ifdef HAVE_CUDA
    CUDAGLInteropHelper* m_CudaGLHelper;
#endif
};

