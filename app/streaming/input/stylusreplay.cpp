#include "stylusreplay.h"

#ifdef MOONLIGHT_ENABLE_FUNCTION_TESTS

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr char DataMagic[] = "SUNSHINE_STYLUS_DAT\t1";
constexpr std::uint16_t RotationUnknown = 0xFFFF;
constexpr std::uint8_t TiltUnknown = 0xFF;

QString replayText(const char* text)
{
    return QCoreApplication::translate("StylusReplayController", text);
}

void trimLineEnding(QByteArray& line)
{
    while (line.endsWith('\n') || line.endsWith('\r')) {
        line.chop(1);
    }
}

}

bool StylusReplayController::loadFile(const QString& path, QString& error)
{
    error.clear();

    QFile file(path);
    const QFileInfo fileInfo(file);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        error = replayText("The recording file does not exist.");
        return false;
    }
    if (fileInfo.size() > MaxFileBytes) {
        error = replayText("The recording file exceeds the 64 MiB limit.");
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        error = replayText("The recording file could not be opened.");
        return false;
    }

    QByteArray firstLine = file.readLine();
    trimLineEnding(firstLine);
    if (firstLine.startsWith("\xEF\xBB\xBF")) {
        firstLine.remove(0, 3);
    }
    if (firstLine != DataMagic) {
        error = replayText("This is not a supported stylus recording.");
        return false;
    }

    std::vector<Sample> samples;
    samples.reserve(std::min<std::size_t>(
            static_cast<std::size_t>(std::max<qint64>(0, fileInfo.size() / 64)),
            MaxSamples));
    bool truncated = false;
    std::uint64_t previousTimestamp = 0;
    bool havePreviousTimestamp = false;
    std::size_t lineNumber = 1;

    while (!file.atEnd()) {
        QByteArray rawLine = file.readLine();
        lineNumber++;
        trimLineEnding(rawLine);

        const QString line = QString::fromUtf8(rawLine).trimmed();
        if (line.startsWith(QStringLiteral("# truncated=true"))) {
            truncated = true;
            continue;
        }
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QStringList fields = line.simplified().split(QLatin1Char(' '));
        if (fields.size() != 8 || fields[0] != QStringLiteral("P")) {
            error = replayText("Line %1 has an invalid format.").arg(lineNumber);
            return false;
        }

        bool timestampOk = false;
        bool eventTypeOk = false;
        bool xOk = false;
        bool yOk = false;
        bool pressureOk = false;
        bool rotationOk = false;
        bool tiltOk = false;
        const std::uint64_t timestamp = fields[1].toULongLong(&timestampOk);
        const unsigned int eventType = fields[2].toUInt(&eventTypeOk);
        const double x = fields[3].toDouble(&xOk);
        const double y = fields[4].toDouble(&yOk);
        const double pressure = fields[5].toDouble(&pressureOk);
        const unsigned int rotation = fields[6].toUInt(&rotationOk);
        const int tilt = fields[7].toInt(&tiltOk);

        if (!timestampOk || !eventTypeOk || !xOk || !yOk || !pressureOk ||
                !rotationOk || !tiltOk || eventType > 7 ||
                !std::isfinite(x) || x < 0.0 || x > 1.0 ||
                !std::isfinite(y) || y < 0.0 || y > 1.0 ||
                !std::isfinite(pressure) || pressure < 0.0 || pressure > 1.0 ||
                (rotation > 359 && rotation != RotationUnknown) ||
                (tilt < 0 || (tilt > 90 && tilt != TiltUnknown))) {
            error = replayText("Line %1 contains a value outside the supported range.")
                    .arg(lineNumber);
            return false;
        }
        if (havePreviousTimestamp && timestamp < previousTimestamp) {
            error = replayText("Line %1 has a timestamp earlier than the previous sample.")
                    .arg(lineNumber);
            return false;
        }
        if (samples.size() >= MaxSamples) {
            error = replayText("The recording exceeds the 200000 sample limit.");
            return false;
        }

        samples.push_back({
            timestamp,
            static_cast<std::uint8_t>(eventType),
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(pressure),
            static_cast<std::uint16_t>(rotation),
            static_cast<std::uint8_t>(tilt),
        });
        previousTimestamp = timestamp;
        havePreviousTimestamp = true;
    }

    if (file.error() != QFileDevice::NoError) {
        error = replayText("An error occurred while reading the recording file.");
        return false;
    }
    if (samples.empty()) {
        error = replayText("The recording file does not contain stylus samples.");
        return false;
    }

    stop();
    m_Samples = std::move(samples);
    m_SourceFileName = fileInfo.fileName();
    m_Truncated = truncated;
    return true;
}

