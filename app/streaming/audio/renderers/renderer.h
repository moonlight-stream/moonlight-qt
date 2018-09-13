#pragma once

#include <Limelight.h>

#define MAX_CHANNELS 6
#define SAMPLES_PER_FRAME 240

class IAudioRenderer
{
public:
    virtual ~IAudioRenderer() {}

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig) = 0;

    virtual void submitAudio(short* audioBuffer, int audioSize) = 0;

    virtual bool testAudio(int audioConfiguration) = 0;

    virtual int detectAudioConfiguration() = 0;
};
