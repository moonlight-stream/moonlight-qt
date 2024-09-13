#pragma once

#include "renderer.h"

#include <soundio/soundio.h>

class SoundIoAudioRenderer : public IAudioRenderer
{
public:
    SoundIoAudioRenderer();

    ~SoundIoAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void* getAudioBuffer(int* size);

    virtual bool submitAudio(int bytesWritten);

    virtual int getCapabilities();

    virtual AudioFormat getAudioBufferFormat();

    const char * getRendererName() {
        if (m_SoundIo != nullptr) {
            const char *backend = soundio_backend_name(m_SoundIo->current_backend);
            snprintf(m_Name, 12 + strlen(backend), "libsoundio/%s", backend );
        }
        return m_Name;
    }

private:
    int scoreChannelLayout(const struct SoundIoChannelLayout* layout, const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    static void sioErrorCallback(struct SoundIoOutStream* stream, int err);

    static void sioWriteCallback(struct SoundIoOutStream* stream, int frameCountMin, int frameCountMax);

    static void sioBackendDisconnect(struct SoundIo* soundio, int err);

    static void sioDevicesChanged(SoundIo* soundio);

    int m_OpusChannelCount;
    struct SoundIo* m_SoundIo;
    struct SoundIoDevice* m_Device;
    struct SoundIoOutStream* m_OutputStream;
    struct SoundIoRingBuffer* m_RingBuffer;
    struct SoundIoChannelLayout m_EffectiveLayout;
    double m_AudioPacketDuration;
    double m_Latency;
    bool m_Errored;
    char m_Name[24];
};
