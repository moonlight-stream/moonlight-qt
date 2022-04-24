#pragma once

#include "decoder.h"

#include <SLVideo.h>

class SLVideoDecoder : public IVideoDecoder
{
public:
    SLVideoDecoder(bool testOnly);
    virtual ~SLVideoDecoder();
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool isHardwareAccelerated() override;
    virtual bool isAlwaysFullScreen() override;
    virtual int getDecoderCapabilities() override;
    virtual int getDecoderColorspace() override;
    virtual QSize getDecoderMaxResolution() override;
    virtual int submitDecodeUnit(PDECODE_UNIT du) override;

    // Unused since rendering is done directly from the decode thread
    virtual void renderFrameOnMainThread() override {}

    // HDR is not supported by SLVideo
    virtual void setHdrMode(bool) override {}
    virtual bool isHdrSupported() override {
        return false;
    }

    // SLVideo cannot apply any window changes (nor do we expect any)
    virtual bool applyWindowChange(int, int, int) override {
        return false;
    }

private:
    static void slLogCallback(void* context, ESLVideoLog logLevel, const char* message);

    CSLVideoContext* m_VideoContext;
    CSLVideoStream* m_VideoStream;
};
