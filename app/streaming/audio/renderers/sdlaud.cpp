#include "sdl.h"

#include <Limelight.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <vector>

// ------------------------------------------------------------------------------------------------
// Clock-drift control
//
// The host renders audio on its own clock and this PC plays it on another. Two crystals never
// tick at exactly the same rate; 10-50 ppm apart is normal. With a plain FIFO, the audio queued in
// front of the output device slowly grows when the host runs faster (sound falls further and
// further behind the picture: 15 ppm is about 53 ms per hour) or drains when it runs slower
// (periodic underruns). The old limits below only capped the growth, at roughly 80 ms.
//
// A monitor thread samples SDL's queue every millisecond and averages it over one-second windows.
// By Little's law, the time-averaged queue level divided by the sample rate is the average time a
// sample spends in the queue, so holding that average at a fixed target holds the latency
// constant. A proportional controller turns the error into a small speed correction (100 ppm per
// ms of error, at most 4000 ppm), and submitAudio() applies it by occasionally producing one frame
// fewer, or one more, from a decoded packet. That packet is resampled with a cubic (Catmull-Rom)
// interpolator, so each correction is a 0.4% speed glide lasting one packet (5 ms), not a click.
// At a typical drift that is one glide every second or two.
//
// SDL pulls a whole device period at once, and hosts send audio in bursts (often two 5 ms packets
// per 10 ms capture period), so the queue swings by those amounts even when latency is constant.
// The lowest average that never underruns on a perfectly regular network is half their sum, so
//
//     target = (device pull size + host burst size) / 2 + safety margin
//
// Both sizes are measured: pulls by the monitor (each drop in the queue level), bursts by
// submitAudio() (packets that arrive within 2 ms of each other). The margin covers network and
// scheduling jitter. It starts at ML_AUDIO_MARGIN_MS (default 5 ms) and grows by 2 ms whenever the
// queue runs dry while packets are flowing. After 15 minutes without that happening, it steps back
// toward the starting value by 0.5 ms every 5 minutes. With two nearly matched clocks, the queue's
// low point sweeps through a whole device period every several minutes (about every 11 at a 15 ppm
// mismatch), so a raised margin has to outlast that cycle to protect the next low point.
//
// A queue more than 30 ms above target (a backlog after a network stall) is cut back by dropping
// whole packets, since gliding would take too long. When packets resume after more than 100 ms
// without any (the host stops sending during silence), the queue is primed with silence up to the
// target so the first pulls cannot underrun.
//
// ML_AUDIO_DRIFT_CONTROL=0 restores the previous behavior.
// ------------------------------------------------------------------------------------------------

namespace {

constexpr double kGainPpmPerMs = 100.0;     // gives a time constant of about 10 s
constexpr double kMaxCorrectionPpm = 4000.0; // just under one frame per 5 ms packet
constexpr int kDefaultMarginMs = 5;
constexpr int kMarginStepMs = 2;
constexpr int kMaxMarginMs = 20;
constexpr int kMarginHoldSeconds = 900;
constexpr int kMarginDecaySeconds = 300;
constexpr int kHardCapAboveTargetMs = 30;
constexpr int kHistorySeconds = 600;
constexpr int kStatsSeconds = 60;
constexpr Uint64 kRestartGapUs = 100000;
constexpr Uint64 kSameBurstGapUs = 2000;

// Resamples one packet of interleaved float audio from inFrames to inFrames + delta frames
// (delta is +1 or -1) by sweeping the read position evenly from the first input frame to the
// last. The first and last output frames are exact copies of the first and last input frames,
// so consecutive packets stay continuous and only the interior is interpolated.
void glidePacket(const float* in, float* out, int inFrames, int channels, int delta)
{
    const int outFrames = inFrames + delta;
    const double step = (double)(inFrames - 1) / (double)(outFrames - 1);

    for (int j = 0; j < outFrames; j++) {
        const double pos = j * step;
        int k = (int)pos;
        float t = (float)(pos - k);
        if (k >= inFrames - 1) {
            k = inFrames - 1;
            t = 0.0f;
        }

        const float* p0 = in + (k > 0 ? k - 1 : 0) * channels;
        const float* p1 = in + k * channels;
        const float* p2 = in + (k + 1 < inFrames ? k + 1 : inFrames - 1) * channels;
        const float* p3 = in + (k + 2 < inFrames ? k + 2 : inFrames - 1) * channels;

        const float t2 = t * t;
        const float t3 = t2 * t;
        const float w0 = -0.5f * t3 + t2 - 0.5f * t;
        const float w1 = 1.5f * t3 - 2.5f * t2 + 1.0f;
        const float w2 = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
        const float w3 = 0.5f * t3 - 0.5f * t2;

        float* o = out + j * channels;
        for (int c = 0; c < channels; c++) {
            o[c] = w0 * p0[c] + w1 * p1[c] + w2 * p2[c] + w3 * p3[c];
        }
    }
}

}

