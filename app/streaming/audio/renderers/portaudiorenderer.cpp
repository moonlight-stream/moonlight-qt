#include "portaudiorenderer.h"

#include <SDL.h>

#include <atomic>

PortAudioRenderer::PortAudioRenderer()
    : m_Stream(nullptr),
      m_ChannelCount(0),
      m_WriteIndex(0),
      m_ReadIndex(0)
{
    PaError error = Pa_Initialize();
    if (error != paNoError) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Pa_Initialize() failed: %s",
                     Pa_GetErrorText(error));
        return;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Initialized PortAudio: %s",
                Pa_GetVersionText());
}

PortAudioRenderer::~PortAudioRenderer()
{
    if (m_Stream != nullptr) {
        Pa_CloseStream(m_Stream);
    }

    Pa_Terminate();
}

bool PortAudioRenderer::prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
{
    PaStreamParameters params = {};

    m_ChannelCount = opusConfig->channelCount;

    PaDeviceIndex outputDeviceIndex = Pa_GetDefaultOutputDevice();
    if (outputDeviceIndex == paNoDevice) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "No output device available");
        return false;
    }

    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(outputDeviceIndex);
    if (deviceInfo == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Pa_GetDeviceInfo() failed");
        return false;
    }

    params.channelCount = opusConfig->channelCount;
    params.sampleFormat = paInt16;
    params.device = outputDeviceIndex;
    params.suggestedLatency = deviceInfo->defaultLowOutputLatency;

    PaError error = Pa_OpenStream(&m_Stream, nullptr, &params,
                                  opusConfig->sampleRate,
                                  SAMPLES_PER_FRAME,
                                  paNoFlag,
                                  paStreamCallback, this);
    if (error != paNoError) {
        m_Stream = nullptr;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Pa_OpenStream() failed: %s",
                     Pa_GetErrorText(error));
        return false;
    }

    error = Pa_StartStream(m_Stream);
    if (error != paNoError) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Pa_StartStream() failed: %s",
                     Pa_GetErrorText(error));
        return false;
    }

    return true;
}

void PortAudioRenderer::submitAudio(short* audioBuffer, int audioSize)
{
    SDL_assert(audioSize == SAMPLES_PER_FRAME * m_ChannelCount * 2);

    // Check if there is space for this sample in the buffer. Again, this can race
    // but in the worst case, we'll not see the sample callback having consumed a sample.
    if (((m_WriteIndex + 1) % CIRCULAR_BUFFER_SIZE) == m_ReadIndex) {
        return;
    }

    SDL_memcpy(&m_AudioBuffer[m_WriteIndex * CIRCULAR_BUFFER_STRIDE], audioBuffer, audioSize);

    // Fence with release semantics ensures m_AudioBuffer[m_WriteIndex] is written before the
    // consumer observes m_WriteIndex incrementing.
    std::atomic_thread_fence(std::memory_order_release);

    // This can race with the reader in the sample callback, however this is a benign
    // race since we'll either read the original value of m_WriteIndex (which is safe,
    // we just won't consider this sample) or the new value of m_WriteIndex
    m_WriteIndex = (m_WriteIndex + 1) % CIRCULAR_BUFFER_SIZE;
}

bool PortAudioRenderer::testAudio(int audioConfiguration)
{
    PaStreamParameters params = {};

    PaDeviceIndex outputDeviceIndex = Pa_GetDefaultOutputDevice();
    if (outputDeviceIndex == paNoDevice) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "No output device available");
        return false;
    }

    switch (audioConfiguration)
    {
    case AUDIO_CONFIGURATION_STEREO:
        params.channelCount = 2;
        break;
    case AUDIO_CONFIGURATION_51_SURROUND:
        params.channelCount = 6;
        break;
    default:
        SDL_assert(false);
        return false;
    }

    params.sampleFormat = paInt16;
    params.device = outputDeviceIndex;

    PaError error = Pa_IsFormatSupported(nullptr, &params, 48000);
    if (error == paFormatIsSupported) {
        return true;
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Pa_IsFormatSupported() failed: %s",
                     Pa_GetErrorText(error));
        return false;
    }
}

int PortAudioRenderer::detectAudioConfiguration()
{
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(Pa_GetDefaultOutputDevice());
    if (deviceInfo == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Pa_GetDeviceInfo() failed");
        return false;
    }

    // PulseAudio reports max output channels that don't
    // correspond to any output devices (32 channels), so
    // only use 5.1 surround sound if the output channel count
    // is reasonable. Additionally, PortAudio doesn't do remixing
    // for quadraphonic, so only use 5.1 if we have 6 or more channels.
    if (deviceInfo->maxOutputChannels == 6 || deviceInfo->maxOutputChannels == 8) {
        return AUDIO_CONFIGURATION_51_SURROUND;
    }
    else {
        return AUDIO_CONFIGURATION_STEREO;
    }
}

int PortAudioRenderer::paStreamCallback(const void*, void* output, unsigned long frameCount, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* userData)
{
    auto me = reinterpret_cast<PortAudioRenderer*>(userData);

    SDL_assert(frameCount == SAMPLES_PER_FRAME);

    // If the indexes aren't equal, we have a sample
    if (me->m_WriteIndex != me->m_ReadIndex) {
        // Copy data to the audio buffer
        SDL_memcpy(output,
                   &me->m_AudioBuffer[me->m_ReadIndex * CIRCULAR_BUFFER_STRIDE],
                   frameCount * me->m_ChannelCount * sizeof(short));

        // Fence with acquire semantics ensures m_AudioBuffer[m_ReadIndex] is read before the
        // producer observes m_ReadIndex incrementing.
        std::atomic_thread_fence(std::memory_order_acquire);

        // This can race with the reader in the submitAudio function. This is not a problem
        // because at worst, it just won't see that we've consumed this sample yet.
        me->m_ReadIndex = (me->m_ReadIndex + 1) % CIRCULAR_BUFFER_SIZE;
    }
    else {
        // No data, so play silence
        SDL_memset(output, 0, frameCount * me->m_ChannelCount * sizeof(short));
    }

    return paContinue;
}
