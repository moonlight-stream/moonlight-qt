#include <Limelight.h>
#include "ffmpeg.h"
#include "streaming/streamutils.h"
#include "streaming/session.h"

#include <h264_stream.h>

#include "ffmpeg-renderers/sdlvid.h"
#include "ffmpeg-renderers/cuda.h"

#ifdef Q_OS_WIN32
#include "ffmpeg-renderers/dxva2.h"
#endif

#ifdef Q_OS_DARWIN
#include "ffmpeg-renderers/vt.h"
#endif

#ifdef HAVE_LIBVA
#include "ffmpeg-renderers/vaapi.h"
#endif

#ifdef HAVE_LIBVDPAU
#include "ffmpeg-renderers/vdpau.h"
#endif

#ifdef HAVE_MMAL
#include "ffmpeg-renderers/mmal.h"
#endif

#ifdef HAVE_DRM
#include "ffmpeg-renderers/drm.h"
#endif

#ifdef HAVE_EGL
#include "ffmpeg-renderers/eglvid.h"
#endif

// This is gross but it allows us to use sizeof()
#include "ffmpeg_videosamples.cpp"

#define MAX_SPS_EXTRA_SIZE 16

#define FAILED_DECODES_RESET_THRESHOLD 20

bool FFmpegVideoDecoder::isHardwareAccelerated()
{
    return m_HwDecodeCfg != nullptr ||
            (m_VideoDecoderCtx->codec->capabilities & AV_CODEC_CAP_HARDWARE) != 0;
}

bool FFmpegVideoDecoder::isAlwaysFullScreen()
{
    return m_FrontendRenderer->getRendererAttributes() & RENDERER_ATTRIBUTE_FULLSCREEN_ONLY;
}

int FFmpegVideoDecoder::getDecoderCapabilities()
{
    int capabilities = m_BackendRenderer->getDecoderCapabilities();

    if (!isHardwareAccelerated()) {
        // Slice up to 4 times for parallel CPU decoding, once slice per core
        int slices = qMin(MAX_SLICES, SDL_GetCPUCount());
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Encoder configured for %d slices per frame",
                    slices);
        capabilities |= CAPABILITY_SLICES_PER_FRAME(slices);
    }

    return capabilities;
}

int FFmpegVideoDecoder::getDecoderColorspace()
{
    return m_FrontendRenderer->getDecoderColorspace();
}

QSize FFmpegVideoDecoder::getDecoderMaxResolution()
{
    if (m_BackendRenderer->getRendererAttributes() & RENDERER_ATTRIBUTE_1080P_MAX) {
        return QSize(1920, 1080);
    }
    else {
        // No known maximum
        return QSize(0, 0);
    }
}

enum AVPixelFormat FFmpegVideoDecoder::ffGetFormat(AVCodecContext* context,
                                                   const enum AVPixelFormat* pixFmts)
{
    FFmpegVideoDecoder* decoder = (FFmpegVideoDecoder*)context->opaque;
    const enum AVPixelFormat *p;

    for (p = pixFmts; *p != -1; p++) {
        // Only match our hardware decoding codec or preferred SW pixel
        // format (if not using hardware decoding). It's crucial
        // to override the default get_format() which will try
        // to gracefully fall back to software decode and break us.
        if (*p == (decoder->m_HwDecodeCfg ?
                   decoder->m_HwDecodeCfg->pix_fmt :
                   context->pix_fmt)) {
            return *p;
        }
    }

    // Failed to match the preferred pixel formats. Try non-preferred options for non-hwaccel decoders.
    if (decoder->m_HwDecodeCfg == nullptr) {
        for (p = pixFmts; *p != -1; p++) {
            if (decoder->m_FrontendRenderer->isPixelFormatSupported(decoder->m_VideoFormat, *p)) {
                return *p;
            }
        }
    }

    return AV_PIX_FMT_NONE;
}

FFmpegVideoDecoder::FFmpegVideoDecoder(bool testOnly)
    : m_Pkt(av_packet_alloc()),
      m_VideoDecoderCtx(nullptr),
      m_DecodeBuffer(1024 * 1024, 0),
      m_HwDecodeCfg(nullptr),
      m_BackendRenderer(nullptr),
      m_FrontendRenderer(nullptr),
      m_ConsecutiveFailedDecodes(0),
      m_Pacer(nullptr),
      m_FramesIn(0),
      m_FramesOut(0),
      m_LastFrameNumber(0),
      m_StreamFps(0),
      m_VideoFormat(0),
      m_NeedsSpsFixup(false),
      m_TestOnly(testOnly)
{
    SDL_zero(m_ActiveWndVideoStats);
    SDL_zero(m_LastWndVideoStats);
    SDL_zero(m_GlobalVideoStats);

    // Use linear filtering when renderer scaling is required
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
}

