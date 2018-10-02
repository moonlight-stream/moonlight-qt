#pragma once

#include <Limelight.h>

#define MAX_CHANNELS 6
#define SAMPLES_PER_FRAME 240

class IAudioRenderer
{
public:
    virtual ~IAudioRenderer() {}

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig) = 0;

    // Return false if an unrecoverable error has occurred and the renderer must be reinitialized
    virtual bool submitAudio(short* audioBuffer, int audioSize) = 0;
};
