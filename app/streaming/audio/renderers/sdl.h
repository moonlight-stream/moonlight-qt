#pragma once

#include "renderer.h"
#include <SDL.h>

#ifndef HAVE_SLVIDEO
#error SDL audio backend is only available for Steam Link
#error Please install PortAudio to build for Linux
#endif

class SdlAudioRenderer : public IAudioRenderer
{
public:
    SdlAudioRenderer();

    virtual ~SdlAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void submitAudio(short* audioBuffer, int audioSize);

    virtual int detectAudioConfiguration() const;

private:
    SDL_AudioDeviceID m_AudioDevice;
    Uint32 m_ChannelCount;
    Uint32 m_PendingDrops;
    Uint32 m_PendingHardDrops;
    Uint32 m_SampleIndex;
    Uint32 m_BaselinePendingData;
};