FFmpegVideoDecoder::~FFmpegVideoDecoder()
{
    reset();

    // Set log level back to default.
    // NB: We don't do this in reset() because we want
    // to preserve the log level across reset() during
    // test initialization.
    av_log_set_level(AV_LOG_INFO);

    av_packet_free(&m_Pkt);
}

IFFmpegRenderer* FFmpegVideoDecoder::getBackendRenderer()
{
    return m_BackendRenderer;
}

void FFmpegVideoDecoder::reset()
{
    delete m_Pacer;
    m_Pacer = nullptr;

    // This must be called after deleting Pacer because it
    // may be holding AVFrames to free in its destructor.
    // However, it must be called before deleting the IFFmpegRenderer
    // since the codec context may be referencing objects that we
    // need to delete in the renderer destructor.
    avcodec_free_context(&m_VideoDecoderCtx);

    if (!m_TestOnly) {
        Session::get()->getOverlayManager().setOverlayRenderer(nullptr);
    }

    // If we have a separate frontend renderer, free that first
    if (m_FrontendRenderer != m_BackendRenderer) {
        delete m_FrontendRenderer;
    }

    delete m_BackendRenderer;

    m_FrontendRenderer = m_BackendRenderer = nullptr;

    if (!m_TestOnly) {
        logVideoStats(m_GlobalVideoStats, "Global video stats");
    }
    else {
        // Test-only decoders can't have any frames submitted
        SDL_assert(m_GlobalVideoStats.totalFrames == 0);
    }
}

bool FFmpegVideoDecoder::createFrontendRenderer(PDECODER_PARAMETERS params, bool eglOnly)
{
    if (eglOnly) {
#ifdef HAVE_EGL
        if (m_BackendRenderer->canExportEGL()) {
            m_FrontendRenderer = new EGLRenderer(m_BackendRenderer);
            if (m_FrontendRenderer->initialize(params)) {
                return true;
            }
            delete m_FrontendRenderer;
            m_FrontendRenderer = nullptr;
        }
#endif
        // If we made it here, we failed to create the EGLRenderer
        return false;
    }

    if (m_BackendRenderer->isDirectRenderingSupported()) {
        // The backend renderer can render to the display
        m_FrontendRenderer = m_BackendRenderer;
    }
    else {
        // The backend renderer cannot directly render to the display, so
        // we will create an SDL renderer to draw the frames.
        m_FrontendRenderer = new SdlRenderer();
        if (!m_FrontendRenderer->initialize(params)) {
            return false;
        }
    }

    return true;
}

