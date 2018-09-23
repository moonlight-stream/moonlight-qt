#include "../session.hpp"
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

    bool ret = audioRenderer->testAudio(audioConfiguration);

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

    s_ActiveSession->m_OpusDecoder =
            opus_multistream_decoder_create(opusConfig->sampleRate,
                                            opusConfig->channelCount,
                                            opusConfig->streams,
                                            opusConfig->coupledStreams,
                                            opusConfig->mapping,
                                            &error);
    if (s_ActiveSession->m_OpusDecoder == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create decoder: %d",
                     error);
        return -1;
    }

    s_ActiveSession->m_AudioRenderer = s_ActiveSession->createAudioRenderer();
    if (s_ActiveSession->m_AudioRenderer == nullptr) {
        opus_multistream_decoder_destroy(s_ActiveSession->m_OpusDecoder);
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
    // m_AudioRenderer is deleted in cleanupAudioRenderer()

    opus_multistream_decoder_destroy(s_ActiveSession->m_OpusDecoder);
    s_ActiveSession->m_OpusDecoder = nullptr;
}

// This is called on the main thread
void Session::cleanupAudioRendererOnMainThread()
{
    SDL_AtomicLock(&m_AudioRendererLock);
    delete m_AudioRenderer;
    m_AudioRenderer = nullptr;
    SDL_AtomicUnlock(&m_AudioRendererLock);
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
        // If we can't acquire the lock, that means we're being destroyed
        // so don't even bother trying to wait.
        if (SDL_AtomicTryLock(&s_ActiveSession->m_AudioRendererLock)) {
            if (s_ActiveSession->m_AudioRenderer != nullptr) {
                s_ActiveSession->m_AudioRenderer->submitAudio(s_ActiveSession->m_OpusDecodeBuffer,
                                                              static_cast<int>(sizeof(short) *
                                                                samplesDecoded *
                                                                s_ActiveSession->m_AudioConfig.channelCount));
            }
            SDL_AtomicUnlock(&s_ActiveSession->m_AudioRendererLock);
        }
    }
}
