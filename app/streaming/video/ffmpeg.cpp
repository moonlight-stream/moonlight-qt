#include <Limelight.h>
#include "ffmpeg.h"

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

bool FFmpegVideoDecoder::isHardwareAccelerated()
{
    return m_HwDecodeCfg != nullptr;
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
      m_Renderer(nullptr)
{
    av_init_packet(&m_Pkt);
    SDL_AtomicSet(&m_QueuedFrames, 0);

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
            SDL_Delay(100);
            SDL_PumpEvents();
        }
    }

    avcodec_free_context(&m_VideoDecoderCtx);

    delete m_Renderer;
    m_Renderer = nullptr;
}

bool FFmpegVideoDecoder::completeInitialization(AVCodec* decoder, int videoFormat, int width, int height, bool testOnly)
{
    m_VideoDecoderCtx = avcodec_alloc_context3(decoder);
    if (!m_VideoDecoderCtx) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to allocate video decoder context");
        return false;
    }

    // Always request low delay decoding
    m_VideoDecoderCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;

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

#ifdef QT_DEBUG
    // Restore default log level before streaming
    av_log_set_level(AV_LOG_INFO);
#endif

    return true;
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
        int)
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
                    m_Renderer->initialize(window, videoFormat, width, height) &&
                    completeInitialization(decoder, videoFormat, width, height, false)) {
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
        // Submit test frame to ensure this codec really works
        if (m_Renderer->initialize(window, videoFormat, width, height) &&
                completeInitialization(decoder, videoFormat, width, height, true)) {
            // OK, it worked, so now let's initialize it for real
            reset();
            if ((m_Renderer = createAcceleratedRenderer(config)) != nullptr &&
                    m_Renderer->initialize(window, videoFormat, width, height) &&
                    completeInitialization(decoder, videoFormat, width, height, false)) {
                return true;
            }
            else {
                SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION,
                                "Decoder failed to initialize after successful test");
                reset();
            }
        }
        else {
            // Failed to initialize or test frame failed, so keep looking
            reset();
        }
    }
}

int FFmpegVideoDecoder::submitDecodeUnit(PDECODE_UNIT du)
{
    PLENTRY entry = du->bufferList;
    int err;

    if (du->fullLength + AV_INPUT_BUFFER_PADDING_SIZE > m_DecodeBuffer.length()) {
        m_DecodeBuffer = QByteArray(du->fullLength + AV_INPUT_BUFFER_PADDING_SIZE, 0);
    }

    int offset = 0;
    while (entry != nullptr) {
        memcpy(&m_DecodeBuffer.data()[offset],
               entry->data,
               entry->length);
        offset += entry->length;
        entry = entry->next;
    }

    SDL_assert(offset == du->fullLength);

    m_Pkt.data = reinterpret_cast<uint8_t*>(m_DecodeBuffer.data());
    m_Pkt.size = du->fullLength;

    err = avcodec_send_packet(m_VideoDecoderCtx, &m_Pkt);
    if (err < 0) {
        char errorstring[512];
        av_strerror(err, errorstring, sizeof(errorstring));
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Decoding failed: %s", errorstring);
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
        // Queue the frame for rendering from the main thread
        SDL_AtomicIncRef(&m_QueuedFrames);
        queueFrame(frame);
    }
    else {
        av_frame_free(&frame);
    }

    return DR_OK;
}

// Called on main thread
void FFmpegVideoDecoder::renderFrame(SDL_UserEvent* event)
{
    AVFrame* frame = reinterpret_cast<AVFrame*>(event->data1);
    m_Renderer->renderFrame(frame);
    SDL_AtomicDecRef(&m_QueuedFrames);
}

// Called on main thread
void FFmpegVideoDecoder::dropFrame(SDL_UserEvent* event)
{
    AVFrame* frame = reinterpret_cast<AVFrame*>(event->data1);
    av_frame_free(&frame);
    SDL_AtomicDecRef(&m_QueuedFrames);
}