bool FFmpegVideoDecoder::completeInitialization(AVCodec* decoder, PDECODER_PARAMETERS params, bool testFrame, bool eglOnly)
{
    // In test-only mode, we should only see test frames
    SDL_assert(!m_TestOnly || testFrame);

    // Create the frontend renderer based on the capabilities of the backend renderer
    if (!createFrontendRenderer(params, eglOnly)) {
        return false;
    }

    m_StreamFps = params->frameRate;
    m_VideoFormat = params->videoFormat;

    // Don't bother initializing Pacer if we're not actually going to render
    if (!testFrame) {
        m_Pacer = new Pacer(m_FrontendRenderer, &m_ActiveWndVideoStats);
        if (!m_Pacer->initialize(params->window, params->frameRate, params->enableFramePacing)) {
            return false;
        }
    }

    m_VideoDecoderCtx = avcodec_alloc_context3(decoder);
    if (!m_VideoDecoderCtx) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to allocate video decoder context");
        return false;
    }

    // Always request low delay decoding
    m_VideoDecoderCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    // Allow display of corrupt frames and frames missing references
    m_VideoDecoderCtx->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
    m_VideoDecoderCtx->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;

    // Report decoding errors to allow us to request a key frame
    //
    // With HEVC streams, FFmpeg can drop a frame (hwaccel->start_frame() fails)
    // without telling us. Since we have an infinite GOP length, this causes artifacts
    // on screen that persist for a long time. It's easy to cause this condition
    // by using NVDEC and delaying 100 ms randomly in the render path so the decoder
    // runs out of output buffers.
    m_VideoDecoderCtx->err_recognition = AV_EF_EXPLODE;

    // Enable slice multi-threading for software decoding
    if (!isHardwareAccelerated()) {
        m_VideoDecoderCtx->thread_type = FF_THREAD_SLICE;
        m_VideoDecoderCtx->thread_count = qMin(MAX_SLICES, SDL_GetCPUCount());
    }
    else {
        // No threading for HW decode
        m_VideoDecoderCtx->thread_count = 1;
    }

    // Setup decoding parameters
    m_VideoDecoderCtx->width = params->width;
    m_VideoDecoderCtx->height = params->height;
    m_VideoDecoderCtx->pix_fmt = m_FrontendRenderer->getPreferredPixelFormat(params->videoFormat);
    m_VideoDecoderCtx->get_format = ffGetFormat;

    AVDictionary* options = nullptr;

    // Allow the backend renderer to attach data to this decoder
    if (!m_BackendRenderer->prepareDecoderContext(m_VideoDecoderCtx, &options)) {
        return false;
    }

    // Nobody must override our ffGetFormat
    SDL_assert(m_VideoDecoderCtx->get_format == ffGetFormat);

    // Stash a pointer to this object in the context
    SDL_assert(m_VideoDecoderCtx->opaque == nullptr);
    m_VideoDecoderCtx->opaque = this;

    int err = avcodec_open2(m_VideoDecoderCtx, decoder, &options);
    av_dict_free(&options);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to open decoder for format: %x",
                     params->videoFormat);
        return false;
    }

    // FFMpeg doesn't completely initialize the codec until the codec
    // config data comes in. This would be too late for us to change
    // our minds on the selected video codec, so we'll do a trial run
    // now to see if things will actually work when the video stream
    // comes in.
    if (testFrame) {
        switch (params->videoFormat) {
        case VIDEO_FORMAT_H264:
            m_Pkt->data = (uint8_t*)k_H264TestFrame;
            m_Pkt->size = sizeof(k_H264TestFrame);
            break;
        case VIDEO_FORMAT_H265:
            m_Pkt->data = (uint8_t*)k_HEVCMainTestFrame;
            m_Pkt->size = sizeof(k_HEVCMainTestFrame);
            break;
        case VIDEO_FORMAT_H265_MAIN10:
            m_Pkt->data = (uint8_t*)k_HEVCMain10TestFrame;
            m_Pkt->size = sizeof(k_HEVCMain10TestFrame);
            break;
        default:
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "No test frame for format: %x",
                         params->videoFormat);
            return false;
        }

        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to allocate frame");
            return false;
        }

        // Some decoders won't output on the first frame, so we'll submit
        // a few test frames if we get an EAGAIN error.
        for (int retries = 0; retries < 5; retries++) {
            // Most FFmpeg decoders process input using a "push" model.
            // We'll see those fail here if the format is not supported.
            err = avcodec_send_packet(m_VideoDecoderCtx, m_Pkt);
            if (err < 0) {
                av_frame_free(&frame);
                char errorstring[512];
                av_strerror(err, errorstring, sizeof(errorstring));
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Test decode failed: %s", errorstring);
                return false;
            }

            // A few FFmpeg decoders (h264_mmal) process here using a "pull" model.
            // Those decoders will fail here if the format is not supported.
            err = avcodec_receive_frame(m_VideoDecoderCtx, frame);
            if (err == AVERROR(EAGAIN)) {
                // Wait a little while to let the hardware work
                SDL_Delay(100);
            }
            else {
                // Done!
                break;
            }
        }

        // Allow the renderer to do any validation it wants on this frame
        if (!m_FrontendRenderer->testRenderFrame(frame)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Render test failed");
            av_frame_free(&frame);
            return false;
        }

        av_frame_free(&frame);
        if (err < 0) {
            char errorstring[512];
            av_strerror(err, errorstring, sizeof(errorstring));
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Test decode failed: %s", errorstring);
            return false;
        }
    }
    else {
        if ((params->videoFormat & VIDEO_FORMAT_MASK_H264) &&
                !(m_BackendRenderer->getDecoderCapabilities() & CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC)) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Using H.264 SPS fixup");
            m_NeedsSpsFixup = true;
        }
        else {
            m_NeedsSpsFixup = false;
        }

        // Tell overlay manager to use this frontend renderer
        Session::get()->getOverlayManager().setOverlayRenderer(m_FrontendRenderer);
    }

    return true;
}

