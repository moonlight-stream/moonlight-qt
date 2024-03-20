#include "sdl.h"

#include <Limelight.h>
#if HAVE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

SdlAudioRenderer::SdlAudioRenderer()
#if SDL_VERSION_ATLEAST(3, 0, 0)
    : m_AudioStream(nullptr),
#else
    : m_AudioDevice(0),
#endif
      m_AudioBuffer(nullptr)
{
    SDL_assert(!SDL_WasInit(SDL_INIT_AUDIO));

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s",
                     SDL_GetError());
        SDL_assert(SDL_WasInit(SDL_INIT_AUDIO));
    }
}

bool SdlAudioRenderer::prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
{
#if SDL_VERSION_ATLEAST(3, 0, 0)
    SDL_AudioSpec spec;

    SDL_zero(spec);
    spec.freq = opusConfig->sampleRate;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = opusConfig->channelCount;

    m_FrameSize = opusConfig->samplesPerFrame * sizeof(short) * opusConfig->channelCount;

    m_AudioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &spec, NULL, NULL);
    if (!m_AudioStream) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open audio device: %s",
                     SDL_GetError());
        return false;
    }

    m_AudioBuffer = SDL_malloc(m_FrameSize);
    if (m_AudioBuffer == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to allocate audio buffer");
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "SDL audio driver: %s",
                SDL_GetCurrentAudioDriver());

    // Start playback
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(m_AudioStream));
#else
    SDL_AudioSpec want, have;

    SDL_zero(want);
    want.freq = opusConfig->sampleRate;
    want.format = AUDIO_S16;
    want.channels = opusConfig->channelCount;

    // On PulseAudio systems, setting a value too small can cause underruns for other
    // applications sharing this output device. We impose a floor of 480 samples (10 ms)
    // to mitigate this issue. Otherwise, we will buffer up to 3 frames of audio which
    // is 15 ms at regular 5 ms frames and 30 ms at 10 ms frames for slow connections.
    // The buffering helps avoid audio underruns due to network jitter.
#ifndef Q_OS_DARWIN
    want.samples = SDL_max(480, opusConfig->samplesPerFrame * 3);
#else
    // HACK: Changing the buffer size can lead to Bluetooth HFP
    // audio issues on macOS, so we're leaving this alone.
    // https://github.com/moonlight-stream/moonlight-qt/issues/1071
    want.samples = SDL_max(480, opusConfig->samplesPerFrame);
#endif

    m_FrameSize = opusConfig->samplesPerFrame * sizeof(short) * opusConfig->channelCount;

    m_AudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (m_AudioDevice == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open audio device: %s",
                     SDL_GetError());
        return false;
    }

    m_AudioBuffer = SDL_malloc(m_FrameSize);
    if (m_AudioBuffer == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to allocate audio buffer");
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Desired audio buffer: %u samples (%u bytes)",
                want.samples,
                want.samples * (Uint32)sizeof(short) * want.channels);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Obtained audio buffer: %u samples (%u bytes)",
                have.samples,
                have.size);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "SDL audio driver: %s",
                SDL_GetCurrentAudioDriver());

    // Start playback
    SDL_PauseAudioDevice(m_AudioDevice, 0);
#endif

    return true;
}

SdlAudioRenderer::~SdlAudioRenderer()
{
#if SDL_VERSION_ATLEAST(3, 0, 0)
    if (m_AudioStream) {
        // Stop and destroy audio stream
        SDL_PauseAudioDevice(SDL_GetAudioStreamDevice(m_AudioStream));
        SDL_DestroyAudioStream(m_AudioStream);
    }
#else
    if (m_AudioDevice) {
        // Stop playback
        SDL_PauseAudioDevice(m_AudioDevice, 1);
        SDL_CloseAudioDevice(m_AudioDevice);
    }
#endif

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
    // Our device may enter a permanent error status upon removal, so we need
    // to recreate the audio device to pick up the new default audio device.
#if SDL_VERSION_ATLEAST(3, 0, 0)
    if (SDL_AudioDevicePaused(SDL_GetAudioStreamDevice(m_AudioStream))) {
#else
    if (SDL_GetAudioDeviceStatus(m_AudioDevice) == SDL_AUDIO_STOPPED) {
#endif
        return false;
    }

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
    // in SDL's audio queue.
#if SDL_VERSION_ATLEAST(3, 0, 0)
    // TODO: Check to see if this is correct and doing what SDL2 was doing.
    while (SDL_GetAudioStreamQueued(m_AudioStream) / m_FrameSize > 10) {
        SDL_Delay(1);
    }
#else
    while (SDL_GetQueuedAudioSize(m_AudioDevice) / m_FrameSize > 10) {
        SDL_Delay(1);
    }
#endif

#if SDL_VERSION_ATLEAST(3, 0, 0)
    if (SDL_PutAudioStreamData(m_AudioStream, m_AudioBuffer, bytesWritten) < 0) {
#else
    if (SDL_QueueAudio(m_AudioDevice, m_AudioBuffer, bytesWritten) < 0) {
#endif
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to queue audio sample: %s",
                     SDL_GetError());
    }

    return true;
}

int SdlAudioRenderer::getCapabilities()
{
    // Direct submit can't be used because we use LiGetPendingAudioDuration()
    return CAPABILITY_SUPPORTS_ARBITRARY_AUDIO_DURATION;
}
