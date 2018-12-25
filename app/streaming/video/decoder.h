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
    uint32_t networkDroppedFrames;
    uint32_t pacerDroppedFrames;
    uint32_t totalReassemblyTime;
    uint32_t totalDecodeTime;
    uint32_t totalPacerTime;
    uint32_t totalRenderTime;
    float receivedFps;
    float decodedFps;
    float renderedFps;
    uint32_t measurementStartTimestamp;
} VIDEO_STATS, *PVIDEO_STATS;

class IVideoDecoder {
public:
    virtual ~IVideoDecoder() {}
    virtual bool initialize(StreamingPreferences::VideoDecoderSelection vds,
                            SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height,
                            int frameRate,
                            bool enableVsync,
                            bool enableFramePacing) = 0;
    virtual bool isHardwareAccelerated() = 0;
    virtual int getDecoderCapabilities() = 0;
    virtual int submitDecodeUnit(PDECODE_UNIT du) = 0;
    virtual void renderFrame(SDL_UserEvent* event) = 0;
    virtual void dropFrame(SDL_UserEvent* event) = 0;

    virtual void queueFrame(void* data1 = nullptr,
                            void* data2 = nullptr)
    {
        SDL_Event event;

        event.type = SDL_USEREVENT;
        event.user.code = SDL_CODE_FRAME_READY;
        event.user.data1 = data1;
        event.user.data2 = data2;

        SDL_PushEvent(&event);
    }
};