void FFmpegVideoDecoder::addVideoStats(VIDEO_STATS& src, VIDEO_STATS& dst)
{
    dst.receivedFrames += src.receivedFrames;
    dst.decodedFrames += src.decodedFrames;
    dst.renderedFrames += src.renderedFrames;
    dst.totalFrames += src.totalFrames;
    dst.networkDroppedFrames += src.networkDroppedFrames;
    dst.pacerDroppedFrames += src.pacerDroppedFrames;
    dst.totalReassemblyTime += src.totalReassemblyTime;
    dst.totalDecodeTime += src.totalDecodeTime;
    dst.totalPacerTime += src.totalPacerTime;
    dst.totalRenderTime += src.totalRenderTime;

    Uint32 now = SDL_GetTicks();

    // Initialize the measurement start point if this is the first video stat window
    if (!dst.measurementStartTimestamp) {
        dst.measurementStartTimestamp = src.measurementStartTimestamp;
    }

    // The following code assumes the global measure was already started first
    SDL_assert(dst.measurementStartTimestamp <= src.measurementStartTimestamp);

    dst.totalFps = (float)dst.totalFrames / ((float)(now - dst.measurementStartTimestamp) / 1000);
    dst.receivedFps = (float)dst.receivedFrames / ((float)(now - dst.measurementStartTimestamp) / 1000);
    dst.decodedFps = (float)dst.decodedFrames / ((float)(now - dst.measurementStartTimestamp) / 1000);
    dst.renderedFps = (float)dst.renderedFrames / ((float)(now - dst.measurementStartTimestamp) / 1000);
}

void FFmpegVideoDecoder::stringifyVideoStats(VIDEO_STATS& stats, char* output)
{
    int offset = 0;
    const char* codecString;

    // Start with an empty string
    output[offset] = 0;

    switch (m_VideoFormat)
    {
    case VIDEO_FORMAT_H264:
        codecString = "H.264";
        break;

    case VIDEO_FORMAT_H265:
        codecString = "HEVC";
        break;

    case VIDEO_FORMAT_H265_MAIN10:
        codecString = "HEVC Main 10";
        break;

    default:
        SDL_assert(false);
        codecString = "UNKNOWN";
        break;
    }

    if (stats.receivedFps > 0) {
        if (m_VideoDecoderCtx != nullptr) {
            offset += sprintf(&output[offset],
                              "Video stream: %dx%d %.2f FPS (Codec: %s)\n",
                              m_VideoDecoderCtx->width,
                              m_VideoDecoderCtx->height,
                              stats.totalFps,
                              codecString);
        }

        offset += sprintf(&output[offset],
                          "Incoming frame rate from network: %.2f FPS\n"
                          "Decoding frame rate: %.2f FPS\n"
                          "Rendering frame rate: %.2f FPS\n",
                          stats.receivedFps,
                          stats.decodedFps,
                          stats.renderedFps);
    }

    if (stats.renderedFrames != 0) {
        offset += sprintf(&output[offset],
                          "Frames dropped by your network connection: %.2f%%\n"
                          "Frames dropped due to network jitter: %.2f%%\n"
                          "Average receive time: %.2f ms\n"
                          "Average decoding time: %.2f ms\n"
                          "Average frame queue delay: %.2f ms\n"
                          "Average rendering time (including monitor V-sync latency): %.2f ms\n",
                          (float)stats.networkDroppedFrames / stats.totalFrames * 100,
                          (float)stats.pacerDroppedFrames / stats.decodedFrames * 100,
                          (float)stats.totalReassemblyTime / stats.receivedFrames,
                          (float)stats.totalDecodeTime / stats.decodedFrames,
                          (float)stats.totalPacerTime / stats.renderedFrames,
                          (float)stats.totalRenderTime / stats.renderedFrames);
    }
}

void FFmpegVideoDecoder::logVideoStats(VIDEO_STATS& stats, const char* title)
{
    if (stats.renderedFps > 0 || stats.renderedFrames != 0) {
        char videoStatsStr[512];
        stringifyVideoStats(stats, videoStatsStr);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "%s", title);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "----------------------------------------------------------\n%s",
                    videoStatsStr);
    }
}

