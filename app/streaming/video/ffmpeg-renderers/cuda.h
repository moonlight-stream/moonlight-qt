#pragma once

#include "renderer.h"

class CUDARenderer : public IFFmpegRenderer {
public:
    CUDARenderer();
    virtual ~CUDARenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS) override;
    virtual bool prepareDecoderContext(AVCodecContext* context) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual bool needsTestFrame() override;
    virtual bool isDirectRenderingSupported() override;

private:
    AVBufferRef* m_HwContext;
};

