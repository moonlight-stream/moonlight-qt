#pragma once

#include "renderer.h"
#include "SDL_compat.h"

class SdlAudioRenderer : public IAudioRenderer
{
public:
    explicit SdlAudioRenderer(int audioPlaybackThresholdMs = 30, int audioDropThresholdMs = 30);

    virtual ~SdlAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void* getAudioBuffer(int* size);

    virtual bool submitAudio(int bytesWritten);

    virtual AudioFormat getAudioBufferFormat();

private:
    SDL_AudioDeviceID m_AudioDevice;
    void* m_AudioBuffer;
    int m_FrameSize;
    int m_BytesPerMs;
    int m_AudioPlaybackThresholdMs;
    int m_AudioDropThresholdMs;
    bool m_WaitingForPlaybackThreshold;
};
