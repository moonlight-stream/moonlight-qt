#pragma once

#include "decoder.h"
#include "ffmpeg-renderers/renderer.h"
#include "ffmpeg-renderers/pacer/pacer.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class FFmpegVideoDecoder : public IVideoDecoder {
public:
    FFmpegVideoDecoder(bool testOnly);
    virtual ~FFmpegVideoDecoder() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool isHardwareAccelerated() override;
    virtual int getDecoderCapabilities() override;
    virtual int submitDecodeUnit(PDECODE_UNIT du) override;
    virtual void renderFrameOnMainThread() override;

    virtual IFFmpegRenderer* getBackendRenderer();

private:
    bool completeInitialization(AVCodec* decoder, PDECODER_PARAMETERS params, bool testFrame);

    void stringifyVideoStats(VIDEO_STATS& stats, char* output);

    void logVideoStats(VIDEO_STATS& stats, const char* title);

    void addVideoStats(VIDEO_STATS& src, VIDEO_STATS& dst);

    bool createFrontendRenderer(PDECODER_PARAMETERS params);

    IFFmpegRenderer* createAcceleratedRenderer(const AVCodecHWConfig* hwDecodeCfg);

    void reset();

    void writeBuffer(PLENTRY entry, int& offset);

    static
    enum AVPixelFormat ffGetFormat(AVCodecContext* context,
                                   const enum AVPixelFormat* pixFmts);

    AVPacket m_Pkt;
    AVCodecContext* m_VideoDecoderCtx;
    QByteArray m_DecodeBuffer;
    const AVCodecHWConfig* m_HwDecodeCfg;
    IFFmpegRenderer* m_BackendRenderer;
    IFFmpegRenderer* m_FrontendRenderer;
    int m_ConsecutiveFailedDecodes;
    Pacer* m_Pacer;
    VIDEO_STATS m_ActiveWndVideoStats;
    VIDEO_STATS m_LastWndVideoStats;
    VIDEO_STATS m_GlobalVideoStats;

    int m_LastFrameNumber;
    int m_StreamFps;
    bool m_NeedsSpsFixup;
    bool m_TestOnly;

    static const uint8_t k_H264TestFrame[];
    static const uint8_t k_HEVCTestFrame[];
};
