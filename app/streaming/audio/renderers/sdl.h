#pragma once

#include "renderer.h"
#if HAVE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

class SdlAudioRenderer : public IAudioRenderer
{
public:
    SdlAudioRenderer();

    virtual ~SdlAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void* getAudioBuffer(int* size);

    virtual bool submitAudio(int bytesWritten);

    virtual int getCapabilities();

private:
#if SDL_VERSION_ATLEAST(3, 0, 0)
    SDL_AudioStream *m_AudioStream;
#else
    SDL_AudioDeviceID m_AudioDevice;
#endif
    void* m_AudioBuffer;
    int m_FrameSize;
};