SdlAudioRenderer::SdlAudioRenderer()
    : m_AudioDevice(0),
      m_AudioBuffer(nullptr),
      m_FrameSize(0),
      m_FrameDurationMs(0),
      m_DriftControl(false),
      m_SampleRate(0),
      m_Channels(0),
      m_BytesPerFrame(0),
      m_InitialMarginFrames(0),
      m_GlideBuffer(nullptr),
      m_SilenceBuffer(nullptr),
      m_SilenceFrames(0),
      m_DriftMonitorThread(nullptr),
      m_StopDriftMonitor(false),
      m_LastPacketTime(0),
      m_BurstFrames(0),
      m_RestartWindow(false),
      m_Underruns(0),
      m_FramesRemoved(0),
      m_FramesInserted(0),
      m_PacketsDropped(0),
      m_TargetFrames(0),
      m_CorrectionPpm(0.0f),
      m_GlideAccumulator(0.0),
      m_PrevPacketTime(0),
      m_CurrentBurstFrames(0),
      m_BurstSampleCount(0),
      m_BurstWindowStart(0)
{
    SDL_assert(!SDL_WasInit(SDL_INIT_AUDIO));

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s",
                     SDL_GetError());
        SDL_assert(SDL_WasInit(SDL_INIT_AUDIO));
    }
}

