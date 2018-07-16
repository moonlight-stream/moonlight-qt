#include <Limelight.h>
#include "session.hpp"

#ifdef _WIN32
#include "renderers/dxva2.h"
#endif

#ifdef __APPLE__
#include "renderers/vt.h"
#endif

AVPacket Session::s_Pkt;
AVCodecContext* Session::s_VideoDecoderCtx;
QByteArray Session::s_DecodeBuffer;
const AVCodecHWConfig* Session::s_HwDecodeCfg;
IRenderer* Session::s_Renderer;

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

bool Session::chooseDecoder(StreamingPreferences::VideoDecoderSelection vds,
                            SDL_Window* window,
                            int videoFormat,
                            int width, int height,
                            AVCodec*& chosenDecoder,
                            const AVCodecHWConfig*& chosenHwConfig,
                            IRenderer*& newRenderer)
{
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
        if (!config || vds == StreamingPreferences::VDS_FORCE_SOFTWARE) {
            // No matching hardware acceleration support.
            // This is not an error.
            chosenHwConfig = nullptr;
            newRenderer = new SdlRenderer();
            if (vds != StreamingPreferences::VDS_FORCE_HARDWARE &&
                    newRenderer->initialize(window, videoFormat, width, height)) {
                return true;
            }
            else {
                delete newRenderer;
                newRenderer = nullptr;
                return false;
            }
        }

        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
            continue;
        }

        // Look for acceleration types we support
        switch (config->device_type) {
#ifdef _WIN32
        case AV_HWDEVICE_TYPE_DXVA2:
            newRenderer = new DXVA2Renderer();
            break;
#endif
#ifdef __APPLE__
        case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
            newRenderer = VTRendererFactory::createRenderer();
            break;
#endif
        default:
            continue;
        }

        if (newRenderer->initialize(window, videoFormat, width, height)) {
            chosenHwConfig = config;
            return true;
        }
        else {
            // Failed to initialize
            delete newRenderer;
            newRenderer = nullptr;
        }
    }
}

bool Session::isHardwareDecodeAvailable(
        StreamingPreferences::VideoDecoderSelection vds,
        int videoFormat, int width, int height)
{
    AVCodec* decoder;
    const AVCodecHWConfig* hwConfig;
    IRenderer* renderer;

    // Create temporary window to instantiate the decoder
    SDL_Window* window = SDL_CreateWindow("", 0, 0, width, height, SDL_WINDOW_HIDDEN);
    if (!window) {
        return false;
    }

    if (chooseDecoder(vds, window, videoFormat, width, height, decoder, hwConfig, renderer)) {
        // The renderer may have referenced the window, so
        // we must delete the renderer before the window.
        delete renderer;

        SDL_DestroyWindow(window);
        return hwConfig != nullptr;
    }
    else {
        SDL_DestroyWindow(window);
        // Failed to find *any* decoder, including software
        return false;
    }
}

int Session::drSetup(int videoFormat, int width, int height, int /* frameRate */, void*, int)
{
    AVCodec* decoder;

    // Use linear filtering when renderer scaling is required
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    av_init_packet(&s_Pkt);

    if (!chooseDecoder(s_ActiveSession->m_Preferences.videoDecoderSelection,
                       s_ActiveSession->m_Window,
                       videoFormat, width, height,
                       decoder, s_HwDecodeCfg, s_Renderer)) {
        // Error logged in chooseDecoder()
        return -1;
    }

    s_VideoDecoderCtx = avcodec_alloc_context3(decoder);
    if (!s_VideoDecoderCtx) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to allocate video decoder context");
        delete s_Renderer;
        return -1;
    }

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

    // Allow the renderer to attach data to this decoder
    if (!s_Renderer->prepareDecoderContext(s_VideoDecoderCtx)) {
        delete s_Renderer;
        av_free(s_VideoDecoderCtx);
        return -1;
    }

    int err = avcodec_open2(s_VideoDecoderCtx, decoder, nullptr);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to open decoder for format: %x",
                     videoFormat);
        delete s_Renderer;
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

    delete s_Renderer;
    s_Renderer = nullptr;
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
    s_Renderer->renderFrame(frame);
    av_frame_free(&frame);
}

// Called on main thread
void Session::dropFrame(SDL_UserEvent* event)
{
    AVFrame* frame = reinterpret_cast<AVFrame*>(event->data1);
    av_frame_free(&frame);
}
