#pragma once

#include <Limelight.h>
#include <SDL.h>
#include "settings/streamingpreferences.h"

#define SDL_CODE_FRAME_READY 0

#define MAX_SLICES 4

typedef struct _VIDEO_STATS {
    uint32_t receivedFrames;
    uint32_t decodedFrames;
    uint32_t renderedFrames;
    uint32_t totalFrames;
    uint32_t networkDroppedFrames;
    uint32_t pacerDroppedFrames;
    uint16_t minHostProcessingLatency;
    uint16_t maxHostProcessingLatency;
    uint32_t totalHostProcessingLatency;
    uint32_t framesWithHostProcessingLatency;
    uint32_t totalReassemblyTime;
    uint32_t totalDecodeTime;
    uint32_t totalPacerTime;
    uint32_t totalRenderTime;
    uint32_t lastRtt;
    uint32_t lastRttVariance;
    float totalFps;
    float receivedFps;
    float decodedFps;
    float renderedFps;
    uint32_t measurementStartTimestamp;
} VIDEO_STATS, *PVIDEO_STATS;

typedef struct _DECODER_PARAMETERS {
    SDL_Window* window;
    StreamingPreferences::VideoDecoderSelection vds;

    int videoFormat;
    int width;
    int height;
    int frameRate;
    bool enableVsync;
    bool enableFramePacing;
    bool testOnly;
} DECODER_PARAMETERS, *PDECODER_PARAMETERS;

class IVideoDecoder {
public:
    virtual ~IVideoDecoder() {}
    virtual bool initialize(PDECODER_PARAMETERS params) = 0;
    virtual bool isHardwareAccelerated() = 0;
    virtual bool isAlwaysFullScreen() = 0;
    virtual bool isHdrSupported() = 0;
    virtual int getDecoderCapabilities() = 0;
    virtual int getDecoderColorspace() = 0;
    virtual int getDecoderColorRange() = 0;
    virtual QSize getDecoderMaxResolution() = 0;
    virtual int submitDecodeUnit(PDECODE_UNIT du) = 0;
    virtual void renderFrameOnMainThread() = 0;
    virtual void setHdrMode(bool enabled) = 0;
};
