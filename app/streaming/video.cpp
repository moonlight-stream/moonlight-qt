#include <Limelight.h>
#include "session.hpp"

AVPacket Session::s_Pkt;
AVCodecContext* Session::s_VideoDecoderCtx;
QByteArray Session::s_DecodeBuffer;
AVBufferRef* Session::s_HwDeviceCtx;
const AVCodecHWConfig* Session::s_HwDecodeCfg;

#ifdef _WIN32
#include <D3D9.h>
#endif

#define MAX_SLICES 4

int Session::getDecoderCapabilities()
{
    int caps = 0;

    // Submit for decode without using a separate thread
    caps |= CAPABILITY_DIRECT_SUBMIT;

    // Slice up to 4 times for parallel decode, once slice per core
    caps |= CAPABILITY_SLICES_PER_FRAME(qMin(MAX_SLICES, SDL_GetCPUCount()));

    return caps;
}

bool Session::chooseDecoder(int videoFormat,
                            AVCodec*& chosenDecoder,
                            const AVCodecHWConfig*& chosenHwConfig,
                            AVBufferRef*& newHwContext)
{
    int err;

    if (videoFormat & VIDEO_FORMAT_MASK_H264) {
        chosenDecoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    }
    else if (videoFormat & VIDEO_FORMAT_MASK_H265) {
        chosenDecoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    }
    else {
        Q_ASSERT(false);
        chosenDecoder = nullptr;
    }

    if (!chosenDecoder) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to find decoder for format: %x",
                     videoFormat);
        return false;
    }

    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(chosenDecoder, i);
        if (!config) {
            // No matching hardware acceleration support.
            // This is not an error.
            chosenHwConfig = nullptr;
            newHwContext = nullptr;
            return true;
        }

        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
            continue;
        }

        // Look for acceleration types we support
        switch (config->device_type) {
        case AV_HWDEVICE_TYPE_VAAPI:
        case AV_HWDEVICE_TYPE_VDPAU:
        case AV_HWDEVICE_TYPE_DXVA2:
            // Ensure we can actually use this decoder. FFmpeg may
            // have returned decoders it thinks we can use but they
            // may not really be usable when it goes to load the libraries.
            err = av_hwdevice_ctx_create(&newHwContext,
                                         config->device_type,
                                         nullptr,
                                         nullptr,
                                         0);
            if (err < 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Failed to initialize hardware decoder: %d %x",
                             config->device_type,
                             err);
                continue;
            }

            // FIXME: Validate profiles (free newHwContext on failure)

            chosenHwConfig = config;
            return true;
        default:
            continue;
        }
    }
}

bool Session::isHardwareDecodeAvailable(int videoFormat)
{
    AVCodec* decoder;
    const AVCodecHWConfig* hwConfig;
    AVBufferRef* hwCtx;

    if (chooseDecoder(videoFormat, decoder, hwConfig, hwCtx)) {
        if (hwCtx != nullptr) {
            // Free the context, since we don't actually need it
            av_buffer_unref(&hwCtx);
            return true;
        }
        else {
            // Didn't find a working HW codec
            return false;
        }
    }
    else {
        // Failed to find *any* decoder, including software
        return false;
    }
}

