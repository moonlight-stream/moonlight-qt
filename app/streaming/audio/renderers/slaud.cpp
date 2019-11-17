#include "slaud.h"

#include <SDL.h>

SLAudioRenderer::SLAudioRenderer()
    : m_AudioContext(nullptr),
      m_AudioStream(nullptr),
      m_AudioBuffer(nullptr)
{
    SLAudio_SetLogFunction(SLAudioRenderer::slLogCallback, nullptr);
}

bool SLAudioRenderer::prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
{
    m_AudioContext = SLAudio_CreateContext();
    if (m_AudioContext == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SLAudio_CreateContext() failed");
        return false;
    }

    // This number is pretty conservative (especially for surround), but
    // it's hard to avoid since we get crushed by CPU limitations.
    m_MaxQueuedAudioMs = 40 * opusConfig->channelCount / 2;

    m_FrameDuration = opusConfig->samplesPerFrame / 48;
    m_AudioBufferSize = opusConfig->samplesPerFrame * sizeof(short) * opusConfig->channelCount;
    m_AudioStream = SLAudio_CreateStream(m_AudioContext,
                                         opusConfig->sampleRate,
                                         opusConfig->channelCount,
                                         m_AudioBufferSize,
                                         1);
    if (m_AudioStream == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SLAudio_CreateStream() failed");
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using SLAudio renderer with %d ms frames",
                m_FrameDuration);

    return true;
}

#define SWAP_CHANNEL(i, j) \
    tmp = opusConfig->mapping[i]; \
    opusConfig->mapping[i] = opusConfig->mapping[j]; \
    opusConfig->mapping[j] = tmp

void SLAudioRenderer::remapChannels(POPUS_MULTISTREAM_CONFIGURATION opusConfig) {
    unsigned char tmp;

    if (opusConfig->channelCount == 6) {
        // The Moonlight's default channel order is FL,FR,C,LFE,RL,RR
        // SLAudio expects FL,C,FR,RL,RR,LFE so we swap the channels around to match

        // Swap FR and C - now FL,C,FR,LFE,RL,RR
        SWAP_CHANNEL(1, 2);

        // Swap LFE and RR - now FL,C,FR,RR,RL,LFE
        SWAP_CHANNEL(3, 5);

        // Swap RR and RL - now FL,C,FR,RL,RR,LFE
        SWAP_CHANNEL(4, 5);
    }
}

void* SLAudioRenderer::getAudioBuffer(int* size)
{
    SDL_assert(*size == m_AudioBufferSize);

    if (m_AudioBuffer == nullptr) {
        m_AudioBuffer = SLAudio_BeginFrame(m_AudioStream);
    }

    return m_AudioBuffer;
}

SLAudioRenderer::~SLAudioRenderer()
{
    if (m_AudioBuffer != nullptr) {
        memset(m_AudioBuffer, 0, m_AudioBufferSize);
        SLAudio_SubmitFrame(m_AudioStream);
    }

    if (m_AudioStream != nullptr) {
        SLAudio_FreeStream(m_AudioStream);
    }

    if (m_AudioContext != nullptr) {
        SLAudio_FreeContext(m_AudioContext);
    }
}

bool SLAudioRenderer::submitAudio(int bytesWritten)
{
    if (bytesWritten == 0) {
        // This buffer will be reused next time
        return true;
    }

    if (LiGetPendingAudioFrames() * m_FrameDuration < m_MaxQueuedAudioMs) {
        SLAudio_SubmitFrame(m_AudioStream);
        m_AudioBuffer = nullptr;
    }
    else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Too many queued audio frames: %d",
                    LiGetPendingAudioFrames());
    }

    return true;
}

int SLAudioRenderer::getCapabilities()
{
    return CAPABILITY_SLOW_OPUS_DECODER;
}

void SLAudioRenderer::slLogCallback(void*, ESLAudioLog logLevel, const char *message)
{
    SDL_LogPriority priority;

    switch (logLevel)
    {
    case k_ESLAudioLogError:
        priority = SDL_LOG_PRIORITY_ERROR;
        break;
    case k_ESLAudioLogWarning:
        priority = SDL_LOG_PRIORITY_WARN;
        break;
    case k_ESLAudioLogInfo:
        priority = SDL_LOG_PRIORITY_INFO;
        break;
    default:
    case k_ESLAudioLogDebug:
        priority = SDL_LOG_PRIORITY_DEBUG;
        break;
    }

    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION,
                   priority,
                   "SLAudio: %s",
                   message);
}
