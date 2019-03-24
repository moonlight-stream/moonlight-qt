#include "sdlvid.h"

#include "streaming/session.h"
#include "path.h"

#include <QDir>

#include <Limelight.h>

SdlRenderer::SdlRenderer()
    : m_Renderer(nullptr),
      m_Texture(nullptr)
{
    SDL_assert(TTF_WasInit() == 0);
    if (TTF_Init() != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "TTF_Init() failed: %s",
                    TTF_GetError());
        return;
    }

    SDL_zero(m_OverlayFonts);
    SDL_zero(m_OverlaySurfaces);
    SDL_zero(m_OverlayTextures);
}

SdlRenderer::~SdlRenderer()
{
    for (int i = 0; i < Overlay::OverlayMax; i++) {
        if (m_OverlayFonts[i] != nullptr) {
            TTF_CloseFont(m_OverlayFonts[i]);
        }
    }

    TTF_Quit();
    SDL_assert(TTF_WasInit() == 0);

    for (int i = 0; i < Overlay::OverlayMax; i++) {
        if (m_OverlayTextures[i] != nullptr) {
            SDL_DestroyTexture(m_OverlayTextures[i]);
        }
    }

    for (int i = 0; i < Overlay::OverlayMax; i++) {
        if (m_OverlaySurfaces[i] != nullptr) {
            SDL_FreeSurface(m_OverlaySurfaces[i]);
        }
    }

    if (m_Texture != nullptr) {
        SDL_DestroyTexture(m_Texture);
    }

    if (m_Renderer != nullptr) {
        SDL_DestroyRenderer(m_Renderer);
    }
}

bool SdlRenderer::prepareDecoderContext(AVCodecContext*)
{
    /* Nothing to do */

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using SDL software renderer");

    return true;
}

bool SdlRenderer::needsTestFrame()
{
    // This renderer should always work
    return false;
}

int SdlRenderer::getDecoderCapabilities()
{
    // The FFmpeg CPU decoder can handle reference frame invalidation,
    // but only for H.264.
    return CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC;
}

IFFmpegRenderer::FramePacingConstraint SdlRenderer::getFramePacingConstraint()
{
    return PACING_ANY;
}

void SdlRenderer::notifyOverlayUpdated(Overlay::OverlayType type)
{
    // Construct the required font to render the overlay
    if (m_OverlayFonts[type] == nullptr) {
        QByteArray fontData = Path::readDataFile("ModeSeven.ttf");
        if (fontData.isEmpty()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to load SDL overlay font");
            return;
        }

        m_OverlayFonts[type] = TTF_OpenFontRW(SDL_RWFromConstMem(fontData.constData(), fontData.size()),
                                              1,
                                              Session::get()->getOverlayManager().getOverlayFontSize(type));
        if (m_OverlayFonts[type] == nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "TTF_OpenFont() failed: %s",
                        TTF_GetError());

            // Can't proceed without a font
            return;
        }
    }

    SDL_Surface* oldSurface = (SDL_Surface*)SDL_AtomicGetPtr((void**)&m_OverlaySurfaces[type]);

    // Free the old surface
    if (oldSurface != nullptr && SDL_AtomicCASPtr((void**)&m_OverlaySurfaces[type], oldSurface, nullptr)) {
        SDL_FreeSurface(oldSurface);
    }

    if (Session::get()->getOverlayManager().isOverlayEnabled(type)) {
        // The _Wrapped variant is required for line breaks to work
        SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(m_OverlayFonts[type],
                                                              Session::get()->getOverlayManager().getOverlayText(type),
                                                              Session::get()->getOverlayManager().getOverlayColor(type),
                                                              1000);
        SDL_AtomicSetPtr((void**)&m_OverlaySurfaces[type], surface);
    }
}

bool SdlRenderer::initialize(SDL_Window* window,
                             int,
                             int width,
                             int height,
                             int,
                             bool enableVsync)
{
    Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;

    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
        // In full-screen exclusive mode, we enable V-sync if requested. For other modes, Windows and Mac
        // have compositors that make rendering tear-free. Linux compositor varies by distro and user
        // configuration but doesn't seem feasible to detect here.
        if (enableVsync) {
            rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
        }
    }

    m_Renderer = SDL_CreateRenderer(window, -1, rendererFlags);
    if (!m_Renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
        return false;
    }

    // The window may be smaller than the stream size, so ensure our
    // logical rendering surface size is equal to the stream size
    SDL_RenderSetLogicalSize(m_Renderer, width, height);

    // Draw a black frame until the video stream starts rendering
    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(m_Renderer);
    SDL_RenderPresent(m_Renderer);

    m_Texture = SDL_CreateTexture(m_Renderer,
                                  SDL_PIXELFORMAT_YV12,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  width,
                                  height);
    if (!m_Texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
        return false;
    }

#ifdef Q_OS_WIN32
    // For some reason, using Direct3D9Ex breaks this with multi-monitor setups.
    // When focus is lost, the window is minimized then immediately restored without
    // input focus. This glitches out the renderer and a bunch of other stuff.
    // Direct3D9Ex itself seems to have this minimize on focus loss behavior on its
    // own, so just disable SDL's handling of the focus loss event.
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0", SDL_HINT_OVERRIDE);
#endif

    return true;
}

void SdlRenderer::renderOverlay(Overlay::OverlayType type)
{
    if (Session::get()->getOverlayManager().isOverlayEnabled(type)) {
        // If a new surface has been created for updated overlay data, convert it into a texture.
        // NB: We have to do this conversion at render-time because we can only interact
        // with the renderer on a single thread.
        SDL_Surface* surface = (SDL_Surface*)SDL_AtomicGetPtr((void**)&m_OverlaySurfaces[type]);
        if (surface != nullptr && SDL_AtomicCASPtr((void**)&m_OverlaySurfaces[type], surface, nullptr)) {
            if (m_OverlayTextures[type] != nullptr) {
                SDL_DestroyTexture(m_OverlayTextures[type]);
            }

            if (type == Overlay::OverlayStatusUpdate) {
                // Bottom Left
                int unused;
                SDL_RenderGetLogicalSize(m_Renderer, &unused, &m_OverlayRects[type].y);
                m_OverlayRects[type].x = 0;
                m_OverlayRects[type].y -= surface->h;
            }
            else if (type == Overlay::OverlayDebug) {
                // Top left
                m_OverlayRects[type].x = 0;
                m_OverlayRects[type].y = 0;
            }

            m_OverlayRects[type].w = surface->w;
            m_OverlayRects[type].h = surface->h;

            m_OverlayTextures[type] = SDL_CreateTextureFromSurface(m_Renderer, surface);
            SDL_FreeSurface(surface);
        }

        // If we have an overlay texture, render it too
        if (m_OverlayTextures[type] != nullptr) {
            SDL_RenderCopy(m_Renderer, m_OverlayTextures[type], nullptr, &m_OverlayRects[type]);
        }
    }
}

void SdlRenderer::renderFrame(AVFrame* frame)
{
    SDL_UpdateYUVTexture(m_Texture, nullptr,
                         frame->data[0],
                         frame->linesize[0],
                         frame->data[1],
                         frame->linesize[1],
                         frame->data[2],
                         frame->linesize[2]);

    SDL_RenderClear(m_Renderer);

    // Draw the video content itself
    SDL_RenderCopy(m_Renderer, m_Texture, nullptr, nullptr);

    // Draw the overlays
    for (int i = 0; i < Overlay::OverlayMax; i++) {
        renderOverlay((Overlay::OverlayType)i);
    }

    SDL_RenderPresent(m_Renderer);
}