bool SdlAudioRenderer::prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
{
    SDL_AudioSpec want, have;

    SDL_zero(want);
    want.freq = opusConfig->sampleRate;
    want.format = AUDIO_F32SYS;
    want.channels = opusConfig->channelCount;

    // On PulseAudio systems, setting a value too small can cause underruns for other
    // applications sharing this output device. We impose a floor of 480 samples (10 ms)
    // to mitigate this issue. Otherwise, we will buffer up to 3 frames of audio which
    // is 15 ms at regular 5 ms frames and 30 ms at 10 ms frames for slow connections.
    // The buffering helps avoid audio underruns due to network jitter.
    want.samples = SDL_max(480, opusConfig->samplesPerFrame * 3);

    m_FrameDurationMs = opusConfig->samplesPerFrame / (opusConfig->sampleRate / 1000);
    m_FrameSize = opusConfig->samplesPerFrame *
                  opusConfig->channelCount *
                  getAudioBufferSampleSize();

    m_AudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (m_AudioDevice == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open audio device: %s",
                     SDL_GetError());
        return false;
    }

    m_AudioBuffer = SDL_malloc(m_FrameSize);
    if (m_AudioBuffer == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to allocate audio buffer");
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Desired audio buffer: %u samples (%u bytes)",
                want.samples,
                want.samples * want.channels * getAudioBufferSampleSize());

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Obtained audio buffer: %u samples (%u bytes)",
                have.samples,
                have.size);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "SDL audio driver: %s",
                SDL_GetCurrentAudioDriver());

    m_SampleRate = opusConfig->sampleRate;
    m_Channels = opusConfig->channelCount;
    m_BytesPerFrame = opusConfig->channelCount * getAudioBufferSampleSize();

    // The glide resampler works on float samples, which is all this renderer produces.
    const char* driftEnv = SDL_getenv("ML_AUDIO_DRIFT_CONTROL");
    m_DriftControl = (driftEnv == nullptr || SDL_atoi(driftEnv) != 0) &&
                     getAudioBufferFormat() == AudioFormat::Float32NE;
    if (m_DriftControl) {
        int marginMs = kDefaultMarginMs;
        const char* marginEnv = SDL_getenv("ML_AUDIO_MARGIN_MS");
        if (marginEnv != nullptr) {
            marginMs = SDL_clamp(SDL_atoi(marginEnv), 1, kMaxMarginMs);
        }
        m_InitialMarginFrames = marginMs * m_SampleRate / 1000;

        // Until the monitor has measured them, assume 10 ms device pulls and 10 ms host
        // bursts (the usual shared-mode device period and host capture period).
        m_BurstFrames = m_SampleRate / 100;
        m_TargetFrames = m_SampleRate / 100 + m_InitialMarginFrames;

        m_GlideBuffer = (float*)SDL_malloc(m_FrameSize + m_BytesPerFrame);
        m_SilenceFrames = m_SampleRate * 60 / 1000;
        m_SilenceBuffer = SDL_calloc(m_SilenceFrames, m_BytesPerFrame);
        if (m_GlideBuffer == nullptr || m_SilenceBuffer == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to allocate audio drift control buffers");
            m_DriftControl = false;
        }
    }

    // Start playback
    SDL_PauseAudioDevice(m_AudioDevice, 0);

    if (m_DriftControl) {
        m_DriftMonitorThread = SDL_CreateThread(SdlAudioRenderer::driftMonitorThreadProc,
                                                "AudioDrift", this);
        if (m_DriftMonitorThread == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create audio drift monitor thread: %s",
                         SDL_GetError());
            m_DriftControl = false;
        }
    }

    if (m_DriftControl) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Audio drift control enabled: holding SDL's queue at (device pull + host burst) / 2 + %d ms",
                    m_InitialMarginFrames * 1000 / m_SampleRate);
    }
    else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Audio drift control disabled");
    }

    return true;
}

