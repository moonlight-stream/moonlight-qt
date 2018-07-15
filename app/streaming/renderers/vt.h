#pragma once

#include "renderer.h"

class VTRenderer : public IRenderer
{
public:
    VTRenderer();
    virtual ~VTRenderer();
    virtual bool initialize(SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height);
    virtual bool prepareDecoderContext(AVCodecContext* context);
    virtual void renderFrame(AVFrame* frame);

private:
};
