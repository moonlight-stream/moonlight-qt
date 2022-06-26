#include "sdlvid.h"

#include "streaming/session.h"
#include "streaming/streamutils.h"

#include <Limelight.h>

SdlRenderer::SdlRenderer()
    : m_VideoFormat(0),
      m_Renderer(nullptr),
      m_Texture(nullptr),
      m_SwPixelFormat(AV_PIX_FMT_NONE),
      m_ColorSpace(AVCOL_SPC_UNSPECIFIED),
      m_MapFrame(false)
{
    SDL_zero(m_OverlayTextures);

#ifdef HAVE_CUDA
    m_CudaGLHelper = nullptr;
#endif
}

SdlRenderer::~SdlRenderer()
{
#ifdef HAVE_CUDA
    if (m_CudaGLHelper != nullptr) {
        delete m_CudaGLHelper;
    }
#endif

    for (int i = 0; i < Overlay::OverlayMax; i++) {
        if (m_OverlayTextures[i] != nullptr) {
            SDL_DestroyTexture(m_OverlayTextures[i]);
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

    m_VideoFormat = params->videoFormat;

    if (params->videoFormat & VIDEO_FORMAT_MASK_10BIT) {
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

    // SDL_CreateRenderer() can end up having to recreate our window (SDL_RecreateWindow())
    // to ensure it's compatible with the renderer's OpenGL context. If that happens, we
    // can get spurious SDL_WINDOWEVENT events that will cause us to (again) recreate our
    // renderer. This can lead to an infinite to renderer recreation, so discard all
    // SDL_WINDOWEVENT events after SDL_CreateRenderer().
    Session* session = Session::get();
    if (session != nullptr) {
        // If we get here during a session, we need to synchronize with the event loop
        // to ensure we don't drop any important events.
        session->flushWindowEvents();
    }
    else {
        // If we get here prior to the start of a session, just pump and flush ourselves.
        SDL_PumpEvents();
        SDL_FlushEvent(SDL_WINDOWEVENT);
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
        SDL_Surface* newSurface = Session::get()->getOverlayManager().getUpdatedOverlaySurface(type);
        if (newSurface != nullptr) {
            if (m_OverlayTextures[type] != nullptr) {
                SDL_DestroyTexture(m_OverlayTextures[type]);
            }

            if (type == Overlay::OverlayStatusUpdate) {
                // Bottom Left
                SDL_Rect viewportRect;
                SDL_RenderGetViewport(m_Renderer, &viewportRect);
                m_OverlayRects[type].x = 0;
                m_OverlayRects[type].y = viewportRect.h - newSurface->h;
            }
            else if (type == Overlay::OverlayDebug) {
                // Top left
                m_OverlayRects[type].x = 0;
                m_OverlayRects[type].y = 0;
            }

            m_OverlayRects[type].w = newSurface->w;
            m_OverlayRects[type].h = newSurface->h;

            m_OverlayTextures[type] = SDL_CreateTextureFromSurface(m_Renderer, newSurface);
            SDL_FreeSurface(newSurface);
        }

        // If we have an overlay texture, render it too
        if (m_OverlayTextures[type] != nullptr) {
            SDL_RenderCopy(m_Renderer, m_OverlayTextures[type], nullptr, &m_OverlayRects[type]);
        }
    }
}

bool SdlRenderer::initializeReadBackFormat(AVBufferRef* hwFrameCtxRef, AVFrame* testFrame)
{
    auto hwFrameCtx = (AVHWFramesContext*)hwFrameCtxRef->data;
    int err;
    enum AVPixelFormat *formats;
    AVFrame* outputFrame;

    // This function must only be called once per instance
    SDL_assert(m_SwPixelFormat == AV_PIX_FMT_NONE);
    SDL_assert(!m_MapFrame);

    // Try direct mapping before resorting to copying the frame
    outputFrame = av_frame_alloc();
    if (outputFrame != nullptr) {
        err = av_hwframe_map(outputFrame, testFrame, AV_HWFRAME_MAP_READ);
        if (err == 0) {
            if (isPixelFormatSupported(m_VideoFormat, (AVPixelFormat)outputFrame->format)) {
                m_SwPixelFormat = (AVPixelFormat)outputFrame->format;
                m_MapFrame = true;
                goto Exit;
            }
            else {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Skipping unsupported hwframe mapping format: %d",
                            outputFrame->format);
            }
        }
        else {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "av_hwframe_map() is unsupported (error: %d)",
                        err);
            SDL_assert(err == AVERROR(ENOSYS));
        }
    }

    // Direct mapping didn't work, so let's see what transfer formats we have
    err = av_hwframe_transfer_get_formats(hwFrameCtxRef, AV_HWFRAME_TRANSFER_DIRECTION_FROM, &formats, 0);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "av_hwframe_transfer_get_formats() failed: %d",
                     err);
        goto Exit;
    }

    // NB: In this algorithm, we prefer to get a preferred hardware readback format
    // and non-preferred rendering format rather than the other way around. This is
    // why we loop through the readback format list in order, rather than searching
    // for the format from getPreferredPixelFormat() in the list.
    for (int i = 0; formats[i] != AV_PIX_FMT_NONE; i++) {
        SDL_assert(m_VideoFormat != 0);
        if (!isPixelFormatSupported(m_VideoFormat, formats[i])) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Skipping unsupported hwframe transfer format %d",
                        formats[i]);
            continue;
        }

        m_SwPixelFormat = formats[i];
        break;
    }

    av_freep(&formats);

