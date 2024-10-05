#pragma once

#include "renderer.h"
#include <SDL.h>

class SdlAudioRenderer : public IAudioRenderer
{
public:
    SdlAudioRenderer();

    virtual ~SdlAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void* getAudioBuffer(int* size);

    virtual bool submitAudio(int bytesWritten);

    virtual int getCapabilities();

    virtual AudioFormat getAudioBufferFormat();

    const char * getRendererName() { return m_Name; }

private:
    SDL_AudioDeviceID m_AudioDevice;
    void* m_AudioBuffer;
    int m_FrameSize;
    char m_Name[24];
};
