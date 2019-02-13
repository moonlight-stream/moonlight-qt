#pragma once

#include "renderer.h"

#include <SDL_ttf.h>

class SdlRenderer : public IFFmpegRenderer {
public:
    SdlRenderer();
    virtual ~SdlRenderer() override;
    virtual bool initialize(SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height,
                            int maxFps,
                            bool enableVsync) override;
    virtual bool prepareDecoderContext(AVCodecContext* context) override;
    virtual void renderFrameAtVsync(AVFrame* frame) override;
    virtual bool needsTestFrame() override;
    virtual int getDecoderCapabilities() override;
    virtual FramePacingConstraint getFramePacingConstraint() override;
    virtual void notifyOverlayUpdated(Overlay::OverlayType) override;

private:
    SDL_Renderer* m_Renderer;
    SDL_Texture* m_Texture;
    TTF_Font* m_DebugOverlayFont;
    SDL_Surface* m_DebugOverlaySurface;
    SDL_Texture* m_DebugOverlayTexture;
    SDL_Rect m_DebugOverlayRect;
};

