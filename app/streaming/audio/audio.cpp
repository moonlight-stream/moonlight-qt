#include "../session.h"
#include "renderers/renderer.h"

#ifdef HAVE_PORTAUDIO
#include "renderers/portaudiorenderer.h"
#else
#include "renderers/sdl.h"
#endif

#include <Limelight.h>

IAudioRenderer* Session::createAudioRenderer()
{
#ifdef HAVE_PORTAUDIO
    return new PortAudioRenderer();
#else
    return new SdlAudioRenderer();
#endif
}

bool Session::testAudio(int audioConfiguration)
{
    IAudioRenderer* audioRenderer;

    audioRenderer = createAudioRenderer();
    if (audioRenderer == nullptr) {
        return false;
    }

    // Build a fake OPUS_MULTISTREAM_CONFIGURATION to give
    // the renderer the channel count and sample rate.
    OPUS_MULTISTREAM_CONFIGURATION opusConfig = {};
    opusConfig.sampleRate = 48000;

    switch (audioConfiguration)
    {
    case AUDIO_CONFIGURATION_STEREO:
        opusConfig.channelCount = 2;
        break;
    case AUDIO_CONFIGURATION_51_SURROUND:
        opusConfig.channelCount = 6;
        break;
    default:
        SDL_assert(false);
        return false;
    }

    bool ret = audioRenderer->prepareForPlayback(&opusConfig);

    delete audioRenderer;

    return ret;
}

int Session::detectAudioConfiguration()
{
    IAudioRenderer* audioRenderer;

    audioRenderer = createAudioRenderer();
    if (audioRenderer == nullptr) {
        // Hope for the best
        return AUDIO_CONFIGURATION_STEREO;
    }

    int audioConfig = audioRenderer->detectAudioConfiguration();

    delete audioRenderer;

    return audioConfig;
}

int Session::arInit(int /* audioConfiguration */,
                    const POPUS_MULTISTREAM_CONFIGURATION opusConfig,
                    void* /* arContext */, int /* arFlags */)
{
    int error;

    SDL_memcpy(&s_ActiveSession->m_AudioConfig, opusConfig, sizeof(*opusConfig));

    s_ActiveSession->m_AudioRenderer = s_ActiveSession->createAudioRenderer();
    if (s_ActiveSession->m_AudioRenderer == nullptr) {
        return -1;
    }

    // Allow the audio renderer to adjust the channel mapping to fit its
    // preferred channel order
    s_ActiveSession->m_AudioRenderer->adjustOpusChannelMapping(&s_ActiveSession->m_AudioConfig);

    s_ActiveSession->m_OpusDecoder =
            opus_multistream_decoder_create(opusConfig->sampleRate,
                                            opusConfig->channelCount,
                                            opusConfig->streams,
                                            opusConfig->coupledStreams,
                                            s_ActiveSession->m_AudioConfig.mapping,
                                            &error);
    if (s_ActiveSession->m_OpusDecoder == NULL) {
        delete s_ActiveSession->m_AudioRenderer;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create decoder: %d",
                     error);
        return -2;
    }

    if (!s_ActiveSession->m_AudioRenderer->prepareForPlayback(opusConfig)) {
        delete s_ActiveSession->m_AudioRenderer;
        opus_multistream_decoder_destroy(s_ActiveSession->m_OpusDecoder);
        return -3;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio stream has %d channels",
                opusConfig->channelCount);

    return 0;
}

void Session::arCleanup()
{
    delete s_ActiveSession->m_AudioRenderer;
    s_ActiveSession->m_AudioRenderer = nullptr;

    opus_multistream_decoder_destroy(s_ActiveSession->m_OpusDecoder);
    s_ActiveSession->m_OpusDecoder = nullptr;
}

void Session::arDecodeAndPlaySample(char* sampleData, int sampleLength)
{
    int samplesDecoded;

    samplesDecoded = opus_multistream_decode(s_ActiveSession->m_OpusDecoder,
                                             (unsigned char*)sampleData,
                                             sampleLength,
                                             s_ActiveSession->m_OpusDecodeBuffer,
                                             SAMPLES_PER_FRAME,
                                             0);
    if (samplesDecoded > 0) {
        s_ActiveSession->m_AudioRenderer->submitAudio(s_ActiveSession->m_OpusDecodeBuffer,
                                                      static_cast<int>(sizeof(short) *
                                                        samplesDecoded *
                                                        s_ActiveSession->m_AudioConfig.channelCount));
    }
}
