#pragma once

#include <Limelight.h>
#include <QtGlobal>
#include <SDL.h>

typedef struct _AUDIO_STATS {
    uint32_t opusBytesReceived;
    uint32_t decodedPackets;                    // total packets decoded (if less than renderedPackets it indicates droppedOverload)
    uint32_t renderedPackets;                   // total audio packets rendered (only for certain backends)

    uint32_t droppedNetwork;                    // total packets lost to the network
    uint32_t droppedOverload;                   // total times we dropped a packet due to being unable to run in time
    uint32_t totalGlitches;                     // total times the audio was interrupted

    uint64_t decodeDurationUs;                  // cumulative render time, microseconds
    uint64_t decodeDurationUsMax;               // slowest render time, microseconds
    uint32_t lastRtt;                           // network latency from enet, milliseconds
    uint64_t measurementStartUs;                // timestamp stats were started, microseconds
    double opusKbitsPerSec;                     // current Opus bitrate in kbps, not including FEC overhead
} AUDIO_STATS, *PAUDIO_STATS;

class IAudioRenderer
{
public:
    IAudioRenderer();

    virtual ~IAudioRenderer() {}

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig) = 0;

    virtual void setOpusConfig(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig) {
        m_opusConfig = opusConfig;
    }

    virtual void* getAudioBuffer(int* size) = 0;

    // Return false if an unrecoverable error has occurred and the renderer must be reinitialized
    virtual bool submitAudio(int bytesWritten) = 0;

    virtual int getCapabilities() = 0;

    virtual void remapChannels(POPUS_MULTISTREAM_CONFIGURATION) {
        // Use default channel mapping:
        // 0 - Front Left
        // 1 - Front Right
        // 2 - Center
        // 3 - LFE
        // 4 - Surround Left
        // 5 - Surround Right
    }

    enum class AudioFormat {
        Sint16NE,  // 16-bit signed integer (native endian)
        Float32NE, // 32-bit floating point (native endian)
    };
    virtual AudioFormat getAudioBufferFormat() = 0;

    virtual int getAudioBufferSampleSize();

    AUDIO_STATS & getActiveWndAudioStats() {
        return m_ActiveWndAudioStats;
    }

    virtual const char * getRendererName() { return "IAudioRenderer"; };

    // generic stats handling for all child classes
    virtual void addAudioStats(AUDIO_STATS &, AUDIO_STATS &);
    virtual void flipAudioStatsWindows();
    virtual void logGlobalAudioStats();
    virtual void snapshotAudioStats(AUDIO_STATS &);
    virtual void statsAddOpusBytesReceived(int);
    virtual void statsTrackDecodeTime(uint64_t);
    virtual int stringifyAudioStats(AUDIO_STATS &, char *, int);

protected:
    AUDIO_STATS m_ActiveWndAudioStats;
    AUDIO_STATS m_LastWndAudioStats;
    AUDIO_STATS m_GlobalAudioStats;

    // input stream metadata
    const OPUS_MULTISTREAM_CONFIGURATION* m_opusConfig;
};
