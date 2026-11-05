#include "sdl.h"

#include <Limelight.h>

SdlAudioRenderer::SdlAudioRenderer()
    : m_AudioDevice(0),
      m_AudioStream(nullptr),
      m_AudioBuffer(nullptr)
{
    SDL_assert(!SDL_WasInit(SDL_INIT_AUDIO));

    if (SDLC_FAILURE(SDL_InitSubSystem(SDL_INIT_AUDIO))) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s",
                     SDL_GetError());
        SDL_assert(SDL_WasInit(SDL_INIT_AUDIO));
    }
}

bool SdlAudioRenderer::prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
{
    SDL_AudioSpec want, have;

    SDL_zero(want);
    want.freq = opusConfig->sampleRate;
    want.format = SDL_AUDIO_F32;
    want.channels = opusConfig->channelCount;

    m_FrameDurationMs = opusConfig->samplesPerFrame / (opusConfig->sampleRate / 1000);
    m_FrameSize = opusConfig->samplesPerFrame *
                  opusConfig->channelCount *
                  getAudioBufferSampleSize();

    m_AudioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                              &want,
                                              nullptr,
                                              nullptr);
    if (m_AudioStream == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open audio device: %s",
                     SDL_GetError());
        return false;
    }
    m_AudioDevice = SDL_GetAudioStreamDevice(m_AudioStream);

    m_AudioBuffer = SDL_malloc(m_FrameSize);
    if (m_AudioBuffer == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to allocate audio buffer");
        return false;
    }

    int sampleFrames = 0;
    if (SDL_GetAudioDeviceFormat(m_AudioDevice, &have, &sampleFrames)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Obtained audio buffer: %d samples (%d bytes)",
                    sampleFrames,
                    sampleFrames * SDL_AUDIO_FRAMESIZE(have));
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "SDL audio driver: %s",
                SDL_GetCurrentAudioDriver());

    // Start playback
    SDL_ResumeAudioStreamDevice(m_AudioStream);

    return true;
}

SdlAudioRenderer::~SdlAudioRenderer()
{
    if (m_AudioStream != nullptr) {
        // Stop playback
        SDL_PauseAudioStreamDevice(m_AudioStream);
        SDL_DestroyAudioStream(m_AudioStream);
    }

    if (m_AudioBuffer != nullptr) {
        SDL_free(m_AudioBuffer);
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_assert(!SDL_WasInit(SDL_INIT_AUDIO));
}

void* SdlAudioRenderer::getAudioBuffer(int*)
{
    return m_AudioBuffer;
}

bool SdlAudioRenderer::submitAudio(int bytesWritten)
{
    if (bytesWritten == 0) {
        // Nothing to do
        return true;
    }

    // Don't queue if there's already more than 30 ms of audio data waiting
    // in Moonlight's audio queue.
    if (LiGetPendingAudioDuration() > 30) {
        return true;
    }

    // Provide backpressure on the queue to ensure too many frames don't build up
    // in SDL's audio queue, but don't wait forever to avoid a deadlock if the
    // audio device fails.
    for (int i = 0; i < 100; i++) {
        // Our device may enter a permanent error status upon removal, so we need
        // to recreate the audio device to pick up the new default audio device.
        SDL_AudioSpec deviceSpec;
        if (!SDL_GetAudioDeviceFormat(m_AudioDevice, &deviceSpec, nullptr)) {
            return false;
        }

        // Only queue more samples where there is 50 ms or less in SDL's queue
        if (SDL_GetAudioStreamQueued(m_AudioStream) / m_FrameSize * m_FrameDurationMs <= 50) {
            break;
        }

        SDL_Delay(1);
    }

    if (!SDL_PutAudioStreamData(m_AudioStream, m_AudioBuffer, bytesWritten)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to queue audio sample: %s",
                     SDL_GetError());
    }

    return true;
}

IAudioRenderer::AudioFormat SdlAudioRenderer::getAudioBufferFormat()
{
    return AudioFormat::Float32NE;
}
