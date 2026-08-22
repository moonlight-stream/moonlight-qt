#include "streaming/input/stylusreplay.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryFile>

#include <cstdint>
#include <limits>
#include <vector>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

namespace {

bool writeFile(QTemporaryFile& file, const QByteArray& contents)
{
    if (!file.open()) {
        return false;
    }
    if (file.write(contents) != contents.size()) {
        return false;
    }
    return file.flush();
}

}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryFile validFile;
    const QByteArray validData =
            "\xEF\xBB\xBF" "SUNSHINE_STYLUS_DAT\t1\r\n"
            "# columns=P timestamp_us event_type x y pressure rotation tilt\r\n"
            "P 1000 0 0.10 0.20 0.0 65535 255\r\n"
            "P 5000 1 0.20 0.30 0.4 10 20\r\n"
            "P 9000 3 0.30 0.40 0.5 11 21\r\n"
            "P 13000 2 0.40 0.50 0.0 12 22\r\n"
            "P 13000 6 0.40 0.50 0.0 65535 255\r\n"
            "# truncated=true\r\n";
    CHECK(writeFile(validFile, validData));

    StylusReplayController replay;
    QString error;
    CHECK(replay.loadFile(validFile.fileName(), error));
    CHECK(error.isEmpty());
    CHECK(replay.sampleCount() == 5);
    CHECK(replay.durationUs() == 12000);
    CHECK(replay.isTruncated());

    constexpr std::uint64_t startUs = 1000000;
    CHECK(replay.start(startUs, 2, error));
    CHECK(replay.nextDelayMs(startUs) == 0);

    std::vector<std::uint8_t> events;
    auto collect = [&events](const StylusReplayController::Sample& sample) {
        events.push_back(sample.eventType);
        return 0;
    };

    std::size_t sent = 0;
    CHECK(replay.processDue(startUs, collect, &sent) ==
          StylusReplayController::ProcessResult::Running);
    CHECK(sent == 1);
    CHECK(replay.processedSampleCount() == 1);
    CHECK(replay.remainingUs(startUs) == 6000);
    CHECK(replay.nextDelayMs(startUs) == 2);

    CHECK(replay.processDue(startUs + 1999, collect, &sent) ==
          StylusReplayController::ProcessResult::Running);
    CHECK(sent == 0);
    CHECK(replay.processDue(startUs + 2000, collect, &sent) ==
          StylusReplayController::ProcessResult::Running);
    CHECK(sent == 1);
    CHECK(replay.remainingUs(startUs + 2000) == 4000);

    CHECK(replay.processDue(startUs + 6000, collect, &sent) ==
          StylusReplayController::ProcessResult::Finished);
    CHECK(sent == 3);
    CHECK(events == std::vector<std::uint8_t>({0, 1, 3, 2, 6}));
    CHECK(!replay.isPlaying());

    CHECK(replay.start(startUs, 4, error));
    CHECK(replay.processDue(startUs, [](const StylusReplayController::Sample&) {
        return -1;
    }) == StylusReplayController::ProcessResult::SendFailed);
    CHECK(!replay.isPlaying());

    QTemporaryFile reversedFile;
    CHECK(writeFile(reversedFile,
            "SUNSHINE_STYLUS_DAT\t1\n"
            "P 20 1 0.1 0.1 0.2 0 0\n"
            "P 10 3 0.2 0.2 0.3 0 0\n"));
    CHECK(!replay.loadFile(reversedFile.fileName(), error));
    CHECK(replay.sampleCount() == 5); // failed imports retain the last valid recording

    QTemporaryFile burstFile;
    QByteArray burstData("SUNSHINE_STYLUS_DAT\t1\n");
    for (int i = 0; i < 300; i++) {
        burstData += "P 0 3 0.5 0.5 0.5 0 0\n";
    }
    CHECK(writeFile(burstFile, burstData));
    CHECK(replay.loadFile(burstFile.fileName(), error));
    CHECK(replay.start(startUs, 4, error));
    std::size_t burstSent = 0;
    CHECK(replay.processDue(startUs, [](const StylusReplayController::Sample&) {
        return 0;
    }, &burstSent) == StylusReplayController::ProcessResult::Running);
    CHECK(burstSent == StylusReplayController::MaxSamplesPerProcess);
    CHECK(replay.processDue(startUs, [](const StylusReplayController::Sample&) {
        return 0;
    }, &burstSent) == StylusReplayController::ProcessResult::Finished);
    CHECK(burstSent == 300 - StylusReplayController::MaxSamplesPerProcess);

    QTemporaryFile largeTimestampFile;
    CHECK(writeFile(largeTimestampFile,
            "SUNSHINE_STYLUS_DAT\t1\n"
            "P 0 0 0.1 0.1 0.0 65535 255\n"
            "P 18446744073709551615 6 0.1 0.1 0.0 65535 255\n"));
    CHECK(replay.loadFile(largeTimestampFile.fileName(), error));
    CHECK(replay.start(startUs, 4, error));
    CHECK(replay.processDue(startUs, [](const StylusReplayController::Sample&) {
        return 0;
    }) == StylusReplayController::ProcessResult::Running);
    CHECK(replay.nextDelayMs(startUs) == std::numeric_limits<int>::max());

    if (app.arguments().size() > 1) {
        CHECK(replay.loadFile(app.arguments().at(1), error));
        CHECK(replay.sampleCount() > 0);
    }

    return 0;
}
