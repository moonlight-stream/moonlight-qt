#include "sdlmicrophonecapture.h"
#include <QDebug>
#include <QLoggingCategory>

SdlMicrophoneCapture::SdlMicrophoneCapture()
    : m_DeviceId(0)
    , m_IsInitialized(false)
    , m_IsCapturing(false)
    , m_HasCurrentBuffer(false)
    , m_SampleRate(48000)
    , m_Channels(1)
    , m_FrameSize(0)
{
    m_CurrentBuffer.data = nullptr;
    m_CurrentBuffer.size = 0;
}

SdlMicrophoneCapture::~SdlMicrophoneCapture()
{
    stopCapture();
    
    if (m_IsInitialized && m_DeviceId > 0) {
        SDL_CloseAudioDevice(m_DeviceId);
    }

    // Clean up any remaining buffers
    QMutexLocker locker(&m_BufferMutex);
    while (!m_BufferQueue.isEmpty()) {
        AudioBuffer buffer = m_BufferQueue.dequeue();
        delete[] buffer.data;
    }

    if (m_CurrentBuffer.data) {
        delete[] m_CurrentBuffer.data;
    }
}

bool SdlMicrophoneCapture::initialize(int sampleRate, int channels)
{
    if (m_IsInitialized) {
        return true;
    }

    m_SampleRate = sampleRate;
    m_Channels = channels;
    
    // Calculate frame size for buffer management
    m_FrameSize = (m_SampleRate * BUFFER_SIZE_MS) / 1000;

    // Initialize SDL audio subsystem if needed
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            qCWarning(QLoggingCategory("microphone")) << "Failed to initialize SDL audio:" << SDL_GetError();
            return false;
        }
    }

    // Set up audio specification
    SDL_zero(m_WantedSpec);
    m_WantedSpec.freq = m_SampleRate;
    m_WantedSpec.format = AUDIO_S16SYS; // 16-bit signed samples
    m_WantedSpec.channels = m_Channels;
    m_WantedSpec.samples = m_FrameSize; // Buffer size in samples
    m_WantedSpec.callback = audioCallback;
    m_WantedSpec.userdata = this;

    // Open audio capture device
    m_DeviceId = SDL_OpenAudioDevice(nullptr, 1, &m_WantedSpec, &m_ObtainedSpec, 
                                    SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);

    if (m_DeviceId == 0) {
        qCWarning(QLoggingCategory("microphone")) << "Failed to open audio capture device:" << SDL_GetError();
        return false;
    }

    // Log the obtained audio format
    qCInfo(QLoggingCategory("microphone")) << "SDL microphone capture initialized:";
    qCInfo(QLoggingCategory("microphone")) << "  Sample rate:" << m_ObtainedSpec.freq;
    qCInfo(QLoggingCategory("microphone")) << "  Channels:" << m_ObtainedSpec.channels;
    qCInfo(QLoggingCategory("microphone")) << "  Format:" << (m_ObtainedSpec.format == AUDIO_S16SYS ? "16-bit signed" : "other");
    qCInfo(QLoggingCategory("microphone")) << "  Buffer size:" << m_ObtainedSpec.samples;

    m_IsInitialized = true;
    return true;
}

bool SdlMicrophoneCapture::startCapture()
{
    if (!m_IsInitialized || m_IsCapturing) {
        return false;
    }

    // Start audio capture
    SDL_PauseAudioDevice(m_DeviceId, 0);
    m_IsCapturing = true;

    qCInfo(QLoggingCategory("microphone")) << "SDL microphone capture started";
    return true;
}

void SdlMicrophoneCapture::stopCapture()
{
    if (!m_IsCapturing) {
        return;
    }

    // Stop audio capture
    SDL_PauseAudioDevice(m_DeviceId, 1);
    m_IsCapturing = false;

    qCInfo(QLoggingCategory("microphone")) << "SDL microphone capture stopped";
}

bool SdlMicrophoneCapture::isCapturing()
{
    return m_IsCapturing;
}

IMicrophoneCapture::AudioFormat SdlMicrophoneCapture::getAudioFormat()
{
    return AudioFormat::Sint16NE; // SDL uses 16-bit signed samples
}

bool SdlMicrophoneCapture::getAudioBuffer(void** buffer, int* size)
{
    QMutexLocker locker(&m_BufferMutex);

    // Return current buffer if we have one
    if (m_HasCurrentBuffer) {
        return false; // Already have a buffer checked out
    }

    // Get next buffer from queue
    if (m_BufferQueue.isEmpty()) {
        return false; // No audio available
    }

    m_CurrentBuffer = m_BufferQueue.dequeue();
    m_HasCurrentBuffer = true;

    *buffer = m_CurrentBuffer.data;
    *size = m_CurrentBuffer.size;

    return true;
}

void SdlMicrophoneCapture::releaseAudioBuffer()
{
    QMutexLocker locker(&m_BufferMutex);

    if (!m_HasCurrentBuffer) {
        return;
    }

    // Free the current buffer
    if (m_CurrentBuffer.data) {
        delete[] m_CurrentBuffer.data;
        m_CurrentBuffer.data = nullptr;
        m_CurrentBuffer.size = 0;
    }

    m_HasCurrentBuffer = false;
}

int SdlMicrophoneCapture::getCapabilities()
{
    return 0; // No special capabilities
}

void SdlMicrophoneCapture::audioCallback(void* userdata, Uint8* stream, int len)
{
    SdlMicrophoneCapture* capture = static_cast<SdlMicrophoneCapture*>(userdata);
    capture->processAudioData(stream, len);
}

void SdlMicrophoneCapture::processAudioData(Uint8* stream, int len)
{
    QMutexLocker locker(&m_BufferMutex);

    // Drop buffers if queue is getting too full
    while (m_BufferQueue.size() >= MAX_QUEUED_BUFFERS) {
        AudioBuffer oldBuffer = m_BufferQueue.dequeue();
        delete[] oldBuffer.data;
    }

    // Create new buffer and copy audio data
    AudioBuffer newBuffer;
    newBuffer.size = len;
    newBuffer.data = new Uint8[len];
    memcpy(newBuffer.data, stream, len);

    // Add to queue
    m_BufferQueue.enqueue(newBuffer);
}