IFFmpegRenderer* FFmpegVideoDecoder::createHwAccelRenderer(const AVCodecHWConfig* hwDecodeCfg, int pass)
{
    if (!(hwDecodeCfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
        return nullptr;
    }

    // First pass using our top-tier hwaccel implementations
    if (pass == 0) {
        switch (hwDecodeCfg->device_type) {
#ifdef Q_OS_WIN32
        case AV_HWDEVICE_TYPE_DXVA2:
            return new DXVA2Renderer();
#endif
#ifdef Q_OS_DARWIN
        case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
            return VTRendererFactory::createRenderer();
#endif
#ifdef HAVE_LIBVA
        case AV_HWDEVICE_TYPE_VAAPI:
            return new VAAPIRenderer();
#endif
#ifdef HAVE_LIBVDPAU
        case AV_HWDEVICE_TYPE_VDPAU:
            return new VDPAURenderer();
#endif
#ifdef HAVE_DRM
        case AV_HWDEVICE_TYPE_DRM:
            return new DrmRenderer();
#endif
        default:
            return nullptr;
        }
    }
    // Second pass for our second-tier hwaccel implementations
    else if (pass == 1) {
        switch (hwDecodeCfg->device_type) {
        case AV_HWDEVICE_TYPE_CUDA:
            // CUDA should only be used if all other options fail, since it requires
            // read-back of frames. This should only be used for the NVIDIA+Wayland case
            // with VDPAU covering the NVIDIA+X11 scenario.
            return new CUDARenderer();
        default:
            return nullptr;
        }
    }
    else {
        SDL_assert(false);
        return nullptr;
    }
}

bool FFmpegVideoDecoder::tryInitializeRenderer(AVCodec* decoder,
                                               PDECODER_PARAMETERS params,
                                               const AVCodecHWConfig* hwConfig,
                                               std::function<IFFmpegRenderer*()> createRendererFunc)
{
    m_HwDecodeCfg = hwConfig;

    // i == 0 - Indirect via EGL frontend with zero-copy DMA-BUF passing
    // i == 1 - Direct rendering or indirect via SDL read-back
#ifdef HAVE_EGL
    for (int i = 0; i < 2; i++) {
#else
    for (int i = 1; i < 2; i++) {
#endif
        SDL_assert(m_BackendRenderer == nullptr);
        if ((m_BackendRenderer = createRendererFunc()) != nullptr &&
                m_BackendRenderer->initialize(params) &&
                completeInitialization(decoder, params, m_TestOnly || m_BackendRenderer->needsTestFrame(), i == 0 /* EGL */)) {
            if (m_TestOnly) {
                // This decoder is only for testing capabilities, so don't bother
                // creating a usable renderer
                return true;
            }

            if (m_BackendRenderer->needsTestFrame()) {
                // The test worked, so now let's initialize it for real
                reset();
                if ((m_BackendRenderer = createRendererFunc()) != nullptr &&
                        m_BackendRenderer->initialize(params) &&
                        completeInitialization(decoder, params, false, i == 0 /* EGL */)) {
                    return true;
                }
                else {
                    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION,
                                    "Decoder failed to initialize after successful test");
                    reset();
                }
            }
            else {
                // No test required. Good to go now.
                return true;
            }
        }
        else {
            // Failed to initialize, so keep looking
            reset();
        }
    }

    // reset() must be called before we reach this point!
    SDL_assert(m_BackendRenderer == nullptr);
    return false;
}

#define TRY_PREFERRED_PIXEL_FORMAT(RENDERER_TYPE) \
    { \
        RENDERER_TYPE renderer; \
        if (renderer.getPreferredPixelFormat(params->videoFormat) == decoder->pix_fmts[i]) { \
            if (tryInitializeRenderer(decoder, params, nullptr, \
                                      []() -> IFFmpegRenderer* { return new RENDERER_TYPE(); })) { \
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, \
                            "Chose " #RENDERER_TYPE " for codec %s due to preferred pixel format: 0x%x", \
                            decoder->name, decoder->pix_fmts[i]); \
                return true; \
            } \
        } \
    }

#define TRY_SUPPORTED_PIXEL_FORMAT(RENDERER_TYPE) \
    { \
        RENDERER_TYPE renderer; \
        if (renderer.isPixelFormatSupported(params->videoFormat, decoder->pix_fmts[i])) { \
            if (tryInitializeRenderer(decoder, params, nullptr, \
                                      []() -> IFFmpegRenderer* { return new RENDERER_TYPE(); })) { \
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, \
                            "Chose " #RENDERER_TYPE " for codec %s due to compatible pixel format: 0x%x", \
                            decoder->name, decoder->pix_fmts[i]); \
                return true; \
            } \
        } \
    }

