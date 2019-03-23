#pragma once

#include "renderer.h"
#include <SLAudio.h>

class SLAudioRenderer : public IAudioRenderer
{
public:
    SLAudioRenderer();

    virtual ~SLAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual bool submitAudio(short* audioBuffer, int audioSize);

private:
    CSLAudioContext* m_AudioContext;
    CSLAudioStream* m_AudioStream;
};
