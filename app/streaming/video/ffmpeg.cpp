#include <Limelight.h>
#include "ffmpeg.h"
#include "streaming/streamutils.h"
#include "streaming/session.h"

#include <h264_stream.h>

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

// This is gross but it allows us to use sizeof()
#include "ffmpeg_videosamples.cpp"

#define MAX_SPS_EXTRA_SIZE 16

#define FAILED_DECODES_RESET_THRESHOLD 20

bool FFmpegVideoDecoder::isHardwareAccelerated()
{
    return m_HwDecodeCfg != nullptr;
}

int FFmpegVideoDecoder::getDecoderCapabilities()
{
    return m_Renderer->getDecoderCapabilities();
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

FFmpegVideoDecoder::FFmpegVideoDecoder()
    : m_VideoDecoderCtx(nullptr),
      m_DecodeBuffer(1024 * 1024, 0),
      m_HwDecodeCfg(nullptr),
      m_Renderer(nullptr),
      m_ConsecutiveFailedDecodes(0),
      m_Pacer(nullptr),
      m_LastFrameNumber(0),
      m_StreamFps(0),
      m_NeedsSpsFixup(false)
{
    av_init_packet(&m_Pkt);
    SDL_AtomicSet(&m_QueuedFrames, 0);

    SDL_zero(m_ActiveWndVideoStats);
    SDL_zero(m_LastWndVideoStats);
    SDL_zero(m_GlobalVideoStats);

    // Use linear filtering when renderer scaling is required
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
}

FFmpegVideoDecoder::~FFmpegVideoDecoder()
{
    reset();
}

IFFmpegRenderer* FFmpegVideoDecoder::getRenderer()
{
    return m_Renderer;
}

void FFmpegVideoDecoder::reset()
{
    // Drop any frames still queued to ensure
    // they are properly freed.
    SDL_Event event;

    while (SDL_AtomicGet(&m_QueuedFrames) > 0) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Waiting for %d frames to return",
                    SDL_AtomicGet(&m_QueuedFrames));

        if (SDL_PeepEvents(&event,
                           1,
                           SDL_GETEVENT,
                           SDL_USEREVENT,
                           SDL_USEREVENT) == 1) {
            dropFrame(&event.user);
        }
        else {
            SDL_Delay(10);
            SDL_PumpEvents();
        }
    }

    delete m_Pacer;
    m_Pacer = nullptr;

    // This must be called after deleting Pacer because it
    // may be holding AVFrames to free in its destructor.
    // However, it must be called before deleting the IFFmpegRenderer
    // since the codec context may be referencing objects that we
    // need to delete in the renderer destructor.
    avcodec_free_context(&m_VideoDecoderCtx);

    delete m_Renderer;
    m_Renderer = nullptr;

    logVideoStats(m_GlobalVideoStats, "Global video stats");
}

bool FFmpegVideoDecoder::completeInitialization(AVCodec* decoder, SDL_Window* window,
                                                int videoFormat, int width, int height,
                                                int maxFps, bool enableFramePacing, bool testOnly)
{
    auto vsyncConstraint = m_Renderer->getFramePacingConstraint();
    if (vsyncConstraint == IFFmpegRenderer::PACING_FORCE_OFF && enableFramePacing) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Frame pacing is forcefully disabled by the active renderer");
        enableFramePacing = false;
    }
    else if (vsyncConstraint == IFFmpegRenderer::PACING_FORCE_ON && !enableFramePacing) {
        // FIXME: This duplicates logic in Session.cpp
        int displayHz = StreamUtils::getDisplayRefreshRate(window);
        if (displayHz + 5 >= maxFps) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Frame pacing is forcefully enabled by the active renderer");
            enableFramePacing = true;
        }
    }

    m_StreamFps = maxFps;
    m_Pacer = new Pacer(m_Renderer, &m_ActiveWndVideoStats);
    if (!m_Pacer->initialize(window, maxFps, enableFramePacing)) {
        return false;
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

    // Enable slice multi-threading for software decoding
    if (!m_HwDecodeCfg) {
        m_VideoDecoderCtx->thread_type = FF_THREAD_SLICE;
        m_VideoDecoderCtx->thread_count = qMin(MAX_SLICES, SDL_GetCPUCount());
    }
    else {
        // No threading for HW decode
        m_VideoDecoderCtx->thread_count = 1;
    }

    // Setup decoding parameters
    m_VideoDecoderCtx->width = width;
    m_VideoDecoderCtx->height = height;
    m_VideoDecoderCtx->pix_fmt = AV_PIX_FMT_YUV420P; // FIXME: HDR
    m_VideoDecoderCtx->get_format = ffGetFormat;

    // Allow the renderer to attach data to this decoder
    if (!m_Renderer->prepareDecoderContext(m_VideoDecoderCtx)) {
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
                     videoFormat);
        return false;
    }

    // FFMpeg doesn't completely initialize the codec until the codec
    // config data comes in. This would be too late for us to change
    // our minds on the selected video codec, so we'll do a trial run
    // now to see if things will actually work when the video stream
    // comes in.
    if (testOnly) {
        if (videoFormat & VIDEO_FORMAT_MASK_H264) {
            m_Pkt.data = (uint8_t*)k_H264TestFrame;
            m_Pkt.size = sizeof(k_H264TestFrame);
        }
        else {
            m_Pkt.data = (uint8_t*)k_HEVCTestFrame;
            m_Pkt.size = sizeof(k_HEVCTestFrame);
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
        if ((videoFormat & VIDEO_FORMAT_MASK_H264) &&
                !(m_Renderer->getDecoderCapabilities() & CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC)) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Using H.264 SPS fixup");
            m_NeedsSpsFixup = true;
        }
        else {
            m_NeedsSpsFixup = false;
        }
    }

