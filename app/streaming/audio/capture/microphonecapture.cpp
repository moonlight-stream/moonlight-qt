#include "microphonecapture.h"
#include "sdlmicrophonecapture.h"

#include <Limelight.h>
#include <SDL.h>
#include <QDebug>
#include <QHostAddress>
#include <QNetworkDatagram>

MicrophoneCapture::MicrophoneCapture(QObject *parent)
    : QObject(parent)
    , m_ServerPort(0)
    , m_Enabled(false)
    , m_IsStreaming(false)
    , m_AudioCapture(nullptr)
    , m_SampleRate(OPUS_SAMPLE_RATE)
    , m_Channels(OPUS_CHANNELS)
    , m_FrameSize(0)
    , m_OpusEncoder(nullptr)
    , m_EncodedBuffer(nullptr)
    , m_EncodedBufferSize(0)
    , m_UdpSocket(nullptr)
    , m_CaptureTimer(nullptr)
{
}

MicrophoneCapture::~MicrophoneCapture()
{
    cleanupResources();
}

bool MicrophoneCapture::initialize(const QString& serverAddress, int serverPort, 
                                 const STREAM_CONFIGURATION& streamConfig)
{
    m_ServerAddress = serverAddress;
    m_ServerPort = serverPort;

    // Calculate frame size for 20ms at 48kHz
    m_FrameSize = (OPUS_SAMPLE_RATE * OPUS_FRAME_MS) / 1000;

    // Initialize UDP socket
    m_UdpSocket = new QUdpSocket(this);
    if (!m_UdpSocket->bind()) {
        qCWarning(QLoggingCategory("microphone")) << "Failed to bind UDP socket for microphone";
        return false;
    }

    // Initialize capture timer
    m_CaptureTimer = new QTimer(this);
    m_CaptureTimer->setInterval(OPUS_FRAME_MS); // 20ms intervals
    connect(m_CaptureTimer, &QTimer::timeout, this, &MicrophoneCapture::captureAndStreamAudio);

    // Initialize Opus encoder
    if (!initializeOpusEncoder()) {
        qCWarning(QLoggingCategory("microphone")) << "Failed to initialize Opus encoder";
        return false;
    }

    // Create audio capture
    if (!createAudioCapture()) {
        qCWarning(QLoggingCategory("microphone")) << "Failed to create audio capture";
        return false;
    }

    qCInfo(QLoggingCategory("microphone")) << "Microphone capture initialized for" << serverAddress << ":" << serverPort;
    return true;
}

bool MicrophoneCapture::start()
{
    if (m_IsStreaming || !m_Enabled) {
        return false;
    }

    if (!m_AudioCapture || !m_OpusEncoder) {
        qCWarning(QLoggingCategory("microphone")) << "Microphone not properly initialized";
        return false;
    }

    // Start audio capture
    if (!m_AudioCapture->startCapture()) {
        qCWarning(QLoggingCategory("microphone")) << "Failed to start audio capture";
        return false;
    }

    // Start capture timer
    m_CaptureTimer->start();
    m_IsStreaming = true;

    qCInfo(QLoggingCategory("microphone")) << "Microphone streaming started";
    return true;
}

void MicrophoneCapture::stop()
{
    if (!m_IsStreaming) {
        return;
    }

    // Stop capture timer
    if (m_CaptureTimer) {
        m_CaptureTimer->stop();
    }

    // Stop audio capture
    if (m_AudioCapture) {
        m_AudioCapture->stopCapture();
    }

    m_IsStreaming = false;
    qCInfo(QLoggingCategory("microphone")) << "Microphone streaming stopped";
}

void MicrophoneCapture::setEnabled(bool enabled)
{
    if (m_Enabled == enabled) {
        return;
    }

    m_Enabled = enabled;
    
    if (!enabled && m_IsStreaming) {
        stop();
    }
    
    qCInfo(QLoggingCategory("microphone")) << "Microphone" << (enabled ? "enabled" : "disabled");
}

void MicrophoneCapture::captureAndStreamAudio()
{
    if (!m_AudioCapture || !m_OpusEncoder || !m_IsStreaming) {
        return;
    }

    void* audioBuffer = nullptr;
    int audioSize = 0;

    // Get audio buffer from capture device
    if (!m_AudioCapture->getAudioBuffer(&audioBuffer, &audioSize)) {
        return; // No audio available
    }

    // Encode and send the audio
    encodeAndSendAudio(audioBuffer, audioSize);

    // Release the buffer
    m_AudioCapture->releaseAudioBuffer();
}

