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

private:
    int m_DrmFd;
    uint32_t m_CrtcId;
    uint32_t m_PlaneId;
    uint32_t m_CurrentFbId;
    SDL_Rect m_OutputRect;
};