enum AVPixelFormat Session::getHwFormat(AVCodecContext*,
                                        const enum AVPixelFormat* pixFmts)
{
    const enum AVPixelFormat *p;

    for (p = pixFmts; *p != -1; p++) {
        if (*p == s_HwDecodeCfg->pix_fmt)
            return *p;
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to find HW surface format");
    return AV_PIX_FMT_NONE;
}

int Session::drSetup(int videoFormat, int width, int height, int /* frameRate */, void*, int)
{
    AVCodec* decoder;

    av_init_packet(&s_Pkt);

    if (!chooseDecoder(videoFormat, decoder, s_HwDecodeCfg, s_HwDeviceCtx)) {
        // Error logged in chooseDecoder()
        return -1;
    }

    // These will be cleaned up by the Session class outside
    // of our cleanup routine.
    s_ActiveSession->m_Renderer = SDL_CreateRenderer(s_ActiveSession->m_Window, -1,
                                                     SDL_RENDERER_ACCELERATED);
    if (!s_ActiveSession->m_Renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
        return -1;
    }

    s_ActiveSession->m_Texture = SDL_CreateTexture(s_ActiveSession->m_Renderer,
                                                   SDL_PIXELFORMAT_YV12,
                                                   SDL_TEXTUREACCESS_STREAMING,
                                                   width,
                                                   height);
    if (!s_ActiveSession->m_Texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
        return -1;
    }

    s_VideoDecoderCtx = avcodec_alloc_context3(decoder);
    if (!s_VideoDecoderCtx) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to allocate video decoder context");
        return -1;
    }

    // Pass our ownership of our reference to the decoder context (if we have one)
    s_VideoDecoderCtx->hw_device_ctx = s_HwDeviceCtx;

    // Always request low delay decoding
    s_VideoDecoderCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    // Enable slice multi-threading for software decoding
    if (!s_HwDecodeCfg) {
        s_VideoDecoderCtx->thread_type = FF_THREAD_SLICE;
        s_VideoDecoderCtx->thread_count = qMin(MAX_SLICES, SDL_GetCPUCount());
    }
    else {
        // No threading for HW decode
        s_VideoDecoderCtx->thread_count = 1;
    }

    // Setup decoding parameters
    s_VideoDecoderCtx->width = width;
    s_VideoDecoderCtx->height = height;
    s_VideoDecoderCtx->pix_fmt = AV_PIX_FMT_YUV420P; // FIXME: HDR

    // Attach hardware decoding callbacks
    if (s_HwDecodeCfg != nullptr) {
        s_VideoDecoderCtx->get_format = getHwFormat;
    }

    int err = avcodec_open2(s_VideoDecoderCtx, decoder, nullptr);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to open decoder for format: %x",
                     videoFormat);
        av_free(s_VideoDecoderCtx);
        return -1;
    }

    // 1MB frame buffer to start
    s_DecodeBuffer = QByteArray(1024 * 1024, 0);

    return 0;
}

void Session::drCleanup()
{
    avcodec_close(s_VideoDecoderCtx);
    av_free(s_VideoDecoderCtx);
    s_VideoDecoderCtx = nullptr;

    s_HwDecodeCfg = nullptr;
}

int Session::drSubmitDecodeUnit(PDECODE_UNIT du)
{
    PLENTRY entry = du->bufferList;
    int err;

    if (du->fullLength + AV_INPUT_BUFFER_PADDING_SIZE > s_DecodeBuffer.length()) {
        s_DecodeBuffer = QByteArray(du->fullLength + AV_INPUT_BUFFER_PADDING_SIZE, 0);
    }

    int offset = 0;
    while (entry != nullptr) {
        memcpy(&s_DecodeBuffer.data()[offset],
               entry->data,
               entry->length);
        offset += entry->length;
        entry = entry->next;
    }

    SDL_assert(offset == du->fullLength);

    s_Pkt.data = reinterpret_cast<uint8_t*>(s_DecodeBuffer.data());
    s_Pkt.size = du->fullLength;

    err = avcodec_send_packet(s_VideoDecoderCtx, &s_Pkt);
    if (err < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Decoding failed: %d", err);
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

    err = avcodec_receive_frame(s_VideoDecoderCtx, frame);
    if (err == 0) {
        SDL_Event event;

        event.type = SDL_USEREVENT;
        event.user.code = SDL_CODE_FRAME_READY;
        event.user.data1 = frame;

        // The main thread will handle freeing this
        SDL_PushEvent(&event);
    }
    else {
        av_frame_free(&frame);
    }

    return DR_OK;
}

// Called on main thread
void Session::renderFrame(SDL_UserEvent* event)
{
    AVFrame* frame = reinterpret_cast<AVFrame*>(event->data1);

    SDL_UpdateYUVTexture(m_Texture, nullptr,
                         frame->data[0],
                         frame->linesize[0],
                         frame->data[1],
                         frame->linesize[1],
                         frame->data[2],
                         frame->linesize[2]);
    SDL_RenderClear(m_Renderer);
    SDL_RenderCopy(m_Renderer, m_Texture, nullptr, nullptr);
    SDL_RenderPresent(m_Renderer);

    av_frame_free(&frame);
}

// Called on main thread
void Session::dropFrame(SDL_UserEvent* event)
{
    AVFrame* frame = reinterpret_cast<AVFrame*>(event->data1);
    av_frame_free(&frame);
}
