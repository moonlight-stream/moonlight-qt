#include "sdl.h"

#include <Limelight.h>
#include <SDL.h>

#include <QtGlobal>

#define MIN_QUEUED_FRAMES 2
#define MAX_QUEUED_FRAMES 4
#define STOP_THE_WORLD_LIMIT 20
#define DROP_RATIO_DENOM 32

// This isn't accurate on macOS and Linux (PulseAudio),
// since they both report supporting a large number of
// channels, regardless of the actual output device.
int SdlAudioRenderer::detectAudioConfiguration() const
{
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;

    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16;
    want.channels = 6;
    want.samples = 1024;

    // Try to open for 5.1 surround sound, but allow SDL to tell us that's
    // not available.
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (dev == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open audio device");
        // We'll probably have issues during audio stream init, but we'll
        // try anyway
        return AUDIO_CONFIGURATION_STEREO;
    }

    SDL_CloseAudioDevice(dev);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio device has %d channels", have.channels);

    if (have.channels > 2) {
        // We don't support quadraphonic or 7.1 surround, but SDL
        // should be able to downmix or upmix better from 5.1 than
        // from stereo, so use 5.1 in non-stereo cases.
        return AUDIO_CONFIGURATION_51_SURROUND;
    }
    else {
        return AUDIO_CONFIGURATION_STEREO;
    }
}

bool SdlAudioRenderer::testAudio(int audioConfiguration) const
{
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;

    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16;
    want.samples = SAMPLES_PER_FRAME;

    switch (audioConfiguration) {
    case AUDIO_CONFIGURATION_STEREO:
        want.channels = 2;
        break;
    case AUDIO_CONFIGURATION_51_SURROUND:
        want.channels = 6;
        break;
    default:
        SDL_assert(false);
        return false;
    }

    // Test audio device for functionality
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Audio test - Failed to open audio device: %s",
                     SDL_GetError());
        return false;
    }

    SDL_CloseAudioDevice(dev);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio test - Successful with %d channels",
                want.channels);
    return true;
}

SdlAudioRenderer::SdlAudioRenderer()
    : m_AudioDevice(0),
      m_ChannelCount(0),
      m_PendingDrops(0),
      m_PendingHardDrops(0),
      m_SampleIndex(0),
      m_BaselinePendingData(0)
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
    SDL_AudioSpec want, have;

    SDL_zero(want);
    want.freq = opusConfig->sampleRate;
    want.format = AUDIO_S16;
    want.channels = opusConfig->channelCount;

    // This is supposed to be a power of 2, but our
    // frames contain a non-power of 2 number of samples,
    // so the slop would require buffering another full frame.
    // Specifying non-Po2 seems to work for our supported platforms.
    want.samples = SAMPLES_PER_FRAME;

    m_AudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (m_AudioDevice == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open audio device: %s",
                     SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Desired audio buffer: %u samples (%lu bytes)",
                want.samples,
                want.samples * sizeof(short) * want.channels);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Obtained audio buffer: %u samples (%u bytes)",
                have.samples,
                have.size);

    // SDL counts pending samples in the queued
    // audio size using the WASAPI backend. This
    // includes silence, which can throw off our
    // pending data count. Get a baseline so we
    // can exclude that data.
    m_BaselinePendingData = 0;
#ifdef Q_OS_WIN32
    for (int i = 0; i < 100; i++) {
        m_BaselinePendingData = qMax(m_BaselinePendingData, SDL_GetQueuedAudioSize(m_AudioDevice));
        SDL_Delay(10);
    }
#endif
    m_BaselinePendingData *= 2;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Baseline pending audio data: %d bytes",
                m_BaselinePendingData);

    m_ChannelCount = opusConfig->channelCount;
    m_SampleIndex = 0;
    m_PendingDrops = m_PendingHardDrops = 0;

    // Start playback
    SDL_PauseAudioDevice(m_AudioDevice, 0);

    return true;
}

SdlAudioRenderer::~SdlAudioRenderer()
{
    if (m_AudioDevice != 0) {
        // Stop playback
        SDL_PauseAudioDevice(m_AudioDevice, 1);
        SDL_CloseAudioDevice(m_AudioDevice);
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_assert(!SDL_WasInit(SDL_INIT_AUDIO));
}

void SdlAudioRenderer::submitAudio(short* audioBuffer, int audioSize)
{
    m_SampleIndex++;

    Uint32 queuedAudio = SDL_GetQueuedAudioSize(m_AudioDevice);
    if (queuedAudio > m_BaselinePendingData) {
        queuedAudio -= m_BaselinePendingData;
    }
    else {
        queuedAudio = 0;
    }

    Uint32 framesQueued = queuedAudio / (SAMPLES_PER_FRAME * m_ChannelCount * sizeof(short));

    // We must check this prior to the below checks to ensure we don't
    // underflow if framesQueued - m_PendingHardDrops < 0.
    if (framesQueued <= MIN_QUEUED_FRAMES) {
        m_PendingDrops = m_PendingHardDrops = 0;
    }
    // Pend enough drops to get us back to MIN_QUEUED_FRAMES, checking first
    // to ensure we don't underflow.
    else if (framesQueued > m_PendingHardDrops &&
             framesQueued - m_PendingHardDrops > STOP_THE_WORLD_LIMIT) {
        m_PendingHardDrops = framesQueued - MIN_QUEUED_FRAMES;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Pending hard drop of %u audio frames",
                    m_PendingHardDrops);
    }
    // If we're under the stop the world limit, we can drop samples
    // gracefully over the next little while.
    else if (framesQueued > m_PendingHardDrops + m_PendingDrops &&
             framesQueued - m_PendingHardDrops - m_PendingDrops > MAX_QUEUED_FRAMES) {
        m_PendingDrops = framesQueued - MIN_QUEUED_FRAMES;
    }

    // Determine if this frame should be dropped
    if (m_PendingHardDrops != 0) {
        // Hard drops happen all at once to forcefully
        // resync with the source.
        m_PendingHardDrops--;
        return;
    }
    else if (m_PendingDrops != 0 && m_SampleIndex % DROP_RATIO_DENOM == 0) {
        // Normal drops are interspersed with the audio data
        // to hide the glitches.
        m_PendingDrops--;
        return;
    }

    if (SDL_QueueAudio(m_AudioDevice, audioBuffer, audioSize) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to queue audio sample: %s",
                     SDL_GetError());
    }
}