Exit:
    av_frame_free(&outputFrame);

    // If we didn't find any supported formats, try hwFrameCtx->sw_format.
    if (m_SwPixelFormat == AV_PIX_FMT_NONE) {
        if (isPixelFormatSupported(m_VideoFormat, hwFrameCtx->sw_format)) {
            m_SwPixelFormat = hwFrameCtx->sw_format;
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to find compatible hwframe transfer format (sw_format = %d)",
                         hwFrameCtx->sw_format);
            return false;
        }
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Selected hwframe->swframe format: %d (mapping: %s)",
                m_SwPixelFormat,
                m_MapFrame ? "yes" : "no");
    return true;
}

AVFrame* SdlRenderer::getSwFrameFromHwFrame(AVFrame* hwFrame)
{
    int err;

    SDL_assert(m_SwPixelFormat != AV_PIX_FMT_NONE);

    AVFrame* swFrame = av_frame_alloc();
    if (swFrame == nullptr) {
        return nullptr;
    }

    swFrame->format = m_SwPixelFormat;

    if (m_MapFrame) {
        // We don't use AV_HWFRAME_MAP_DIRECT here because it can cause huge
        // performance penalties on Intel hardware with VAAPI due to mappings
        // being uncached memory.
        err = av_hwframe_map(swFrame, hwFrame, AV_HWFRAME_MAP_READ);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "av_hwframe_map() failed: %d",
                         err);
            av_frame_free(&swFrame);
            return nullptr;
        }
    }
    else {
        err = av_hwframe_transfer_data(swFrame, hwFrame, 0);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "av_hwframe_transfer_data() failed: %d",
                         err);
            av_frame_free(&swFrame);
            return nullptr;
        }

        // av_hwframe_transfer_data() doesn't transfer metadata
        // (and can even nuke existing metadata in dst), so we
        // will propagate metadata manually afterwards.
        av_frame_copy_props(swFrame, hwFrame);
    }

    return swFrame;
}