bool FFmpegVideoDecoder::tryInitializeRendererForDecoderByName(const char *decoderName,
                                                               PDECODER_PARAMETERS params)
{
    AVCodec* decoder = avcodec_find_decoder_by_name(decoderName);
    if (decoder == nullptr) {
        return false;
    }

    // This might be a hwaccel decoder, so try any hw configs first
    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
        if (!config) {
            // No remaing hwaccel options
            break;
        }

        // Initialize the hardware codec and submit a test frame if the renderer needs it
        if (tryInitializeRenderer(decoder, params, config,
                                  [config]() -> IFFmpegRenderer* { return createHwAccelRenderer(config, 0); })) {
            return true;
        }
    }

    if (decoder->pix_fmts == NULL) {
        // Supported output pixel formats are unknown. We'll just try SDL and hope it can cope.
        return tryInitializeRenderer(decoder, params, nullptr,
                                     []() -> IFFmpegRenderer* { return new SdlRenderer(); });
    }

    // Check if any of our decoders prefer any of the pixel formats first
    for (int i = 0; decoder->pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
#ifdef HAVE_DRM
        TRY_PREFERRED_PIXEL_FORMAT(DrmRenderer);
#endif
#ifdef HAVE_MMAL
        TRY_PREFERRED_PIXEL_FORMAT(MmalRenderer);
#endif
        TRY_PREFERRED_PIXEL_FORMAT(SdlRenderer);
    }

    // Nothing prefers any of them. Let's see if anyone will tolerate one.
    for (int i = 0; decoder->pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
#ifdef HAVE_DRM
        TRY_SUPPORTED_PIXEL_FORMAT(DrmRenderer);
#endif
#ifdef HAVE_MMAL
        TRY_SUPPORTED_PIXEL_FORMAT(MmalRenderer);
#endif
        TRY_SUPPORTED_PIXEL_FORMAT(SdlRenderer);
    }

    // If we made it here, we couldn't find anything
    return false;
}

bool FFmpegVideoDecoder::initialize(PDECODER_PARAMETERS params)
{
    // Increase log level until the first frame is decoded
    av_log_set_level(AV_LOG_DEBUG);

    // First try decoders that the user has manually specified via environment variables.
    // These must output surfaces in one of the formats that one of our renderers supports,
    // which is currently:
    // - AV_PIX_FMT_DRM_PRIME
    // - AV_PIX_FMT_MMAL
    // - AV_PIX_FMT_YUV420P
    // - AV_PIX_FMT_NV12
    // - AV_PIX_FMT_NV21
    {
        QString h264DecoderHint = qgetenv("H264_DECODER_HINT");
        if (!h264DecoderHint.isEmpty() && (params->videoFormat & VIDEO_FORMAT_MASK_H264)) {
            QByteArray decoderString = h264DecoderHint.toLocal8Bit();
            if (tryInitializeRendererForDecoderByName(decoderString.constData(), params)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Using custom H.264 decoder (H264_DECODER_HINT): %s",
                            decoderString.constData());
                return true;
            }
            else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Custom H.264 decoder (H264_DECODER_HINT) failed to load: %s",
                             decoderString.constData());
            }
        }
    }
    {
        QString hevcDecoderHint = qgetenv("HEVC_DECODER_HINT");
        if (!hevcDecoderHint.isEmpty() && (params->videoFormat & VIDEO_FORMAT_MASK_H265)) {
            QByteArray decoderString = hevcDecoderHint.toLocal8Bit();
            if (tryInitializeRendererForDecoderByName(decoderString.constData(), params)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Using custom HEVC decoder (HEVC_DECODER_HINT): %s",
                            decoderString.constData());
                return true;
            }
            else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Custom HEVC decoder (HEVC_DECODER_HINT) failed to load: %s",
                             decoderString.constData());
            }
        }
    }

    AVCodec* decoder;

    if (params->videoFormat & VIDEO_FORMAT_MASK_H264) {
        decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    }
    else if (params->videoFormat & VIDEO_FORMAT_MASK_H265) {
        decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    }
    else {
        Q_ASSERT(false);
        decoder = nullptr;
    }

    if (!decoder) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to find decoder for format: %x",
                     params->videoFormat);
        return false;
    }

    // Look for a hardware decoder first unless software-only
    if (params->vds != StreamingPreferences::VDS_FORCE_SOFTWARE) {
        // Look for the first matching hwaccel hardware decoder (pass 0)
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
            if (!config) {
                // No remaing hwaccel options
                break;
            }

            // Initialize the hardware codec and submit a test frame if the renderer needs it
            if (tryInitializeRenderer(decoder, params, config,
                                      [config]() -> IFFmpegRenderer* { return createHwAccelRenderer(config, 0); })) {
                return true;
            }
        }

        // Continue with special non-hwaccel hardware decoders
        if (params->videoFormat & VIDEO_FORMAT_MASK_H264) {
            QList<const char *> knownAvcCodecs = { "h264_mmal", "h264_rkmpp", "h264_nvmpi", "h264_v4l2m2m" };
            for (const char* codec : knownAvcCodecs) {
                if (tryInitializeRendererForDecoderByName(codec, params)) {
                    return true;
                }
            }
        }
        else {
            QList<const char *> knownHevcCodecs = { "hevc_rkmpp", "hevc_nvmpi", "hevc_v4l2m2m" };
            for (const char* codec : knownHevcCodecs) {
                if (tryInitializeRendererForDecoderByName(codec, params)) {
                    return true;
                }
            }
        }

        // Look for the first matching hwaccel hardware decoder (pass 1)
        // This picks up "second-tier" hwaccels like CUDA.
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
            if (!config) {
                // No remaing hwaccel options
                break;
            }

            // Initialize the hardware codec and submit a test frame if the renderer needs it
            if (tryInitializeRenderer(decoder, params, config,
                                      [config]() -> IFFmpegRenderer* { return createHwAccelRenderer(config, 1); })) {
                return true;
            }
        }
    }

    // Fallback to software if no matching hardware decoder was found
    // and if software fallback is allowed
    if (params->vds != StreamingPreferences::VDS_FORCE_HARDWARE) {
        if (tryInitializeRenderer(decoder, params, nullptr,
                                  []() -> IFFmpegRenderer* { return new SdlRenderer(); })) {
            return true;
        }
    }

    // No decoder worked
    return false;
}