SdlAudioRenderer::~SdlAudioRenderer()
{
    // The monitor polls the device, so it must be gone before the device is closed
    if (m_DriftMonitorThread != nullptr) {
        m_StopDriftMonitor = true;
        SDL_WaitThread(m_DriftMonitorThread, nullptr);
    }

    if (m_AudioDevice != 0) {
        // Stop playback
        SDL_PauseAudioDevice(m_AudioDevice, 1);
        SDL_CloseAudioDevice(m_AudioDevice);
    }

    if (m_AudioBuffer != nullptr) {
        SDL_free(m_AudioBuffer);
    }

    if (m_GlideBuffer != nullptr) {
        SDL_free(m_GlideBuffer);
    }

    if (m_SilenceBuffer != nullptr) {
        SDL_free(m_SilenceBuffer);
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_assert(!SDL_WasInit(SDL_INIT_AUDIO));
}

void* SdlAudioRenderer::getAudioBuffer(int*)
{
    return m_AudioBuffer;
}

bool SdlAudioRenderer::submitAudio(int bytesWritten)
{
    if (bytesWritten == 0) {
        // Nothing to do
        return true;
    }

    // Don't queue if there's already more than 30 ms of audio data waiting
    // in Moonlight's audio queue.
    if (LiGetPendingAudioDuration() > 30) {
        return true;
    }

    if (!m_DriftControl) {
        return submitAudioLegacy(bytesWritten);
    }

    // Our device may enter a permanent error status upon removal, so we need
    // to recreate the audio device to pick up the new default audio device.
    if (SDL_GetAudioDeviceStatus(m_AudioDevice) == SDL_AUDIO_STOPPED) {
        return false;
    }

    const Uint64 now = SDL_GetPerformanceCounter();
    const Uint64 gapUs = m_PrevPacketTime == 0 ?
        UINT64_MAX : (now - m_PrevPacketTime) * 1000000 / SDL_GetPerformanceFrequency();
    m_PrevPacketTime = now;

    const int frames = bytesWritten / m_BytesPerFrame;
    const int queued = (int)(SDL_GetQueuedAudioSize(m_AudioDevice) / (Uint32)m_BytesPerFrame);
    const int target = m_TargetFrames.load(std::memory_order_relaxed);

    trackBurst(frames, gapUs, now);

    if (gapUs > kRestartGapUs) {
        // First packet, or audio resumed after the host stopped sending (it does during
        // silence). Prime the queue to the target so the first pulls can't run dry, and
        // have the monitor start a fresh measurement.
        if (queued < target) {
            queueSilence(target - queued);
        }
        m_GlideAccumulator = 0.0;
        m_RestartWindow = true;
    }
    else if (queued == 0) {
        // SDL used up everything we had while packets were still flowing
        m_Underruns++;
    }
    else if (queued > target + kHardCapAboveTargetMs * m_SampleRate / 1000) {
        // A backlog, typically right after a network stall. Gliding it away would take too long.
        m_PacketsDropped++;
        m_LastPacketTime = now;
        return true;
    }

    m_LastPacketTime = now;

    const void* data = m_AudioBuffer;
    int bytes = bytesWritten;

    // Apply the speed correction the monitor asked for, at most one frame per packet
    m_GlideAccumulator += m_CorrectionPpm.load(std::memory_order_relaxed) * 1e-6 * frames;
    m_GlideAccumulator = SDL_clamp(m_GlideAccumulator, -1.5, 1.5);
    if (frames >= 32) {
        int delta = 0;
        if (m_GlideAccumulator >= 1.0) {
            delta = -1;
            m_GlideAccumulator -= 1.0;
            m_FramesRemoved++;
        }
        else if (m_GlideAccumulator <= -1.0) {
            delta = 1;
            m_GlideAccumulator += 1.0;
            m_FramesInserted++;
        }

        if (delta != 0) {
            glidePacket((const float*)m_AudioBuffer, m_GlideBuffer, frames, m_Channels, delta);
            data = m_GlideBuffer;
            bytes = (frames + delta) * m_BytesPerFrame;
        }
    }

    if (SDL_QueueAudio(m_AudioDevice, data, (Uint32)bytes) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to queue audio sample: %s",
                     SDL_GetError());
    }

    return true;
}

bool SdlAudioRenderer::submitAudioLegacy(int bytesWritten)
{
    // Provide backpressure on the queue to ensure too many frames don't build up
    // in SDL's audio queue, but don't wait forever to avoid a deadlock if the
    // audio device fails.
    for (int i = 0; i < 100; i++) {
        // Our device may enter a permanent error status upon removal, so we need
        // to recreate the audio device to pick up the new default audio device.
        if (SDL_GetAudioDeviceStatus(m_AudioDevice) == SDL_AUDIO_STOPPED) {
            return false;
        }

        // Only queue more samples where there is 50 ms or less in SDL's queue
        if (SDL_GetQueuedAudioSize(m_AudioDevice) / m_FrameSize * m_FrameDurationMs <= 50) {
            break;
        }

        SDL_Delay(1);
    }

    if (SDL_QueueAudio(m_AudioDevice, m_AudioBuffer, bytesWritten) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to queue audio sample: %s",
                     SDL_GetError());
    }

    return true;
}

void SdlAudioRenderer::trackBurst(int frames, Uint64 gapUs, Uint64 now)
{
    // Packets that arrive within 2 ms of each other came from the same host capture period
    if (gapUs < kSameBurstGapUs) {
        m_CurrentBurstFrames += frames;
    }
    else {
        if (m_CurrentBurstFrames > 0 && m_BurstSampleCount < (int)SDL_arraysize(m_BurstSamples)) {
            m_BurstSamples[m_BurstSampleCount++] = m_CurrentBurstFrames;
        }
        m_CurrentBurstFrames = frames;
    }

    // Publish the median burst size once a second
    if (now - m_BurstWindowStart >= SDL_GetPerformanceFrequency()) {
        if (m_BurstSampleCount >= 10) {
            std::nth_element(m_BurstSamples,
                             m_BurstSamples + m_BurstSampleCount / 2,
                             m_BurstSamples + m_BurstSampleCount);
            m_BurstFrames = m_BurstSamples[m_BurstSampleCount / 2];
        }
        m_BurstSampleCount = 0;
        m_BurstWindowStart = now;
    }
}

