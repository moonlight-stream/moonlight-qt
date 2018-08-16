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
                            int maxFps) override;
    virtual bool isHardwareAccelerated() override;
    virtual int submitDecodeUnit(PDECODE_UNIT du) override;
    virtual void renderFrame(SDL_UserEvent* event) override;
    virtual void dropFrame(SDL_UserEvent* event) override;

    virtual IFFmpegRenderer* getRenderer();

private:
    bool completeInitialization(AVCodec* decoder, int videoFormat,
                                int width, int height, bool testOnly);

    IFFmpegRenderer* createAcceleratedRenderer(const AVCodecHWConfig* hwDecodeCfg);

    void reset();

    static
    enum AVPixelFormat ffGetFormat(AVCodecContext* context,
                                   const enum AVPixelFormat* pixFmts);

    AVPacket m_Pkt;
    AVCodecContext* m_VideoDecoderCtx;
    QByteArray m_DecodeBuffer;
    const AVCodecHWConfig* m_HwDecodeCfg;
    IFFmpegRenderer* m_Renderer;
    SDL_atomic_t m_QueuedFrames;
    int m_ConsecutiveFailedDecodes;

    static const uint8_t k_H264TestFrame[];
    static const uint8_t k_HEVCTestFrame[];
};