void FFmpegVideoDecoder::writeBuffer(PLENTRY entry, int& offset)
{
    if (m_NeedsSpsFixup && entry->bufferType == BUFFER_TYPE_SPS) {
        const char naluHeader[] = {0x00, 0x00, 0x00, 0x01};
        h264_stream_t* stream = h264_new();
        int nalStart, nalEnd;

        // Read the old NALU
        find_nal_unit((uint8_t*)entry->data, entry->length, &nalStart, &nalEnd);
        read_nal_unit(stream,
                      (unsigned char *)&entry->data[nalStart],
                      nalEnd - nalStart);

        SDL_assert(nalStart == sizeof(naluHeader));
        SDL_assert(nalEnd == entry->length);

        // Fixup the SPS to what OS X needs to use hardware acceleration
        stream->sps->num_ref_frames = 1;
        stream->sps->vui.max_dec_frame_buffering = 1;

        int initialOffset = offset;

        // Copy the modified NALU data. This assumes a 3 byte prefix and
        // begins writing from the 2nd byte, so we must write the data
        // first, then go back and write the Annex B prefix.
        offset += write_nal_unit(stream, (uint8_t*)&m_DecodeBuffer.data()[initialOffset + 3],
                                 MAX_SPS_EXTRA_SIZE + entry->length - sizeof(naluHeader));

        // Copy the NALU prefix over from the original SPS
        memcpy(&m_DecodeBuffer.data()[initialOffset], naluHeader, sizeof(naluHeader));
        offset += sizeof(naluHeader);

        h264_free(stream);
    }
    else {
        // Write the buffer as-is
        memcpy(&m_DecodeBuffer.data()[offset],
               entry->data,
               entry->length);
        offset += entry->length;
    }
}