void SdlRenderer::renderFrame(AVFrame* frame)
{
    int err;
    AVFrame* swFrame = nullptr;

    if (frame->hw_frames_ctx != nullptr && frame->format != AV_PIX_FMT_CUDA) {
#ifdef HAVE_CUDA
ReadbackRetry:
#endif
        // If we are acting as the frontend for a hardware
        // accelerated decoder, we'll need to read the frame
        // back to render it.

        // Find the native read-back format
        if (m_SwPixelFormat == AV_PIX_FMT_NONE) {
            initializeReadBackFormat(frame->hw_frames_ctx, frame);

            // If we don't support any of the hw transfer formats, we should
            // have failed inside testRenderFrame() and not made it here.
            SDL_assert(m_SwPixelFormat != AV_PIX_FMT_NONE);
        }

        // Map or copy this hwframe to a swframe that we can work with
        frame = swFrame = getSwFrameFromHwFrame(frame);
        if (swFrame == nullptr) {
            return;
        }
    }

    // Because the specific YUV color conversion shader is established at
    // texture creation for most SDL render backends, we need to recreate
    // the texture when the colorspace changes.
    if (frame->colorspace != m_ColorSpace) {
#ifdef HAVE_CUDA
        if (m_CudaGLHelper != nullptr) {
            delete m_CudaGLHelper;
            m_CudaGLHelper = nullptr;
        }
#endif

        if (m_Texture != nullptr) {
            SDL_DestroyTexture(m_Texture);
            m_Texture = nullptr;
        }

        m_ColorSpace = frame->colorspace;
    }

    if (m_Texture == nullptr) {
        Uint32 sdlFormat;

        // Remember to keep this in sync with SdlRenderer::isPixelFormatSupported()!
        switch (frame->format)
        {
        case AV_PIX_FMT_YUV420P:
            sdlFormat = SDL_PIXELFORMAT_YV12;
            break;
        case AV_PIX_FMT_CUDA:
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

#ifdef HAVE_CUDA
        if (frame->format == AV_PIX_FMT_CUDA) {
            SDL_assert(m_CudaGLHelper == nullptr);
            m_CudaGLHelper = new CUDAGLInteropHelper(((AVHWFramesContext*)frame->hw_frames_ctx->data)->device_ctx);

            SDL_GL_BindTexture(m_Texture, nullptr, nullptr);
            if (!m_CudaGLHelper->registerBoundTextures()) {
                // If we can't register textures, fall back to normal read-back rendering
                delete m_CudaGLHelper;
                m_CudaGLHelper = nullptr;
            }
            SDL_GL_UnbindTexture(m_Texture);
        }
#endif
    }

    if (frame->format == AV_PIX_FMT_CUDA) {
#ifdef HAVE_CUDA
        if (m_CudaGLHelper == nullptr || !m_CudaGLHelper->copyCudaFrameToTextures(frame)) {
            goto ReadbackRetry;
        }
#else
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Got CUDA frame, but not built with CUDA support!");
        goto Exit;
#endif
    }
    else if (frame->format == AV_PIX_FMT_YUV420P) {
        SDL_UpdateYUVTexture(m_Texture, nullptr,
                             frame->data[0],
                             frame->linesize[0],
                             frame->data[1],
                             frame->linesize[1],
                             frame->data[2],
                             frame->linesize[2]);
    }
    else {
#if SDL_VERSION_ATLEAST(2, 0, 15)
        // SDL_UpdateNVTexture is not supported on all renderer backends,
        // (notably not DX9), so we must have a fallback in case it's not
        // supported and for earlier versions of SDL.
        if (SDL_UpdateNVTexture(m_Texture,
                                nullptr,
                                frame->data[0],
                                frame->linesize[0],
                                frame->data[1],
                                frame->linesize[1]) != 0)
#endif
        {
            char* pixels;
            int texturePitch;

            err = SDL_LockTexture(m_Texture, nullptr, (void**)&pixels, &texturePitch);
            if (err < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "SDL_LockTexture() failed: %s",
                             SDL_GetError());
                goto Exit;
            }

            // If the planar pitches match, we can use a single memcpy() to transfer
            // the data. If not, we'll need to do separate memcpy() calls for each
            // line to ensure the pitch doesn't get screwed up.

            if (frame->linesize[0] == texturePitch) {
                memcpy(pixels,
                       frame->data[0],
                       frame->linesize[0] * frame->height);
            }
            else {
                int pitch = SDL_min(frame->linesize[0], texturePitch);
                for (int i = 0; i < frame->height; i++) {
                    memcpy(pixels + (texturePitch * i),
                           frame->data[0] + (frame->linesize[0] * i),
                           pitch);
                }
            }

            if (frame->linesize[1] == texturePitch) {
                memcpy(pixels + (texturePitch * frame->height),
                       frame->data[1],
                       frame->linesize[1] * frame->height / 2);
            }
            else {
                int pitch = SDL_min(frame->linesize[1], texturePitch);
                for (int i = 0; i < frame->height / 2; i++) {
                    memcpy(pixels + (texturePitch * (frame->height + i)),
                           frame->data[1] + (frame->linesize[1] * i),
                           pitch);
                }
            }

            SDL_UnlockTexture(m_Texture);
        }
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

bool SdlRenderer::testRenderFrame(AVFrame* frame)
{
    // If we are acting as the frontend for a hardware
    // accelerated decoder, we'll need to read the frame
    // back to render it. Test that this can be done
    // for the given frame successfully.
    if (frame->hw_frames_ctx != nullptr) {
#ifdef HAVE_MMAL
        // FFmpeg for Raspberry Pi has NEON-optimized routines that allow
        // us to use av_hwframe_transfer_data() to convert from SAND frames
        // to standard fully-planar YUV. Unfortunately, the combination of
        // doing this conversion on the CPU and the slow GL texture upload
        // performance on the Pi make the performance of this approach
        // unacceptable. Don't use this copyback path on the Pi by default
        // to ensure we fall back to H.264 (with the MMAL renderer) in X11
        // rather than using HEVC+copyback and getting terrible performance.
        if (qgetenv("RPI_ALLOW_COPYBACK_RENDER") != "1") {
            return false;
        }
#endif

        if (!initializeReadBackFormat(frame->hw_frames_ctx, frame)) {
            return false;
        }

        AVFrame* swFrame = getSwFrameFromHwFrame(frame);
        if (swFrame == nullptr) {
            return false;
        }

        av_frame_free(&swFrame);
    }
    else if (!isPixelFormatSupported(m_VideoFormat, (AVPixelFormat)frame->format)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Swframe pixel format unsupported: %d",
                    frame->format);
        return false;
    }

    return true;
}
