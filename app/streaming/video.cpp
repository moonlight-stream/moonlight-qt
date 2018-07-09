#include <Limelight.h>
#include "session.hpp"

AVPacket Session::s_Pkt;
AVCodecContext* Session::s_VideoDecoderCtx;
QByteArray Session::s_DecodeBuffer;

#define MAX_SLICES 4

int Session::getDecoderCapabilities()
{
    int caps = 0;

    // Submit for decode without using a separate thread
    caps |= CAPABILITY_DIRECT_SUBMIT;

    // Slice up to 4 times for parallel decode, once slice per core
    caps |= CAPABILITY_SLICES_PER_FRAME(std::min(MAX_SLICES, SDL_GetCPUCount()));

    return caps;
}

int Session::drSetup(int videoFormat, int width, int height, int /* frameRate */, void*, int)
{
    AVCodec* decoder;

    av_init_packet(&s_Pkt);

    switch (videoFormat) {
    case VIDEO_FORMAT_H264:
        decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
        break;
    case VIDEO_FORMAT_H265:
    case VIDEO_FORMAT_H265_MAIN10:
        decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
        break;
    }

    if (!decoder) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to find decoder for format: %x",
                     videoFormat);
        return -1;
    }

    s_VideoDecoderCtx = avcodec_alloc_context3(decoder);
    if (!s_VideoDecoderCtx) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to allocate video decoder context");
        return -1;
    }

    // Enable slice multi-threading for software decoding
    s_VideoDecoderCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    s_VideoDecoderCtx->thread_type = FF_THREAD_SLICE;
    s_VideoDecoderCtx->thread_count = std::min(MAX_SLICES, SDL_GetCPUCount());

    // Setup decoding parameters
    s_VideoDecoderCtx->width = width;
    s_VideoDecoderCtx->height = height;
    s_VideoDecoderCtx->pix_fmt = AV_PIX_FMT_YUV420P; // FIXME: HDR

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
