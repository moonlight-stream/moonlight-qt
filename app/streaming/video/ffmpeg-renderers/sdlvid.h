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
    virtual void renderFrame(AVFrame* frame) override;
    virtual bool needsTestFrame() override;
    virtual int getDecoderCapabilities() override;
    virtual FramePacingConstraint getFramePacingConstraint() override;
    virtual void notifyOverlayUpdated(Overlay::OverlayType) override;

private:
    void renderOverlay(Overlay::OverlayType type);

    SDL_Renderer* m_Renderer;
    SDL_Texture* m_Texture;
    TTF_Font* m_OverlayFonts[Overlay::OverlayMax];
    SDL_Surface* m_OverlaySurfaces[Overlay::OverlayMax];
    SDL_Texture* m_OverlayTextures[Overlay::OverlayMax];
    SDL_Rect m_OverlayRects[Overlay::OverlayMax];
};

