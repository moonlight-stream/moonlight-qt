#pragma once

#ifdef MOONLIGHT_ENABLE_FUNCTION_TESTS

#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

class StylusReplayController
{
public:
    struct Sample {
        std::uint64_t timestampUs;
        std::uint8_t eventType;
        float x;
        float y;
        float pressure;
        std::uint16_t rotation;
        std::uint8_t tilt;
    };

    enum class ProcessResult {
        Idle,
        Running,
        Finished,
        SendFailed,
    };

    using SendCallback = std::function<int(const Sample&)>;

    static constexpr std::size_t MaxSamples = 200000;
    static constexpr std::size_t MaxSamplesPerProcess = 256;
    static constexpr qint64 MaxFileBytes = 64LL * 1024LL * 1024LL;

    bool loadFile(const QString& path, QString& error);

    bool start(std::uint64_t nowUs, int speed, QString& error);
    void stop();

    ProcessResult processDue(std::uint64_t nowUs, const SendCallback& send,
                             std::size_t* sentSamples = nullptr);
    int nextDelayMs(std::uint64_t nowUs) const;

    bool isLoaded() const { return !m_Samples.empty(); }
    bool isPlaying() const { return m_Playing; }
    bool isTruncated() const { return m_Truncated; }
    int speed() const { return m_Speed; }
    std::size_t sampleCount() const { return m_Samples.size(); }
    std::size_t processedSampleCount() const { return m_NextSample; }
    std::uint64_t durationUs() const;
    std::uint64_t remainingUs(std::uint64_t nowUs) const;
    QString sourceFileName() const { return m_SourceFileName; }

private:
    std::uint64_t dueOffsetUs(std::size_t index) const;

    std::vector<Sample> m_Samples;
    QString m_SourceFileName;
    bool m_Truncated = false;
    bool m_Playing = false;
    int m_Speed = 1;
    std::size_t m_NextSample = 0;
    std::uint64_t m_StartTimeUs = 0;
};

#endif
