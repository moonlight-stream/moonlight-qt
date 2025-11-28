/**
 * @file app/streaming/audio/renderers/sdl.h
 * @brief SDL audio renderer implementation.
 */

#pragma once

#include "renderer.h"
#include "SDL_compat.h"

/**
 * @brief Audio renderer using SDL audio subsystem.
 */
class SdlAudioRenderer : public IAudioRenderer
{
public:
    SdlAudioRenderer();

    virtual ~SdlAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void* getAudioBuffer(int* size);

    virtual bool submitAudio(int bytesWritten);

    virtual AudioFormat getAudioBufferFormat();

private:
    SDL_AudioDeviceID m_AudioDevice;
    void* m_AudioBuffer;
    int m_FrameSize;
};
