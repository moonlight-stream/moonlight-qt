#pragma once

#include <QtGlobal>
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <opus.h>

// Forward declarations
struct STREAM_CONFIGURATION;
class NvComputer;

/**
 * @brief Interface for microphone capture implementations
 */
class IMicrophoneCapture
{
public:
    virtual ~IMicrophoneCapture() {}

    /**
     * @brief Initialize microphone capture with given sample rate and channels
     * @param sampleRate Sample rate (48000 Hz recommended for Opus)
     * @param channels Number of channels (1 for mono, 2 for stereo)
     * @return true if initialization successful
     */
    virtual bool initialize(int sampleRate, int channels) = 0;

    /**
     * @brief Start capturing audio from microphone
     * @return true if capture started successfully
     */
    virtual bool startCapture() = 0;

    /**
     * @brief Stop capturing audio
     */
    virtual void stopCapture() = 0;

    /**
     * @brief Check if microphone is currently capturing
     * @return true if capturing
     */
    virtual bool isCapturing() = 0;

    /**
     * @brief Get audio buffer format
     */
    enum class AudioFormat {
        Sint16NE,  // 16-bit signed integer (native endian)
        Float32NE, // 32-bit floating point (native endian)
    };
    virtual AudioFormat getAudioFormat() = 0;

    /**
     * @brief Get the next audio buffer for encoding
     * @param buffer Pointer to buffer data
     * @param size Size of buffer in bytes
     * @return true if buffer available
     */
    virtual bool getAudioBuffer(void** buffer, int* size) = 0;

    /**
     * @brief Release the audio buffer after processing
     */
    virtual void releaseAudioBuffer() = 0;

    /**
     * @brief Get microphone capabilities
     */
    virtual int getCapabilities() = 0;

    int getAudioBufferSampleSize() {
        switch (getAudioFormat()) {
        case IMicrophoneCapture::AudioFormat::Sint16NE:
            return sizeof(short);
        case IMicrophoneCapture::AudioFormat::Float32NE:
            return sizeof(float);
        default:
            Q_UNREACHABLE();
        }
    }
};

/**
 * @brief Microphone capture manager that handles encoding and streaming
 */
class MicrophoneCapture : public QObject
{
    Q_OBJECT

public:
    explicit MicrophoneCapture(QObject *parent = nullptr);
    virtual ~MicrophoneCapture();

    /**
     * @brief Initialize microphone streaming to server
     * @param serverAddress Server IP address
     * @param serverPort Microphone stream port (13)
     * @param streamConfig Stream configuration
     * @return true if initialization successful
     */
    bool initialize(const QString& serverAddress, int serverPort, 
                   const STREAM_CONFIGURATION& streamConfig);

    /**
     * @brief Start microphone capture and streaming
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop microphone capture and streaming
     */
    void stop();

    /**
     * @brief Check if microphone is enabled and streaming
     * @return true if streaming
     */
    bool isStreaming() const { return m_IsStreaming; }

    /**
     * @brief Enable or disable microphone capture
     * @param enabled true to enable microphone
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if microphone is enabled
     * @return true if enabled
     */
    bool isEnabled() const { return m_Enabled; }

private slots:
    void captureAndStreamAudio();

private:
    bool createAudioCapture();
    bool initializeOpusEncoder();
    void cleanupResources();
    bool encodeAndSendAudio(void* audioData, int audioSize);

    // Configuration
    QString m_ServerAddress;
    int m_ServerPort;
    bool m_Enabled;
    bool m_IsStreaming;

    // Audio capture
    IMicrophoneCapture* m_AudioCapture;
    int m_SampleRate;
    int m_Channels;
    int m_FrameSize;

    // Opus encoding
    OpusEncoder* m_OpusEncoder;
    unsigned char* m_EncodedBuffer;
    int m_EncodedBufferSize;

    // Network streaming
    QUdpSocket* m_UdpSocket;
    QTimer* m_CaptureTimer;

    // Constants
    static constexpr int OPUS_SAMPLE_RATE = 48000;
    static constexpr int OPUS_CHANNELS = 1; // Mono for microphone
    static constexpr int OPUS_FRAME_MS = 20; // 20ms frames
    static constexpr int OPUS_BITRATE = 64000; // 64 kbps
    static constexpr int MAX_PACKET_SIZE = 4000;
};