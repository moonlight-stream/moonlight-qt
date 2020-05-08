#include "sdlvid.h"

#include "streaming/session.h"
#include "streaming/streamutils.h"
#include "path.h"

#include <QDir>

#include <Limelight.h>

SdlRenderer::SdlRenderer()
    : m_Renderer(nullptr),
      m_Texture(nullptr),
      m_SwPixelFormat(AV_PIX_FMT_NONE),
      m_FontData(Path::readDataFile("ModeSeven.ttf"))
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

bool SdlRenderer::prepareDecoderContext(AVCodecContext*, AVDictionary**)
{
    /* Nothing to do */

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using SDL renderer");

    return true;
}

void SdlRenderer::notifyOverlayUpdated(Overlay::OverlayType type)
{
    // Construct the required font to render the overlay
    if (m_OverlayFonts[type] == nullptr) {
        if (m_FontData.isEmpty()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL overlay font failed to load");
            return;
        }

        // m_FontData must stay around until the font is closed
        m_OverlayFonts[type] = TTF_OpenFontRW(SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()),
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

bool SdlRenderer::isRenderThreadSupported()
{
    SDL_RendererInfo info;
    SDL_GetRendererInfo(m_Renderer, &info);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "SDL renderer backend: %s",
                info.name);

    if (info.name != QString("direct3d")) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL renderer backend requires main thread rendering");
        return false;
    }

    return true;
}

bool SdlRenderer::isPixelFormatSupported(int, AVPixelFormat pixelFormat)
{
    // Remember to keep this in sync with SdlRenderer::renderFrame()!
    switch (pixelFormat)
    {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
        return true;

    default:
        return false;
    }
}

bool SdlRenderer::initialize(PDECODER_PARAMETERS params)
{
    Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;

    if (params->videoFormat == VIDEO_FORMAT_H265_MAIN10) {
        // SDL doesn't support rendering YUV 10-bit textures yet
        return false;
    }

    if ((SDL_GetWindowFlags(params->window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
        // In full-screen exclusive mode, we enable V-sync if requested. For other modes, Windows and Mac
        // have compositors that make rendering tear-free. Linux compositor varies by distro and user
        // configuration but doesn't seem feasible to detect here.
        if (params->enableVsync) {
            rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
        }
    }

#ifdef Q_OS_WIN32
    // We render on a different thread than the main thread which is handling window
    // messages. Without D3DCREATE_MULTITHREADED, this can cause a deadlock by blocking
    // on a window message being processed while the main thread is blocked waiting for
    // the render thread to finish.
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "1", SDL_HINT_OVERRIDE);
#endif

    m_Renderer = SDL_CreateRenderer(params->window, -1, rendererFlags);
    if (!m_Renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
        return false;
    }

    // Calculate the video region size, scaling to fill the output size while
    // preserving the aspect ratio of the video stream.
    SDL_Rect src, dst;
    src.x = src.y = 0;
    src.w = params->width;
    src.h = params->height;
    dst.x = dst.y = 0;
    SDL_GetRendererOutputSize(m_Renderer, &dst.w, &dst.h);
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    // Ensure the viewport is set to the desired video region
    SDL_RenderSetViewport(m_Renderer, &dst);

    // Draw a black frame until the video stream starts rendering
    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(m_Renderer);
    SDL_RenderPresent(m_Renderer);

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
                SDL_Rect viewportRect;
                SDL_RenderGetViewport(m_Renderer, &viewportRect);
                m_OverlayRects[type].x = 0;
                m_OverlayRects[type].y = viewportRect.h - surface->h;
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
    int err;
    AVFrame* swFrame = nullptr;

    if (frame->hw_frames_ctx != nullptr) {
        // If we are acting as the frontend for a hardware
        // accelerated decoder, we'll need to read the frame
        // back to render it.

        // Find the native read-back format
        if (m_SwPixelFormat == AV_PIX_FMT_NONE) {
            auto hwFrameCtx = (AVHWFramesContext*)frame->hw_frames_ctx->data;

            m_SwPixelFormat = hwFrameCtx->sw_format;
            SDL_assert(m_SwPixelFormat != AV_PIX_FMT_NONE);

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Selected read-back format: %d",
                        m_SwPixelFormat);
        }

        swFrame = av_frame_alloc();
        if (swFrame == nullptr) {
            return;
        }

        swFrame->width = frame->width;
        swFrame->height = frame->height;
        swFrame->format = m_SwPixelFormat;

        err = av_hwframe_transfer_data(swFrame, frame, 0);
        if (err != 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "av_hwframe_transfer_data() failed: %d",
                         err);
            goto Exit;
        }

        // av_hwframe_transfer_data() can nuke frame metadata,
        // so anything other than width, height, and format must
        // be set *after* calling av_hwframe_transfer_data().
        swFrame->colorspace = frame->colorspace;

        frame = swFrame;
    }

    if (m_Texture == nullptr) {
        Uint32 sdlFormat;

        // Remember to keep this in sync with SdlRenderer::isPixelFormatSupported()!
        switch (frame->format)
        {
        case AV_PIX_FMT_YUV420P:
            sdlFormat = SDL_PIXELFORMAT_YV12;
            break;
        case AV_PIX_FMT_NV12:
            sdlFormat = SDL_PIXELFORMAT_NV12;
            break;
        case AV_PIX_FMT_NV21:
            sdlFormat = SDL_PIXELFORMAT_NV21;
            break;
        default:
            SDL_assert(false);
            goto Exit;
        }

        switch (frame->colorspace)
        {
        case AVCOL_SPC_BT709:
            SDL_SetYUVConversionMode(SDL_YUV_CONVERSION_BT709);
            break;
        case AVCOL_SPC_BT470BG:
        case AVCOL_SPC_SMPTE170M:
        default:
            SDL_SetYUVConversionMode(SDL_YUV_CONVERSION_BT601);
            break;
        }

        m_Texture = SDL_CreateTexture(m_Renderer,
                                      sdlFormat,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      frame->width,
                                      frame->height);
        if (!m_Texture) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_CreateTexture() failed: %s",
                         SDL_GetError());
            goto Exit;
        }
    }

    if (frame->format == AV_PIX_FMT_YUV420P) {
        SDL_UpdateYUVTexture(m_Texture, nullptr,
                             frame->data[0],
                             frame->linesize[0],
                             frame->data[1],
                             frame->linesize[1],
                             frame->data[2],
                             frame->linesize[2]);
    }
    else {
        char* pixels;
        int pitch;

        err = SDL_LockTexture(m_Texture, nullptr, (void**)&pixels, &pitch);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_LockTexture() failed: %s",
                         SDL_GetError());
            goto Exit;
        }

        memcpy(pixels,
               frame->data[0],
               frame->linesize[0] * frame->height);
        memcpy(pixels + (frame->linesize[0] * frame->height),
               frame->data[1],
               frame->linesize[1] * frame->height / 2);

        SDL_UnlockTexture(m_Texture);
    }

    SDL_RenderClear(m_Renderer);

    // Draw the video content itself
    SDL_RenderCopy(m_Renderer, m_Texture, nullptr, nullptr);

    // Draw the overlays
    for (int i = 0; i < Overlay::OverlayMax; i++) {
        renderOverlay((Overlay::OverlayType)i);
    }

    SDL_RenderPresent(m_Renderer);

Exit:
    if (swFrame != nullptr) {
        av_frame_free(&swFrame);
    }
}
