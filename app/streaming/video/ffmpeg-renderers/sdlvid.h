#pragma once

#include "renderer.h"

class SdlRenderer : public IFFmpegRenderer {
public:
    SdlRenderer();
    virtual ~SdlRenderer();
    virtual bool initialize(SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height,
                            int maxFps,
                            bool enableVsync);
    virtual bool prepareDecoderContext(AVCodecContext* context);
    virtual void renderFrameAtVsync(AVFrame* frame);
    virtual bool needsTestFrame();
    virtual int getDecoderCapabilities();
    virtual FramePacingConstraint getFramePacingConstraint();

private:
    SDL_Renderer* m_Renderer;
    SDL_Texture* m_Texture;
};