bool MicrophoneCapture::createAudioCapture()
{
    // Try SDL audio capture first
    m_AudioCapture = new SdlMicrophoneCapture();
    
    if (m_AudioCapture->initialize(m_SampleRate, m_Channels)) {
        qCInfo(QLoggingCategory("microphone")) << "Using SDL microphone capture";
        return true;
    }

    delete m_AudioCapture;
    m_AudioCapture = nullptr;
    return false;
}

bool MicrophoneCapture::initializeOpusEncoder()
{
    int error;
    
    // Create Opus encoder for mono microphone
    m_OpusEncoder = opus_encoder_create(OPUS_SAMPLE_RATE, OPUS_CHANNELS, 
                                       OPUS_APPLICATION_VOIP, &error);
    
    if (error != OPUS_OK || !m_OpusEncoder) {
        qCWarning(QLoggingCategory("microphone")) << "Failed to create Opus encoder:" << opus_strerror(error);
        return false;
    }

    // Configure encoder
    opus_encoder_ctl(m_OpusEncoder, OPUS_SET_BITRATE(OPUS_BITRATE));
    opus_encoder_ctl(m_OpusEncoder, OPUS_SET_VBR(1));
    opus_encoder_ctl(m_OpusEncoder, OPUS_SET_COMPLEXITY(8));

    // Allocate encoding buffer
    m_EncodedBufferSize = MAX_PACKET_SIZE;
    m_EncodedBuffer = new unsigned char[m_EncodedBufferSize];

    qCInfo(QLoggingCategory("microphone")) << "Opus encoder initialized";
    return true;
}

void MicrophoneCapture::cleanupResources()
{
    stop();

    if (m_AudioCapture) {
        delete m_AudioCapture;
        m_AudioCapture = nullptr;
    }

    if (m_OpusEncoder) {
        opus_encoder_destroy(m_OpusEncoder);
        m_OpusEncoder = nullptr;
    }

    if (m_EncodedBuffer) {
        delete[] m_EncodedBuffer;
        m_EncodedBuffer = nullptr;
    }

    if (m_UdpSocket) {
        m_UdpSocket->close();
        delete m_UdpSocket;
        m_UdpSocket = nullptr;
    }

    if (m_CaptureTimer) {
        delete m_CaptureTimer;
        m_CaptureTimer = nullptr;
    }
}

bool MicrophoneCapture::encodeAndSendAudio(void* audioData, int audioSize)
{
    if (!m_OpusEncoder || !m_EncodedBuffer || !m_UdpSocket) {
        return false;
    }

    // Calculate expected samples for this frame
    int expectedSamples = m_FrameSize * m_Channels;
    int bytesPerSample = sizeof(short); // Assuming 16-bit samples
    int expectedBytes = expectedSamples * bytesPerSample;

    // Ensure we have the right amount of data
    if (audioSize < expectedBytes) {
        qCDebug(QLoggingCategory("microphone")) << "Insufficient audio data:" << audioSize << "expected:" << expectedBytes;
        return false;
    }

    // Encode audio with Opus
    int encodedBytes = opus_encode(m_OpusEncoder, 
                                  static_cast<const opus_int16*>(audioData),
                                  m_FrameSize,
                                  m_EncodedBuffer,
                                  m_EncodedBufferSize);

    if (encodedBytes <= 0) {
        qCWarning(QLoggingCategory("microphone")) << "Opus encoding failed:" << opus_strerror(encodedBytes);
        return false;
    }

    // Create packet header (simple format for now)
    // TODO: Add proper packet header with sequence numbers, timestamps
    QByteArray packet;
    packet.append(reinterpret_cast<const char*>(m_EncodedBuffer), encodedBytes);

    // Send to server
    qint64 sent = m_UdpSocket->writeDatagram(packet, QHostAddress(m_ServerAddress), m_ServerPort);
    
    if (sent != packet.size()) {
        qCWarning(QLoggingCategory("microphone")) << "Failed to send audio packet:" << sent << "/" << packet.size();
        return false;
    }

    qCDebug(QLoggingCategory("microphone")) << "Sent audio packet:" << encodedBytes << "bytes";
    return true;
}