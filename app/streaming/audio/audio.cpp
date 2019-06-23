#include "../session.h"
#include "renderers/renderer.h"

#ifdef HAVE_SOUNDIO
#include "renderers/soundioaudiorenderer.h"
#endif

#ifdef HAVE_SLAUDIO
#include "renderers/slaud.h"
#endif

#include "renderers/sdl.h"

#include <Limelight.h>

IAudioRenderer* Session::createAudioRenderer()
{
#if defined(HAVE_SOUNDIO)
#ifdef Q_OS_LINUX
    // Default is SDL with libsoundio as an alternate
    if (qgetenv("ML_AUDIO") == "libsoundio") {
        return new SoundIoAudioRenderer();
    }

    return new SdlAudioRenderer();
#else
    // Default is libsoundio with SDL as an alternate
    if (qgetenv("ML_AUDIO") == "SDL") {
        return new SdlAudioRenderer();
    }

    return new SoundIoAudioRenderer();
#endif
#elif defined(HAVE_SLAUDIO)
    return new SLAudioRenderer();
#else
    return new SdlAudioRenderer();
#endif
}

int Session::getAudioRendererCapabilities()
{
    IAudioRenderer* audioRenderer;

    audioRenderer = createAudioRenderer();
    if (audioRenderer == nullptr) {
        return 0;
    }

    int caps = audioRenderer->getCapabilities();

    delete audioRenderer;

    return caps;
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
    opusConfig.samplesPerFrame = 240;

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
    if (!s_ActiveSession->m_AudioRenderer->prepareForPlayback(opusConfig)) {
        delete s_ActiveSession->m_AudioRenderer;
        opus_multistream_decoder_destroy(s_ActiveSession->m_OpusDecoder);
        return -2;
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

#ifndef STEAM_LINK
    // Set this thread to high priority to reduce the chance of missing
    // our sample delivery time. On Steam Link, this causes starvation
    // of other threads due to severely restricted CPU time available,
    // so we will skip it on that platform.
    if (s_ActiveSession->m_AudioSampleCount == 0) {
        if (SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH) < 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Unable to set audio thread to high priority: %s",
                        SDL_GetError());
        }
    }
#endif

    // See if we need to drop this sample
    if (s_ActiveSession->m_DropAudioEndTime != 0) {
        if (SDL_TICKS_PASSED(SDL_GetTicks(), s_ActiveSession->m_DropAudioEndTime)) {
            // Avoid calling SDL_GetTicks() now
            s_ActiveSession->m_DropAudioEndTime = 0;

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Audio drop window has ended");
        }
        else {
            // We're still in the drop window
            return;
        }
    }

    s_ActiveSession->m_AudioSampleCount++;

    if (s_ActiveSession->m_AudioRenderer != nullptr) {
        int desiredSize = sizeof(short) * s_ActiveSession->m_AudioConfig.samplesPerFrame * s_ActiveSession->m_AudioConfig.channelCount;
        void* buffer = s_ActiveSession->m_AudioRenderer->getAudioBuffer(&desiredSize);
        if (buffer == nullptr) {
            return;
        }

        samplesDecoded = opus_multistream_decode(s_ActiveSession->m_OpusDecoder,
                                                 (unsigned char*)sampleData,
                                                 sampleLength,
                                                 (short*)buffer,
                                                 desiredSize / sizeof(short) / s_ActiveSession->m_AudioConfig.channelCount,
                                                 0);

        // Update desiredSize with the number of bytes actually populated by the decoding operation
        if (samplesDecoded > 0) {
            SDL_assert(desiredSize >= sizeof(short) * samplesDecoded * s_ActiveSession->m_AudioConfig.channelCount);
            desiredSize = sizeof(short) * samplesDecoded * s_ActiveSession->m_AudioConfig.channelCount;
        }
        else {
            desiredSize = 0;
        }

        if (!s_ActiveSession->m_AudioRenderer->submitAudio(desiredSize)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Reinitializing audio renderer after failure");

            delete s_ActiveSession->m_AudioRenderer;
            s_ActiveSession->m_AudioRenderer = nullptr;
        }
    }

    // Only try to recreate the audio renderer every 200 samples (1 second)
    // to avoid thrashing if the audio device is unavailable. It is
    // safe to reinitialize here because we can't be torn down while
    // the audio decoder/playback thread is still alive.
    if (s_ActiveSession->m_AudioRenderer == nullptr && (s_ActiveSession->m_AudioSampleCount % 200) == 0) {
        // Since we're doing this inline and audio initialization takes time, we need
        // to drop samples to account for the time we've spent blocking audio rendering
        // so we return to real-time playback and don't accumulate latency.
        Uint32 audioReinitStartTime = SDL_GetTicks();

        s_ActiveSession->m_AudioRenderer = s_ActiveSession->createAudioRenderer();
        if (!s_ActiveSession->m_AudioRenderer->prepareForPlayback(&s_ActiveSession->m_AudioConfig)) {
            delete s_ActiveSession->m_AudioRenderer;
            s_ActiveSession->m_AudioRenderer = nullptr;
        }

        Uint32 audioReinitStopTime = SDL_GetTicks();

        s_ActiveSession->m_DropAudioEndTime = audioReinitStopTime + (audioReinitStopTime - audioReinitStartTime);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Audio reinitialization took %d ms - starting drop window",
                    audioReinitStopTime - audioReinitStartTime);
    }
}
