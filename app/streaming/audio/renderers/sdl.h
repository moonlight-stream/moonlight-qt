#pragma once

#include "renderer.h"
#include "SDL_compat.h"

#include <atomic>

class SdlAudioRenderer : public IAudioRenderer
{
public:
    SdlAudioRenderer();

    virtual ~SdlAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void* getAudioBuffer(int* size);

    virtual bool submitAudio(int bytesWritten);

    virtual AudioFormat getAudioBufferFormat();

private:
    bool submitAudioLegacy(int bytesWritten);
    void trackBurst(int frames, Uint64 gapUs, Uint64 now);
    void queueSilence(int frames);

    static int SDLCALL driftMonitorThreadProc(void* context);
    void driftMonitorLoop();

    SDL_AudioDeviceID m_AudioDevice;
    void* m_AudioBuffer;
    Uint32 m_FrameSize;
    Uint32 m_FrameDurationMs;

    // Clock-drift control. See the comment at the top of sdlaud.cpp.
    bool m_DriftControl;
    int m_SampleRate;
    int m_Channels;
    int m_BytesPerFrame;
    int m_InitialMarginFrames;
    float* m_GlideBuffer;
    void* m_SilenceBuffer;
    int m_SilenceFrames;
    SDL_Thread* m_DriftMonitorThread;
    std::atomic<bool> m_StopDriftMonitor;

    // Written by submitAudio(), read by the monitor thread
    std::atomic<Uint64> m_LastPacketTime;
    std::atomic<int> m_BurstFrames;
    std::atomic<bool> m_RestartWindow;
    std::atomic<Uint32> m_Underruns;
    std::atomic<Uint32> m_FramesRemoved;
    std::atomic<Uint32> m_FramesInserted;
    std::atomic<Uint32> m_PacketsDropped;

    // Written by the monitor thread, read by submitAudio()
    std::atomic<int> m_TargetFrames;
    std::atomic<float> m_CorrectionPpm;

    // Used only by submitAudio()
    double m_GlideAccumulator;
    Uint64 m_PrevPacketTime;
    int m_CurrentBurstFrames;
    int m_BurstSamples[256];
    int m_BurstSampleCount;
    Uint64 m_BurstWindowStart;
};