bool StylusReplayController::start(std::uint64_t nowUs, int speed, QString& error)
{
    error.clear();
    if (!isLoaded()) {
        error = replayText("Import a stylus recording before starting replay.");
        return false;
    }
    if (speed != 1 && speed != 2 && speed != 4) {
        error = replayText("The selected replay speed is not supported.");
        return false;
    }

    m_StartTimeUs = nowUs;
    m_NextSample = 0;
    m_Speed = speed;
    m_Playing = true;
    return true;
}

void StylusReplayController::stop()
{
    m_Playing = false;
    m_NextSample = 0;
    m_StartTimeUs = 0;
}

StylusReplayController::ProcessResult StylusReplayController::processDue(
        std::uint64_t nowUs, const SendCallback& send, std::size_t* sentSamples)
{
    if (sentSamples) {
        *sentSamples = 0;
    }
    if (!m_Playing) {
        return ProcessResult::Idle;
    }

    const std::uint64_t elapsedUs = nowUs >= m_StartTimeUs ? nowUs - m_StartTimeUs : 0;
    std::size_t sent = 0;
    while (m_NextSample < m_Samples.size() && sent < MaxSamplesPerProcess &&
           dueOffsetUs(m_NextSample) <= elapsedUs) {
        if (!send || send(m_Samples[m_NextSample]) != 0) {
            stop();
            if (sentSamples) {
                *sentSamples = sent;
            }
            return ProcessResult::SendFailed;
        }
        m_NextSample++;
        sent++;
    }

    if (sentSamples) {
        *sentSamples = sent;
    }
    if (m_NextSample == m_Samples.size()) {
        stop();
        return ProcessResult::Finished;
    }
    return ProcessResult::Running;
}

int StylusReplayController::nextDelayMs(std::uint64_t nowUs) const
{
    if (!m_Playing || m_NextSample >= m_Samples.size()) {
        return -1;
    }

    const std::uint64_t elapsedUs = nowUs >= m_StartTimeUs ? nowUs - m_StartTimeUs : 0;
    const std::uint64_t dueUs = dueOffsetUs(m_NextSample);
    if (dueUs <= elapsedUs) {
        return 0;
    }

    const std::uint64_t remainingMs = (dueUs - elapsedUs + 999) / 1000;
    return static_cast<int>(std::min<std::uint64_t>(
            remainingMs, static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
}

std::uint64_t StylusReplayController::durationUs() const
{
    if (m_Samples.size() < 2) {
        return 0;
    }
    return m_Samples.back().timestampUs - m_Samples.front().timestampUs;
}

std::uint64_t StylusReplayController::remainingUs(std::uint64_t nowUs) const
{
    if (!m_Playing || m_Samples.empty()) {
        return 0;
    }

    const std::uint64_t elapsedUs = nowUs >= m_StartTimeUs ? nowUs - m_StartTimeUs : 0;
    const std::uint64_t totalReplayUs = dueOffsetUs(m_Samples.size() - 1);
    return elapsedUs < totalReplayUs ? totalReplayUs - elapsedUs : 0;
}

std::uint64_t StylusReplayController::dueOffsetUs(std::size_t index) const
{
    const std::uint64_t sourceOffset =
            m_Samples[index].timestampUs - m_Samples.front().timestampUs;
    const std::uint64_t speed = static_cast<std::uint64_t>(m_Speed);
    return sourceOffset / speed + (sourceOffset % speed != 0 ? 1 : 0);
}

#endif
