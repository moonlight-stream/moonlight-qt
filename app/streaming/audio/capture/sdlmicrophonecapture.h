#pragma once

#include "microphonecapture.h"
#include <SDL.h>
#include <QMutex>
#include <QQueue>

/**
 * @brief SDL-based microphone capture implementation
 */
class SdlMicrophoneCapture : public IMicrophoneCapture
{
public:
    SdlMicrophoneCapture();
    virtual ~SdlMicrophoneCapture();

    // IMicrophoneCapture interface
    bool initialize(int sampleRate, int channels) override;
    bool startCapture() override;
    void stopCapture() override;
    bool isCapturing() override;
    AudioFormat getAudioFormat() override;
    bool getAudioBuffer(void** buffer, int* size) override;
    void releaseAudioBuffer() override;
    int getCapabilities() override;

private:
    static void audioCallback(void* userdata, Uint8* stream, int len);
    void processAudioData(Uint8* stream, int len);

    // SDL audio configuration
    SDL_AudioDeviceID m_DeviceId;
    SDL_AudioSpec m_WantedSpec;
    SDL_AudioSpec m_ObtainedSpec;
    bool m_IsInitialized;
    bool m_IsCapturing;

    // Audio buffer management
    struct AudioBuffer {
        Uint8* data;
        int size;
    };
    
    QQueue<AudioBuffer> m_BufferQueue;
    QMutex m_BufferMutex;
    AudioBuffer m_CurrentBuffer;
    bool m_HasCurrentBuffer;

    // Configuration
    int m_SampleRate;
    int m_Channels;
    int m_FrameSize;

    static constexpr int MAX_QUEUED_BUFFERS = 10;
    static constexpr int BUFFER_SIZE_MS = 20; // 20ms buffers
};