#pragma once

#include "decoder.h"

#include <SLVideo.h>

class SLVideoDecoder : public IVideoDecoder
{
public:
    SLVideoDecoder(bool testOnly);
    virtual ~SLVideoDecoder();
    virtual bool initialize(StreamingPreferences::VideoDecoderSelection vds,
                            SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height,
                            int frameRate,
                            bool enableVsync,
                            bool enableFramePacing);
    virtual bool isHardwareAccelerated();
    virtual int getDecoderCapabilities();
    virtual int submitDecodeUnit(PDECODE_UNIT du);

    // Unused since rendering is done directly from the decode thread
    virtual void renderFrameOnMainThread() {}

private:
    CSLVideoContext* m_VideoContext;
    CSLVideoStream* m_VideoStream;
};
