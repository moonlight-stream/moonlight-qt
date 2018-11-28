#pragma once

#include "renderer.h"

#include <soundio/soundio.h>

class SoundIoAudioRenderer : public IAudioRenderer
{
public:
    SoundIoAudioRenderer();

    ~SoundIoAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual bool submitAudio(short* audioBuffer, int audioSize);

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
    bool m_Errored;

    static const double k_RawSampleLengthSec;
};
