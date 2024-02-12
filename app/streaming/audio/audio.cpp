#include "../session.h"
#include "renderers/renderer.h"

#ifdef HAVE_SOUNDIO
#include "renderers/soundioaudiorenderer.h"
#endif

#ifdef HAVE_SLAUDIO
#include "renderers/slaud.h"
#endif

#include "renderers/sdl.h"

// SDL_TICKS_PASSED is removed in SDL3
#if SDL_VERSION_ATLEAST(3, 0, 0)
#define SDL_TICKS_PASSED(A, B)  ((Sint32)((B) - (A)) <= 0)
#endif

#include <Limelight.h>

#define TRY_INIT_RENDERER(renderer, opusConfig)        \
{                                                      \
    IAudioRenderer* __renderer = new renderer();       \
    if (__renderer->prepareForPlayback(opusConfig))    \
        return __renderer;                             \
    delete __renderer;                                 \
}

IAudioRenderer* Session::createAudioRenderer(const POPUS_MULTISTREAM_CONFIGURATION opusConfig)
{
    // Handle explicit ML_AUDIO setting and fail if the requested backend fails
    QString mlAudio = qgetenv("ML_AUDIO").toLower();
    if (mlAudio == "sdl") {
        TRY_INIT_RENDERER(SdlAudioRenderer, opusConfig)
        return nullptr;
    }
#ifdef HAVE_SOUNDIO
    else if (mlAudio == "libsoundio") {
        TRY_INIT_RENDERER(SoundIoAudioRenderer, opusConfig)
        return nullptr;
    }
#endif
#if defined(HAVE_SLAUDIO)
    else if (mlAudio == "slaudio") {
        TRY_INIT_RENDERER(SLAudioRenderer, opusConfig)
        return nullptr;
    }
#endif
    else if (!mlAudio.isEmpty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unknown audio backend: %s",
                     SDL_getenv("ML_AUDIO"));
        return nullptr;
    }

    // -------------- Automatic backend selection below this line ---------------

#if defined(HAVE_SLAUDIO)
    // Steam Link should always have SLAudio
    TRY_INIT_RENDERER(SLAudioRenderer, opusConfig)
#endif

    // Default to SDL and use libsoundio as a fallback
    TRY_INIT_RENDERER(SdlAudioRenderer, opusConfig)
#ifdef HAVE_SOUNDIO
    TRY_INIT_RENDERER(SoundIoAudioRenderer, opusConfig)
#endif

    return nullptr;
}

int Session::getAudioRendererCapabilities(int audioConfiguration)
{
    // Build a fake OPUS_MULTISTREAM_CONFIGURATION to give
    // the renderer the channel count and sample rate.
    OPUS_MULTISTREAM_CONFIGURATION opusConfig = {};
    opusConfig.sampleRate = 48000;
    opusConfig.samplesPerFrame = 240;
    opusConfig.channelCount = CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(audioConfiguration);

    IAudioRenderer* audioRenderer = createAudioRenderer(&opusConfig);
    if (audioRenderer == nullptr) {
        return 0;
    }

    int caps = audioRenderer->getCapabilities();

    delete audioRenderer;

    return caps;
}

bool Session::testAudio(int audioConfiguration)
{
    // Build a fake OPUS_MULTISTREAM_CONFIGURATION to give
    // the renderer the channel count and sample rate.
    OPUS_MULTISTREAM_CONFIGURATION opusConfig = {};
    opusConfig.sampleRate = 48000;
    opusConfig.samplesPerFrame = 240;
    opusConfig.channelCount = CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(audioConfiguration);

    IAudioRenderer* audioRenderer = createAudioRenderer(&opusConfig);
    if (audioRenderer == nullptr) {
        return false;
    }

    delete audioRenderer;

    return true;
}

int Session::arInit(int /* audioConfiguration */,
                    const POPUS_MULTISTREAM_CONFIGURATION opusConfig,
                    void* /* arContext */, int /* arFlags */)
{
    int error;

    SDL_memcpy(&s_ActiveSession->m_AudioConfig, opusConfig, sizeof(*opusConfig));

    s_ActiveSession->m_AudioRenderer = s_ActiveSession->createAudioRenderer(&s_ActiveSession->m_AudioConfig);
    if (s_ActiveSession->m_AudioRenderer == nullptr) {
        return -2;
    }

    // Allow the chosen renderer to remap Opus channels as needed to ensure proper output
    s_ActiveSession->m_AudioRenderer->remapChannels(&s_ActiveSession->m_AudioConfig);

    // Create the Opus decoder with the renderer's preferred channel mapping
    s_ActiveSession->m_OpusDecoder =
            opus_multistream_decoder_create(s_ActiveSession->m_AudioConfig.sampleRate,
                                            s_ActiveSession->m_AudioConfig.channelCount,
                                            s_ActiveSession->m_AudioConfig.streams,
                                            s_ActiveSession->m_AudioConfig.coupledStreams,
                                            s_ActiveSession->m_AudioConfig.mapping,
                                            &error);
    if (s_ActiveSession->m_OpusDecoder == NULL) {
        delete s_ActiveSession->m_AudioRenderer;
        s_ActiveSession->m_AudioRenderer = nullptr;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create decoder: %d",
                     error);
        return -1;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio stream has %d channels",
                s_ActiveSession->m_AudioConfig.channelCount);

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

    // If audio is muted, don't decode or play the audio
    if (s_ActiveSession->m_AudioMuted) {
        return;
    }

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
            SDL_assert(desiredSize >= (int)(sizeof(short) * samplesDecoded * s_ActiveSession->m_AudioConfig.channelCount));
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

        s_ActiveSession->m_AudioRenderer = s_ActiveSession->createAudioRenderer(&s_ActiveSession->m_AudioConfig);

        Uint32 audioReinitStopTime = SDL_GetTicks();

        s_ActiveSession->m_DropAudioEndTime = audioReinitStopTime + (audioReinitStopTime - audioReinitStartTime);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Audio reinitialization took %d ms - starting drop window",
                    audioReinitStopTime - audioReinitStartTime);
    }
}
