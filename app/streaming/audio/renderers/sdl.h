#pragma once

#include "renderer.h"
#include <SDL.h>

class SdlAudioRenderer : public IAudioRenderer
{
public:
    SdlAudioRenderer();

    virtual ~SdlAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual bool submitAudio(short* audioBuffer, int audioSize);

private:
    SDL_AudioDeviceID m_AudioDevice;
    Uint32 m_ChannelCount;
    Uint32 m_PendingDrops;
    Uint32 m_PendingHardDrops;
    Uint32 m_SampleIndex;
    Uint32 m_BaselinePendingData;
};
