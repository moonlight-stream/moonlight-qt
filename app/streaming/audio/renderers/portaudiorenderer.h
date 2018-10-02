#pragma once

#include <portaudio.h>

#include "renderer.h"

#define CIRCULAR_BUFFER_SIZE 16
#define MAX_CHANNEL_COUNT 6

#define CIRCULAR_BUFFER_STRIDE (MAX_CHANNEL_COUNT * SAMPLES_PER_FRAME)

class PortAudioRenderer : public IAudioRenderer
{
public:
    PortAudioRenderer();

    virtual ~PortAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void submitAudio(short* audioBuffer, int audioSize);

    virtual bool testAudio(int audioConfiguration) const;

    virtual int detectAudioConfiguration() const;

    virtual void adjustOpusChannelMapping(OPUS_MULTISTREAM_CONFIGURATION* opusConfig) const;

    static int paStreamCallback(const void *input,
                                void *output,
                                unsigned long frameCount,
                                const PaStreamCallbackTimeInfo *timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void *userData);

private:
    PaStream* m_Stream;
    int m_ChannelCount;
    int m_WriteIndex;
    int m_ReadIndex;
    bool m_Started;
    short m_AudioBuffer[CIRCULAR_BUFFER_SIZE * CIRCULAR_BUFFER_STRIDE];
};