void SdlAudioRenderer::queueSilence(int frames)
{
    while (frames > 0) {
        const int chunk = SDL_min(frames, m_SilenceFrames);
        if (SDL_QueueAudio(m_AudioDevice, m_SilenceBuffer, (Uint32)(chunk * m_BytesPerFrame)) < 0) {
            break;
        }
        frames -= chunk;
    }
}

int SDLCALL SdlAudioRenderer::driftMonitorThreadProc(void* context)
{
    static_cast<SdlAudioRenderer*>(context)->driftMonitorLoop();
    return 0;
}

void SdlAudioRenderer::driftMonitorLoop()
{
    const Uint64 freq = SDL_GetPerformanceFrequency();
    const double framesPerMs = m_SampleRate / 1000.0;
    const int marginStep = kMarginStepMs * m_SampleRate / 1000;
    const int marginMax = kMaxMarginMs * m_SampleRate / 1000;
    const int marginDecay = m_SampleRate / 2000;

    // Per-second medians of the device pull size and the host burst size over the last
    // 10 minutes. The estimate is their maximum: a packet arriving in the same millisecond as
    // a pull hides part of that pull, which can only make it look smaller, and with two
    // nearly equal clocks that alignment can persist for minutes.
    std::vector<int> pullHistory(kHistorySeconds, 0);
    std::vector<int> burstHistory(kHistorySeconds, 0);
    int historyPos = 0;
    int pullFrames = m_SampleRate / 100;
    int burstFrames = m_SampleRate / 100;

    std::vector<int> pulls;
    pulls.reserve(1024);
    double levelSum = 0.0;
    Uint32 levelCount = 0;
    int levelMin = INT_MAX;
    int levelMax = 0;
    int prevLevel = -1;

    int marginFrames = m_InitialMarginFrames;
    Uint32 seenUnderruns = 0;

    Uint64 windowStart = SDL_GetPerformanceCounter();
    Uint64 lastUnderrun = windowStart;
    Uint64 lastMarginChange = windowStart;
    Uint64 lastStats = windowStart;
    double statsLevelSum = 0.0;
    Uint32 statsWindows = 0;
    int statsMin = INT_MAX;
    int statsMax = 0;
    Uint32 statsRemoved = 0;
    Uint32 statsInserted = 0;
    Uint32 statsDropped = 0;
    Uint32 statsUnderruns = 0;

    auto resetWindow = [&](Uint64 now) {
        levelSum = 0.0;
        levelCount = 0;
        levelMin = INT_MAX;
        levelMax = 0;
        prevLevel = -1;
        pulls.clear();
        windowStart = now;
    };

    while (!m_StopDriftMonitor) {
        SDL_Delay(1);
        const Uint64 now = SDL_GetPerformanceCounter();

        if (m_RestartWindow.exchange(false)) {
            m_CorrectionPpm = 0.0f;
            resetWindow(now);
            continue;
        }

        // The host stops sending during silence. Don't let the empty queue that leaves
        // behind count toward the average.
        const Uint64 lastPacket = m_LastPacketTime;
        if (lastPacket == 0 || now - lastPacket > freq / 20) {
            resetWindow(now);
            continue;
        }

        const int level = (int)(SDL_GetQueuedAudioSize(m_AudioDevice) / (Uint32)m_BytesPerFrame);
        levelSum += level;
        levelCount++;
        levelMin = SDL_min(levelMin, level);
        levelMax = SDL_max(levelMax, level);
        if (prevLevel >= 0 && prevLevel - level >= 64) {
            // SDL pulled a device period (part of it may be hidden by a packet that
            // arrived in the same millisecond)
            pulls.push_back(prevLevel - level);
        }
        prevLevel = level;

        if (now - windowStart < freq) {
            continue;
        }

        // Once a second: update the size estimates, the margin, the target and the correction
        int pullMedian = 0;
        if (pulls.size() >= 20) {
            std::nth_element(pulls.begin(), pulls.begin() + pulls.size() / 2, pulls.end());
            pullMedian = pulls[pulls.size() / 2];
        }
        pullHistory[historyPos] = pullMedian;
        burstHistory[historyPos] = m_BurstFrames;
        historyPos = (historyPos + 1) % kHistorySeconds;
        const int maxPull = *std::max_element(pullHistory.begin(), pullHistory.end());
        const int maxBurst = *std::max_element(burstHistory.begin(), burstHistory.end());
        if (maxPull > 0) {
            pullFrames = maxPull;
        }
        if (maxBurst > 0) {
            burstFrames = maxBurst;
        }

        const Uint32 underruns = m_Underruns;
        if (underruns != seenUnderruns) {
            seenUnderruns = underruns;
            lastUnderrun = now;
            lastMarginChange = now;
            if (marginFrames < marginMax) {
                marginFrames = SDL_min(marginFrames + marginStep, marginMax);
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Audio queue ran dry while streaming; raising the drift control margin to %.1f ms",
                            marginFrames / framesPerMs);
            }
        }
        else if (marginFrames > m_InitialMarginFrames &&
                 now - lastUnderrun >= kMarginHoldSeconds * freq &&
                 now - lastMarginChange >= kMarginDecaySeconds * freq) {
            lastMarginChange = now;
            marginFrames = SDL_max(marginFrames - marginDecay, m_InitialMarginFrames);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "No audio underruns for %d min; lowering the drift control margin to %.1f ms",
                        (int)((now - lastUnderrun) / freq / 60), marginFrames / framesPerMs);
        }

        const int target = (pullFrames + burstFrames) / 2 + marginFrames;
        m_TargetFrames = target;

        const double average = levelSum / levelCount;
        const double correction = SDL_clamp(kGainPpmPerMs * (average - target) / framesPerMs,
                                            -kMaxCorrectionPpm, kMaxCorrectionPpm);
        m_CorrectionPpm = (float)correction;

        statsLevelSum += average;
        statsWindows++;
        statsMin = SDL_min(statsMin, levelMin);
        statsMax = SDL_max(statsMax, levelMax);
        if (now - lastStats >= kStatsSeconds * freq) {
            const Uint32 removed = m_FramesRemoved;
            const Uint32 inserted = m_FramesInserted;
            const Uint32 dropped = m_PacketsDropped;
            const Uint32 dry = m_Underruns;
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Audio drift control: queue %.1f ms avg (target %.1f, range %.1f-%.1f), "
                        "device pulls %d frames, host bursts %d frames, margin %.1f ms, correction %+.0f ppm; "
                        "since last report: %u frames removed, %u inserted, %u packets dropped, %u underruns",
                        statsLevelSum / statsWindows / framesPerMs,
                        target / framesPerMs,
                        statsMin / framesPerMs,
                        statsMax / framesPerMs,
                        pullFrames,
                        burstFrames,
                        marginFrames / framesPerMs,
                        correction,
                        removed - statsRemoved,
                        inserted - statsInserted,
                        dropped - statsDropped,
                        dry - statsUnderruns);
            statsRemoved = removed;
            statsInserted = inserted;
            statsDropped = dropped;
            statsUnderruns = dry;
            statsLevelSum = 0.0;
            statsWindows = 0;
            statsMin = INT_MAX;
            statsMax = 0;
            lastStats = now;
        }

        resetWindow(now);
    }
}

IAudioRenderer::AudioFormat SdlAudioRenderer::getAudioBufferFormat()
{
    return AudioFormat::Float32NE;
}
