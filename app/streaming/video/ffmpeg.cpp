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

// This is gross but it allows us to use sizeof()
#include "ffmpeg_videosamples.cpp"

#define MAX_SPS_EXTRA_SIZE 16

#define FAILED_DECODES_RESET_THRESHOLD 20

bool FFmpegVideoDecoder::isHardwareAccelerated()
{
    return m_HwDecodeCfg != nullptr ||
            (m_VideoDecoderCtx->codec->capabilities & AV_CODEC_CAP_HARDWARE) != 0;
}

int FFmpegVideoDecoder::getDecoderCapabilities()
{
    return m_BackendRenderer->getDecoderCapabilities();
}

enum AVPixelFormat FFmpegVideoDecoder::ffGetFormat(AVCodecContext* context,
                                                   const enum AVPixelFormat* pixFmts)
{
    FFmpegVideoDecoder* decoder = (FFmpegVideoDecoder*)context->opaque;
    const enum AVPixelFormat *p;

    for (p = pixFmts; *p != -1; p++) {
        // Only match our hardware decoding codec or SW pixel
        // format (if not using hardware decoding). It's crucial
        // to override the default get_format() which will try
        // to gracefully fall back to software decode and break us.
        if (*p == (decoder->m_HwDecodeCfg ?
                   decoder->m_HwDecodeCfg->pix_fmt :
                   context->pix_fmt)) {
            return *p;
        }
    }

    return AV_PIX_FMT_NONE;
}

FFmpegVideoDecoder::FFmpegVideoDecoder(bool testOnly)
    : m_VideoDecoderCtx(nullptr),
      m_DecodeBuffer(1024 * 1024, 0),
      m_HwDecodeCfg(nullptr),
      m_BackendRenderer(nullptr),
      m_FrontendRenderer(nullptr),
      m_ConsecutiveFailedDecodes(0),
      m_Pacer(nullptr),
      m_LastFrameNumber(0),
      m_StreamFps(0),
      m_NeedsSpsFixup(false),
      m_TestOnly(testOnly)
{
    av_init_packet(&m_Pkt);

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

bool FFmpegVideoDecoder::createFrontendRenderer(PDECODER_PARAMETERS params)
{
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

    // Determine whether the frontend renderer prefers frame pacing
    auto vsyncConstraint = m_FrontendRenderer->getFramePacingConstraint();
    if (vsyncConstraint == IFFmpegRenderer::PACING_FORCE_OFF && params->enableFramePacing) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Frame pacing is forcefully disabled by the frontend renderer");
        params->enableFramePacing = false;
    }
    else if (vsyncConstraint == IFFmpegRenderer::PACING_FORCE_ON && !params->enableFramePacing) {
        // FIXME: This duplicates logic in Session.cpp
        int displayHz = StreamUtils::getDisplayRefreshRate(params->window);
        if (displayHz + 5 >= params->frameRate) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Frame pacing is forcefully enabled by the frontend renderer");
            params->enableFramePacing = true;
        }
    }

    return true;
}

