#pragma once

#include <Limelight.h>
#include <SDL.h>
#include "settings/streamingpreferences.h"

#define SDL_CODE_FRAME_READY 0

#define MAX_SLICES 4

class IVideoDecoder {
public:
    virtual ~IVideoDecoder() {}
    virtual bool initialize(StreamingPreferences::VideoDecoderSelection vds,
                            SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height,
                            int frameRate,
                            bool enableVsync) = 0;
    virtual bool isHardwareAccelerated() = 0;
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
