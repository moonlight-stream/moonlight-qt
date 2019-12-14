#pragma once

#include "decoder.h"

#include <SLVideo.h>

class SLVideoDecoder : public IVideoDecoder
{
public:
    SLVideoDecoder(bool testOnly);
    virtual ~SLVideoDecoder();
    virtual bool initialize(PDECODER_PARAMETERS params);
    virtual bool isHardwareAccelerated();
    virtual int getDecoderCapabilities();
    virtual int getDecoderColorspace();
    virtual int submitDecodeUnit(PDECODE_UNIT du);

    // Unused since rendering is done directly from the decode thread
    virtual void renderFrameOnMainThread() {}

private:
    static void slLogCallback(void* context, ESLVideoLog logLevel, const char* message);

    CSLVideoContext* m_VideoContext;
    CSLVideoStream* m_VideoStream;
};
