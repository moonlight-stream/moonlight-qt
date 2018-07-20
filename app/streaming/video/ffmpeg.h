#pragma once

#include "decoder.h"
#include "ffmpeg-renderers/renderer.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class FFmpegVideoDecoder : public IVideoDecoder {
public:
    FFmpegVideoDecoder();
    virtual ~FFmpegVideoDecoder();
    virtual bool initialize(StreamingPreferences::VideoDecoderSelection vds,
                            SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height,
                            int frameRate) override;
    virtual bool isHardwareAccelerated() override;
    virtual int submitDecodeUnit(PDECODE_UNIT du) override;
    virtual void renderFrame(SDL_UserEvent* event) override;
    virtual void dropFrame(SDL_UserEvent* event) override;

private:
    bool
    chooseDecoder(
        StreamingPreferences::VideoDecoderSelection vds,
        SDL_Window* window,
        int videoFormat,
        int width, int height,
        AVCodec*& chosenDecoder,
        const AVCodecHWConfig*& chosenHwConfig,
        IFFmpegRenderer*& newRenderer);

    AVPacket m_Pkt;
    AVCodecContext* m_VideoDecoderCtx;
    QByteArray m_DecodeBuffer;
    const AVCodecHWConfig* m_HwDecodeCfg;
    IFFmpegRenderer* m_Renderer;
    SDL_atomic_t m_QueuedFrames;
};
