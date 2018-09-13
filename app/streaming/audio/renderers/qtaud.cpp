#include "qtaud.h"

#include <QDebug>

QtAudioRenderer::QtAudioRenderer()
    : m_AudioOutput(nullptr),
      m_OutputDevice(nullptr)
{

}

QtAudioRenderer::~QtAudioRenderer()
{
    delete m_AudioOutput;
}

bool QtAudioRenderer::prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
{
    QAudioFormat format;

    // testAudio() assumes this sample rate
    Q_ASSERT(opusConfig->sampleRate == 48000);

    format.setSampleRate(opusConfig->sampleRate);
    format.setChannelCount(opusConfig->channelCount);
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setCodec("audio/pcm");

    m_AudioOutput = new QAudioOutput(format);

    m_AudioOutput->setBufferSize(SAMPLES_PER_FRAME * 2);

    qInfo() << "Audio stream buffer:" << m_AudioOutput->bufferSize() << "bytes";

    m_OutputDevice = m_AudioOutput->start();
    if (m_OutputDevice == nullptr) {
        qWarning() << "Audio start failed:" << m_AudioOutput->error();
        return false;
    }

    return true;
}

void QtAudioRenderer::submitAudio(short* audioBuffer, int audioSize)
{
    m_OutputDevice->write((const char *)audioBuffer, audioSize);
}

bool QtAudioRenderer::testAudio(int audioConfiguration)
{
    QAudioFormat format;

    format.setSampleRate(48000);
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setCodec("audio/pcm");

    switch (audioConfiguration) {
    case AUDIO_CONFIGURATION_STEREO:
        format.setChannelCount(2);
        break;
    case AUDIO_CONFIGURATION_51_SURROUND:
        format.setChannelCount(6);
        break;
    default:
        Q_ASSERT(false);
        return false;
    }

    if (QAudioDeviceInfo::defaultOutputDevice().isFormatSupported(format)) {
        qInfo() << "Audio test successful with" << format.channelCount() << "channels";
        return true;
    }
    else {
        qWarning() << "Audio test failed with" << format.channelCount() << "channels";
        return false;
    }
}

int QtAudioRenderer::detectAudioConfiguration()
{
    int preferredChannelCount = QAudioDeviceInfo::defaultOutputDevice().preferredFormat().channelCount();

    qInfo() << "Audio output device prefers" << preferredChannelCount << "channel configuration";

    // We can better downmix 5.1 to quad than we can upmix stereo
    if (preferredChannelCount > 2) {
        return AUDIO_CONFIGURATION_51_SURROUND;
    }
    else {
        return AUDIO_CONFIGURATION_STEREO;
    }
}
