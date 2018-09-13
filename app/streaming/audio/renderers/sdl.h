#pragma once

#include "renderer.h"
#include <SDL.h>

class SdlAudioRenderer : public IAudioRenderer
{
public:
    SdlAudioRenderer();

    virtual ~SdlAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void submitAudio(short* audioBuffer, int audioSize);

    virtual bool testAudio(int audioConfiguration);

    virtual int detectAudioConfiguration();

private:
    SDL_AudioDeviceID m_AudioDevice;
    int m_ChannelCount;
    int m_PendingDrops;
    int m_PendingHardDrops;
    unsigned int m_SampleIndex;
    Uint32 m_BaselinePendingData;
};
