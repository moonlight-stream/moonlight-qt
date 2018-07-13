#pragma once

#include <SDL.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

class IRenderer {
public:
    virtual bool initialize(SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height) = 0;
    virtual bool prepareDecoderContext(AVCodecContext* context) = 0;
    virtual void renderFrame(AVFrame* frame) = 0;
};

class SdlRenderer : public IRenderer {
public:
    SdlRenderer();
    virtual ~SdlRenderer();
    virtual bool initialize(SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height);
    virtual bool prepareDecoderContext(AVCodecContext* context);
    virtual void renderFrame(AVFrame* frame);

private:
    SDL_Renderer* m_Renderer;
    SDL_Texture* m_Texture;
};
