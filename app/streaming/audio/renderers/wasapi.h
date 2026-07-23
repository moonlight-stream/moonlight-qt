#pragma once

#include "renderer.h"

#include <memory>

class WasapiAudioRenderer : public IAudioRenderer
{
public:
    WasapiAudioRenderer();
    ~WasapiAudioRenderer() override;

    bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig) override;
    void* getAudioBuffer(int* size) override;
    bool submitAudio(int bytesWritten) override;
    AudioFormat getAudioBufferFormat() override;

private:
    class Impl;
    std::unique_ptr<Impl> m_Impl;
};