#ifdef QT_DEBUG
    // Restore default log level before streaming
    av_log_set_level(AV_LOG_INFO);
#endif

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
                          "Frames dropped by frame pacing: %.2f%%\n"
                          "Average receive time: %.2f ms\n"
                          "Average decoding time: %.2f ms\n"
                          "Average frame queue delay: %.2f ms\n"
                          "Average rendering time: %.2f ms\n",
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

IFFmpegRenderer* FFmpegVideoDecoder::createAcceleratedRenderer(const AVCodecHWConfig* hwDecodeCfg)
{
    if (!(hwDecodeCfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
        return nullptr;
    }

    // Look for acceleration types we support
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

bool FFmpegVideoDecoder::initialize(
        StreamingPreferences::VideoDecoderSelection vds,
        SDL_Window* window,
        int videoFormat,
        int width,
        int height,
        int maxFps,
        bool enableVsync,
        bool enableFramePacing)
{
    AVCodec* decoder;

#ifdef QT_DEBUG
    // Increase log level during initialization
    av_log_set_level(AV_LOG_DEBUG);
#endif

    if (videoFormat & VIDEO_FORMAT_MASK_H264) {
        decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    }
    else if (videoFormat & VIDEO_FORMAT_MASK_H265) {
        decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    }
    else {
        Q_ASSERT(false);
        decoder = nullptr;
    }

    if (!decoder) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to find decoder for format: %x",
                     videoFormat);
        return false;
    }

    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
        if (!config || vds == StreamingPreferences::VDS_FORCE_SOFTWARE) {
            // No matching hardware acceleration support.
            // This is not an error.
            m_HwDecodeCfg = nullptr;
            m_Renderer = new SdlRenderer();
            if (vds != StreamingPreferences::VDS_FORCE_HARDWARE &&
                    m_Renderer->initialize(window, videoFormat, width, height, maxFps, enableVsync) &&
                    completeInitialization(decoder, window, videoFormat, width, height, maxFps, enableFramePacing, false)) {
                return true;
            }
            else {
                reset();
                return false;
            }
        }

        m_Renderer = createAcceleratedRenderer(config);
        if (!m_Renderer) {
            continue;
        }

        m_HwDecodeCfg = config;
        // Initialize the hardware codec and submit a test frame if the renderer needs it
        if (m_Renderer->initialize(window, videoFormat, width, height, maxFps, enableVsync) &&
                completeInitialization(decoder, window, videoFormat, width, height, maxFps, enableFramePacing, m_Renderer->needsTestFrame())) {
            if (m_Renderer->needsTestFrame()) {
                // The test worked, so now let's initialize it for real
                reset();
                if ((m_Renderer = createAcceleratedRenderer(config)) != nullptr &&
                        m_Renderer->initialize(window, videoFormat, width, height, maxFps, enableVsync) &&
                        completeInitialization(decoder, window, videoFormat, width, height, maxFps, enableFramePacing, false)) {
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
    }
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
        if (Session::get()->getOverlayManager().isOverlayEnabled(OverlayManager::OverlayDebug)) {
            VIDEO_STATS lastTwoWndStats = {};
            addVideoStats(m_LastWndVideoStats, lastTwoWndStats);
            addVideoStats(m_ActiveWndVideoStats, lastTwoWndStats);

            stringifyVideoStats(lastTwoWndStats, Session::get()->getOverlayManager().getOverlayText(OverlayManager::OverlayDebug));
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
    if (requiredBufferSize + AV_INPUT_BUFFER_PADDING_SIZE > m_DecodeBuffer.length()) {
        m_DecodeBuffer = QByteArray(requiredBufferSize + AV_INPUT_BUFFER_PADDING_SIZE, 0);
    }

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

        // Capture a frame timestamp to measuring pacing delay
        frame->pts = SDL_GetTicks();

        // Queue the frame for rendering from the main thread
        SDL_AtomicIncRef(&m_QueuedFrames);
        queueFrame(frame);

        // Count time in avcodec_send_packet() and avcodec_receive_frame()
        // as time spent decoding
        m_ActiveWndVideoStats.totalDecodeTime += SDL_GetTicks() - beforeDecode;
        m_ActiveWndVideoStats.decodedFrames++;
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

// Called on main thread
void FFmpegVideoDecoder::renderFrame(SDL_UserEvent* event)
{
    AVFrame* frame = reinterpret_cast<AVFrame*>(event->data1);
    m_Pacer->submitFrame(frame);
    SDL_AtomicDecRef(&m_QueuedFrames);
}

// Called on main thread
void FFmpegVideoDecoder::dropFrame(SDL_UserEvent* event)
{
    AVFrame* frame = reinterpret_cast<AVFrame*>(event->data1);
    // If Pacer is using a frame queue, we can just queue the
    // frame and let it decide whether to drop or not. If Pacer
    // is submitting directly for rendering, we drop the frame
    // ourselves.
    if (m_Pacer->isUsingFrameQueue()) {
        m_Pacer->submitFrame(frame);
    }
    else {
        av_frame_free(&frame);

        // Since Pacer won't see this frame, we'll mark it as a drop
        // on its behalf
        m_ActiveWndVideoStats.pacerDroppedFrames++;
    }
    SDL_AtomicDecRef(&m_QueuedFrames);
}
