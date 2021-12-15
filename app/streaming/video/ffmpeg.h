#pragma once

#include <functional>

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
    virtual bool isAlwaysFullScreen() override;
    virtual int getDecoderCapabilities() override;
    virtual int getDecoderColorspace() override;
    virtual QSize getDecoderMaxResolution() override;
    virtual int submitDecodeUnit(PDECODE_UNIT du) override;
    virtual void renderFrameOnMainThread() override;

    virtual IFFmpegRenderer* getBackendRenderer();

private:
    bool completeInitialization(const AVCodec* decoder, PDECODER_PARAMETERS params, bool testFrame, bool eglOnly);

    void stringifyVideoStats(VIDEO_STATS& stats, char* output);

    void logVideoStats(VIDEO_STATS& stats, const char* title);

    void addVideoStats(VIDEO_STATS& src, VIDEO_STATS& dst);

    bool createFrontendRenderer(PDECODER_PARAMETERS params, bool eglOnly);

    bool tryInitializeRendererForDecoderByName(const char* decoderName,
                                               PDECODER_PARAMETERS params);

    bool tryInitializeRenderer(const AVCodec* decoder,
                               PDECODER_PARAMETERS params,
                               const AVCodecHWConfig* hwConfig,
                               std::function<IFFmpegRenderer*()> createRendererFunc);

    static IFFmpegRenderer* createHwAccelRenderer(const AVCodecHWConfig* hwDecodeCfg, int pass);

    void reset();

    void writeBuffer(PLENTRY entry, int& offset);

    static
    enum AVPixelFormat ffGetFormat(AVCodecContext* context,
                                   const enum AVPixelFormat* pixFmts);

    AVPacket* m_Pkt;
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

    int m_FramesIn;
    int m_FramesOut;

    int m_LastFrameNumber;
    int m_StreamFps;
    int m_VideoFormat;
    bool m_NeedsSpsFixup;
    bool m_TestOnly;
    enum {
        RRF_UNKNOWN,
        RRF_YES,
        RRF_NO
    } m_CanRetryReceiveFrame;

    static const uint8_t k_H264TestFrame[];
    static const uint8_t k_HEVCMainTestFrame[];
    static const uint8_t k_HEVCMain10TestFrame[];
};
