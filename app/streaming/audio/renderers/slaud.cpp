#include "slaud.h"

#include <SDL.h>

// To reduce CPU load on the Steam Link, we need to accumulate several frames
// before submitting for playback. Higher frames per submission saves more CPU
// but increases audio latency.
#define FRAMES_PER_SUBMISSION 4

SLAudioRenderer::SLAudioRenderer()
    : m_AudioContext(nullptr),
      m_AudioStream(nullptr),
      m_AudioBuffer(nullptr),
      m_AudioBufferBytesFilled(0)
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

    m_AudioBufferSize = SAMPLES_PER_FRAME * sizeof(short) * opusConfig->channelCount * FRAMES_PER_SUBMISSION;
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
                "Using SLAudio renderer");

    return true;
}

void* SLAudioRenderer::getAudioBuffer(int* size)
{
    SDL_assert(*size == m_AudioBufferSize / FRAMES_PER_SUBMISSION);

    if (m_AudioBuffer == nullptr) {
        m_AudioBufferBytesFilled = 0;
        m_AudioBuffer = (char*)SLAudio_BeginFrame(m_AudioStream);
        if (m_AudioBuffer == nullptr) {
            return nullptr;
        }
    }

    return (void*)&m_AudioBuffer[m_AudioBufferBytesFilled];
}

SLAudioRenderer::~SLAudioRenderer()
{
    if (m_AudioBuffer != nullptr) {
        // We had a buffer in flight when we quit. Just in case
        // SLAudio doesn't handle this properly, we'll zero and submit
        // it just to be safe.
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
    m_AudioBufferBytesFilled += bytesWritten;

    // Submit the buffer when it's full
    SDL_assert(m_AudioBufferBytesFilled <= m_AudioBufferSize);
    if (m_AudioBufferBytesFilled == m_AudioBufferSize) {
        SLAudio_SubmitFrame(m_AudioStream);
        m_AudioBuffer = nullptr;
        m_AudioBufferBytesFilled = 0;
    }

    return true;
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