int FFmpegVideoDecoder::submitDecodeUnit(PDECODE_UNIT du)
{
    PLENTRY entry = du->bufferList;
    int err;

    SDL_assert(!m_TestOnly);

    if (!m_LastFrameNumber) {
        m_ActiveWndVideoStats.measurementStartTimestamp = SDL_GetTicks();
        m_LastFrameNumber = du->frameNumber;
    }
    else {
        // Any frame number greater than m_LastFrameNumber + 1 represents a dropped frame
        m_ActiveWndVideoStats.networkDroppedFrames += du->frameNumber - (m_LastFrameNumber + 1);
        m_ActiveWndVideoStats.totalFrames += du->frameNumber - (m_LastFrameNumber + 1);
        m_LastFrameNumber = du->frameNumber;
    }

    // Flip stats windows roughly every second
    if (SDL_TICKS_PASSED(SDL_GetTicks(), m_ActiveWndVideoStats.measurementStartTimestamp + 1000)) {
        // Update overlay stats if it's enabled
        if (Session::get()->getOverlayManager().isOverlayEnabled(Overlay::OverlayDebug)) {
            VIDEO_STATS lastTwoWndStats = {};
            addVideoStats(m_LastWndVideoStats, lastTwoWndStats);
            addVideoStats(m_ActiveWndVideoStats, lastTwoWndStats);

            stringifyVideoStats(lastTwoWndStats, Session::get()->getOverlayManager().getOverlayText(Overlay::OverlayDebug));
            Session::get()->getOverlayManager().setOverlayTextUpdated(Overlay::OverlayDebug);
        }

        // Accumulate these values into the global stats
        addVideoStats(m_ActiveWndVideoStats, m_GlobalVideoStats);

        // Move this window into the last window slot and clear it for next window
        SDL_memcpy(&m_LastWndVideoStats, &m_ActiveWndVideoStats, sizeof(m_ActiveWndVideoStats));
        SDL_zero(m_ActiveWndVideoStats);
        m_ActiveWndVideoStats.measurementStartTimestamp = SDL_GetTicks();
    }

    m_ActiveWndVideoStats.receivedFrames++;
    m_ActiveWndVideoStats.totalFrames++;

    int requiredBufferSize = du->fullLength;
    if (du->frameType == FRAME_TYPE_IDR) {
        // Add some extra space in case we need to do an SPS fixup
        requiredBufferSize += MAX_SPS_EXTRA_SIZE;
    }

    // Ensure the decoder buffer is large enough
    m_DecodeBuffer.reserve(requiredBufferSize + AV_INPUT_BUFFER_PADDING_SIZE);

    int offset = 0;
    while (entry != nullptr) {
        writeBuffer(entry, offset);
        entry = entry->next;
    }

    m_Pkt->data = reinterpret_cast<uint8_t*>(m_DecodeBuffer.data());
    m_Pkt->size = offset;

    if (du->frameType == FRAME_TYPE_IDR) {
        m_Pkt->flags = AV_PKT_FLAG_KEY;
    }
    else {
        m_Pkt->flags = 0;
    }

    m_ActiveWndVideoStats.totalReassemblyTime += du->enqueueTimeMs - du->receiveTimeMs;

    err = avcodec_send_packet(m_VideoDecoderCtx, m_Pkt);
    if (err < 0) {
        char errorstring[512];
        av_strerror(err, errorstring, sizeof(errorstring));
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "avcodec_send_packet() failed: %s", errorstring);

        // If we've failed a bunch of decodes in a row, the decoder/renderer is
        // clearly unhealthy, so let's generate a synthetic reset event to trigger
        // the event loop to destroy and recreate the decoder.
        if (++m_ConsecutiveFailedDecodes == FAILED_DECODES_RESET_THRESHOLD) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Resetting decoder due to consistent failure");

            SDL_Event event;
            event.type = SDL_RENDER_DEVICE_RESET;
            SDL_PushEvent(&event);
        }

        return DR_NEED_IDR;
    }

    m_FramesIn++;

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        // Failed to allocate a frame but we did submit,
        // so we can return DR_OK
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to allocate frame");
        return DR_OK;
    }

    err = avcodec_receive_frame(m_VideoDecoderCtx, frame);
    if (err == 0) {
        m_FramesOut++;

        // Reset failed decodes count if we reached this far
        m_ConsecutiveFailedDecodes = 0;

        // Restore default log level after a successful decode
        av_log_set_level(AV_LOG_INFO);

        // Store the presentation time
        frame->pts = du->presentationTimeMs;

        // Capture a frame timestamp to measuring pacing delay
        frame->pkt_dts = SDL_GetTicks();

        // Count time in avcodec_send_packet() and avcodec_receive_frame()
        // as time spent decoding. Also count time spent in the decode unit
        // queue because that's directly caused by decoder latency.
        m_ActiveWndVideoStats.totalDecodeTime += LiGetMillis() - du->enqueueTimeMs;

        // Also count the frame-to-frame delay if the decoder is delaying frames
        // until a subsequent frame is submitted.
        m_ActiveWndVideoStats.totalDecodeTime += (m_FramesIn - m_FramesOut) * (1000 / m_StreamFps);

        m_ActiveWndVideoStats.decodedFrames++;

        // Queue the frame for rendering (or render now if pacer is disabled)
        m_Pacer->submitFrame(frame);
    }
    else {
        av_frame_free(&frame);

        char errorstring[512];
        av_strerror(err, errorstring, sizeof(errorstring));
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "avcodec_receive_frame() failed: %s", errorstring);

        if (++m_ConsecutiveFailedDecodes == FAILED_DECODES_RESET_THRESHOLD) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Resetting decoder due to consistent failure");

            SDL_Event event;
            event.type = SDL_RENDER_DEVICE_RESET;
            SDL_PushEvent(&event);
        }
    }

    return DR_OK;
}

void FFmpegVideoDecoder::renderFrameOnMainThread()
{
    m_Pacer->renderOnMainThread();
}

