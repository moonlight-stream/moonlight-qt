#pragma once

#include "renderer.h"
#include <QAudioOutput>

class QtAudioRenderer : public IAudioRenderer
{
public:
    QtAudioRenderer();

    virtual ~QtAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void submitAudio(short* audioBuffer, int audioSize);

    virtual bool testAudio(int audioConfiguration);

    virtual int detectAudioConfiguration();

private:
    QAudioOutput* m_AudioOutput;
    QIODevice* m_OutputDevice;
};
