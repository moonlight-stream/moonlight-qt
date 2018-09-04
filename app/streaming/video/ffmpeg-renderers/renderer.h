#pragma once

#include <SDL.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

class IFFmpegRenderer {
public:
    enum VSyncConstraint {
        VSYNC_FORCE_OFF,
        VSYNC_FORCE_ON,
        VSYNC_ANY
    };

    virtual ~IFFmpegRenderer() {}
    virtual bool initialize(SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height,
                            int maxFps,
                            bool enableVsync) = 0;
    virtual bool prepareDecoderContext(AVCodecContext* context) = 0;
    virtual void renderFrameAtVsync(AVFrame* frame) = 0;
    virtual bool needsTestFrame() = 0;
    virtual int getDecoderCapabilities() = 0;
    virtual VSyncConstraint getVsyncConstraint() = 0;
};

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
    virtual VSyncConstraint getVsyncConstraint();

private:
    SDL_Renderer* m_Renderer;
    SDL_Texture* m_Texture;
};
