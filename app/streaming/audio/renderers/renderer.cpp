#include "renderer.h"

#include <Limelight.h>

IAudioRenderer::IAudioRenderer()
{
    SDL_zero(m_ActiveWndAudioStats);
    SDL_zero(m_LastWndAudioStats);
    SDL_zero(m_GlobalAudioStats);

    m_ActiveWndAudioStats.measurementStartUs = LiGetMicroseconds();
}

int IAudioRenderer::getAudioBufferSampleSize()
{
    switch (getAudioBufferFormat()) {
    case IAudioRenderer::AudioFormat::Sint16NE:
        return sizeof(short);
    case IAudioRenderer::AudioFormat::Float32NE:
        return sizeof(float);
    default:
        Q_UNREACHABLE();
    }
}

void IAudioRenderer::addAudioStats(AUDIO_STATS& src, AUDIO_STATS& dst)
{
    dst.opusBytesReceived     += src.opusBytesReceived;
    dst.decodedPackets        += src.decodedPackets;
    dst.renderedPackets       += src.renderedPackets;
    dst.droppedNetwork        += src.droppedNetwork;
    dst.droppedOverload       += src.droppedOverload;
    dst.decodeDurationUs      += src.decodeDurationUs;

    if (!LiGetEstimatedRttInfo(&dst.lastRtt, NULL)) {
        dst.lastRtt = 0;
    }
    else {
        // Our logic to determine if RTT is valid depends on us never
        // getting an RTT of 0. ENet currently ensures RTTs are >= 1.
        SDL_assert(dst.lastRtt > 0);
    }

    // Initialize the measurement start point if this is the first video stat window
    if (!dst.measurementStartUs) {
        dst.measurementStartUs = src.measurementStartUs;
    }

    // The following code assumes the global measure was already started first
    SDL_assert(dst.measurementStartUs <= src.measurementStartUs);

    double timeDiffSecs = (double)(LiGetMicroseconds() - dst.measurementStartUs) / 1000000.0;
    dst.opusKbitsPerSec = (double)(dst.opusBytesReceived * 8) / 1000.0 / timeDiffSecs;
}

void IAudioRenderer::flipAudioStatsWindows()
{
    // Called once a second, adds stats to the running global total,
    // copies the active window to the last window, and initializes
    // a fresh active window.

    // Accumulate these values into the global stats
    addAudioStats(m_ActiveWndAudioStats, m_GlobalAudioStats);

    // Move this window into the last window slot and clear it for next window
    SDL_memcpy(&m_LastWndAudioStats, &m_ActiveWndAudioStats, sizeof(m_ActiveWndAudioStats));
    SDL_zero(m_ActiveWndAudioStats);
    m_ActiveWndAudioStats.measurementStartUs = LiGetMicroseconds();
}

void IAudioRenderer::logGlobalAudioStats()
{
    if (m_GlobalAudioStats.decodedPackets > 0) {
        char audioStatsStr[1024];
        stringifyAudioStats(m_GlobalAudioStats, audioStatsStr, sizeof(audioStatsStr));

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "\nCurrent session audio stats\n---------------------------\n%s",
                    audioStatsStr);
    }
}

void IAudioRenderer::snapshotAudioStats(AUDIO_STATS &snapshot)
{
    addAudioStats(m_LastWndAudioStats, snapshot);
    addAudioStats(m_ActiveWndAudioStats, snapshot);
}

void IAudioRenderer::statsAddOpusBytesReceived(int size)
{
    m_ActiveWndAudioStats.opusBytesReceived += size;

    if (size) {
        m_ActiveWndAudioStats.decodedPackets++;
    }
    else {
        // if called with size=0 it indicates a packet that is presumed lost by the network
        m_ActiveWndAudioStats.droppedNetwork++;
    }
}

void IAudioRenderer::statsTrackDecodeTime(uint64_t startTimeUs)
{
    uint64_t decodeTimeUs = LiGetMicroseconds() - startTimeUs;
    m_ActiveWndAudioStats.decodeDurationUs += decodeTimeUs;
}

// Provide audio stats common to all renderer backends. Child classes can then add additional lines
// at the returned offset length into output.
int IAudioRenderer::stringifyAudioStats(AUDIO_STATS& stats, char *output, int length)
{
    int offset = 0;

    // Start with an empty string
    output[offset] = 0;

    double opusFrameSize = (double)m_opusConfig->samplesPerFrame / 48.0;
    const RTP_AUDIO_STATS* rtpAudioStats = LiGetRTPAudioStats();
    double fecOverhead = (double)rtpAudioStats->packetCountFec * 1.0 / (rtpAudioStats->packetCountAudio + rtpAudioStats->packetCountFec);

    int ret = snprintf(
        &output[offset],
        length - offset,
        "Audio stream: %s-channel Opus low-delay @ 48 kHz (%s)\n"
        "Bitrate: %.1f kbps, +%.0f%% forward error-correction\n"
        "Opus config: %s, frame size: %.1f ms\n"
        "Packet loss from network: %.2f%%, loss from CPU overload: %.2f%%\n"
        "Average decoding time: %0.2f ms\n",

        // "Audio stream: %s-channel Opus low-delay @ 48 kHz (%s)\n"
        m_opusConfig->channelCount == 6 ? "5.1" : m_opusConfig->channelCount == 8 ? "7.1" : "2",
        getRendererName(),

        // "Bitrate: %.1f %s, +%.0f%% forward error-correction\n"
        stats.opusKbitsPerSec,
        fecOverhead * 100.0,

        // "Opus config: %s, frame size: %.1fms\n"
        // Work out if we're getting high or low quality from Sunshine. coupled surround is designed for physical speakers
        ((m_opusConfig->channelCount == 2 && stats.opusKbitsPerSec > 128) || !m_opusConfig->coupledStreams)
            ? "high quality (LAN)" // 512k stereo coupled, 1.5mbps 5.1 uncoupled, 2mbps 7.1 uncoupled
            : "normal quality",    // 96k stereo coupled, 256k 5.1 coupled, 450k 7.1 coupled
        opusFrameSize,

        // "Packet loss from network: %.2f%%, loss from CPU overload: %.2f%%\n"
        stats.decodedPackets ? ((double)stats.droppedNetwork / stats.decodedPackets) * 100.0 : 0.0,
        stats.decodedPackets ? ((double)stats.droppedOverload / stats.decodedPackets) * 100.0 : 0.0,

        // "Average decoding time: %0.2f ms\n"
        (double)(stats.decodeDurationUs / 1000.0) / stats.decodedPackets
    );
    if (ret < 0 || ret >= length - offset) {
        SDL_assert(false);
        return -1;
    }

    return offset + ret;
}