bool FFmpegVideoDecoder::completeInitialization(AVCodec* decoder, PDECODER_PARAMETERS params, bool testFrame)
{
    // In test-only mode, we should only see test frames
    SDL_assert(!m_TestOnly || testFrame);

    // Create the frontend renderer based on the capabilities of the backend renderer
    if (!createFrontendRenderer(params)) {
        return false;
    }

    m_StreamFps = params->frameRate;

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

    // Allow the backend renderer to attach data to this decoder
    if (!m_BackendRenderer->prepareDecoderContext(m_VideoDecoderCtx)) {
        return false;
    }

    // Nobody must override our ffGetFormat
    SDL_assert(m_VideoDecoderCtx->get_format == ffGetFormat);

    // Stash a pointer to this object in the context
    SDL_assert(m_VideoDecoderCtx->opaque == nullptr);
    m_VideoDecoderCtx->opaque = this;

    int err = avcodec_open2(m_VideoDecoderCtx, decoder, nullptr);
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
            m_Pkt.data = (uint8_t*)k_H264TestFrame;
            m_Pkt.size = sizeof(k_H264TestFrame);
            break;
        case VIDEO_FORMAT_H265:
            m_Pkt.data = (uint8_t*)k_HEVCMainTestFrame;
            m_Pkt.size = sizeof(k_HEVCMainTestFrame);
            break;
        case VIDEO_FORMAT_H265_MAIN10:
            m_Pkt.data = (uint8_t*)k_HEVCMain10TestFrame;
            m_Pkt.size = sizeof(k_HEVCMain10TestFrame);
            break;
        default:
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "No test frame for format: %x",
                         params->videoFormat);
            return false;
        }

        err = avcodec_send_packet(m_VideoDecoderCtx, &m_Pkt);
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

    // Start with an empty string
    output[offset] = 0;

    if (stats.receivedFps > 0) {
        offset += sprintf(&output[offset],
                          "Estimated host PC frame rate: %.2f FPS\n"
                          "Incoming frame rate from network: %.2f FPS\n"
                          "Decoding frame rate: %.2f FPS\n"
                          "Rendering frame rate: %.2f FPS\n",
                          stats.totalFps,
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
    m_BackendRenderer = createRendererFunc();
    m_HwDecodeCfg = hwConfig;

    if (m_BackendRenderer != nullptr &&
            m_BackendRenderer->initialize(params) &&
            completeInitialization(decoder, params, m_TestOnly || m_BackendRenderer->needsTestFrame())) {
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
                    completeInitialization(decoder, params, false)) {
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
        // Failed to initialize or test frame failed, so keep looking
        reset();
    }

    return false;
}

bool FFmpegVideoDecoder::initialize(PDECODER_PARAMETERS params)
{
    AVCodec* decoder;

    // Increase log level until the first frame is decoded
    av_log_set_level(AV_LOG_DEBUG);

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

#ifdef HAVE_MMAL
        // MMAL is the decoder for the Raspberry Pi
        if (params->videoFormat & VIDEO_FORMAT_MASK_H264) {
            AVCodec* mmalDecoder = avcodec_find_decoder_by_name("h264_mmal");
            if (mmalDecoder != nullptr &&
                    tryInitializeRenderer(mmalDecoder, params, nullptr,
                                          []() -> IFFmpegRenderer* { return new MmalRenderer(); })) {
                return true;
            }
        }
#endif

#ifdef HAVE_DRM
        // RKMPP is a hardware accelerated decoder that outputs DRI PRIME buffers
        AVCodec* rkmppDecoder;

        if (params->videoFormat & VIDEO_FORMAT_MASK_H264) {
            rkmppDecoder = avcodec_find_decoder_by_name("h264_rkmpp");
        }
        else {
            rkmppDecoder = avcodec_find_decoder_by_name("hevc_rkmpp");
        }

        if (rkmppDecoder != nullptr &&
                tryInitializeRenderer(rkmppDecoder, params, nullptr,
                                      []() -> IFFmpegRenderer* { return new DrmRenderer(); })) {
            return true;
        }
#endif

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

    m_Pkt.data = reinterpret_cast<uint8_t*>(m_DecodeBuffer.data());
    m_Pkt.size = offset;

    m_ActiveWndVideoStats.totalReassemblyTime += LiGetMillis() - du->receiveTimeMs;

    Uint32 beforeDecode = SDL_GetTicks();

    err = avcodec_send_packet(m_VideoDecoderCtx, &m_Pkt);
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
        // Reset failed decodes count if we reached this far
        m_ConsecutiveFailedDecodes = 0;

        // Restore default log level after a successful decode
        av_log_set_level(AV_LOG_INFO);

        // Store the presentation time
        frame->pts = du->presentationTimeMs;

        // Capture a frame timestamp to measuring pacing delay
        frame->pkt_dts = SDL_GetTicks();

        // Count time in avcodec_send_packet() and avcodec_receive_frame()
        // as time spent decoding
        m_ActiveWndVideoStats.totalDecodeTime += SDL_GetTicks() - beforeDecode;
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

