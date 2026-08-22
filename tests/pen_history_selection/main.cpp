#include "streaming/input/penhistory.h"

#include <array>
#include <cstdint>
#include <limits>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

int main()
{
    using PenHistory::selectRecentSampleCount;
    using PenHistory::selectReplaySamples;

    CHECK(selectRecentSampleCount(nullptr, 0, 100) == 0);

    const std::array<std::uint32_t, 4> recent = { 100, 96, 90, 84 };
    CHECK(selectRecentSampleCount(recent.data(), recent.size(), 100) == recent.size());

    const std::array<std::uint32_t, 4> stale = { 100, 96, 83, 80 };
    CHECK(selectRecentSampleCount(stale.data(), stale.size(), 100) == 2);

    const std::array<std::uint32_t, 3> exactBoundary = { 100, 84, 83 };
    CHECK(selectRecentSampleCount(exactBoundary.data(), exactBoundary.size(), 100) == 2);

    // Even a compact history batch is stale if the window thread did not
    // process the pointer message promptly. Always retain only its newest point.
    const std::array<std::uint32_t, 3> delayedBatch = { 100, 98, 95 };
    CHECK(selectRecentSampleCount(delayedBatch.data(), delayedBatch.size(), 130) == 1);

    const std::array<std::uint32_t, 3> unavailable = { 0, 0, 0 };
    CHECK(selectRecentSampleCount(unavailable.data(), unavailable.size(), 100) == unavailable.size());

    const std::array<std::uint32_t, 3> partialTimestamps = { 100, 0, 90 };
    CHECK(selectRecentSampleCount(partialTimestamps.data(), partialTimestamps.size(), 100) == 1);

    // DWORD subtraction is intentionally unsigned so a GetTickCount-style
    // wrap still produces the correct small age.
    const std::array<std::uint32_t, 2> wrapped = {
        2,
        std::numeric_limits<std::uint32_t>::max() - 2,
    };
    CHECK(selectRecentSampleCount(wrapped.data(), wrapped.size(), 3) == wrapped.size());

    // History is newest-first. The barrel press and release are both stale,
    // but they remain in the replay set while ordinary stale motion is removed.
    const std::array<std::uint32_t, 6> stateTimes = { 100, 95, 80, 75, 70, 65 };
    const std::array<std::uint32_t, 6> barrelStates = { 0, 0, 0, 1, 1, 0 };
    const auto barrelSelection = selectReplaySamples(
            stateTimes.data(), barrelStates.data(), stateTimes.size(), 100, true, 0);
    CHECK(barrelSelection.count == 4);
    CHECK(barrelSelection.oldestFirstIndices[0] == 4); // press
    CHECK(barrelSelection.oldestFirstIndices[1] == 2); // release
    CHECK(barrelSelection.oldestFirstIndices[2] == 1); // recent motion
    CHECK(barrelSelection.oldestFirstIndices[3] == 0); // latest position

    const std::array<std::uint32_t, 6> stableStates = { 0, 0, 0, 0, 0, 0 };
    const auto motionSelection = selectReplaySamples(
            stateTimes.data(), stableStates.data(), stateTimes.size(), 100, true, 0);
    CHECK(motionSelection.count == 2);
    CHECK(motionSelection.oldestFirstIndices[0] == 1);
    CHECK(motionSelection.oldestFirstIndices[1] == 0);

    std::array<std::uint32_t, 24> deepTimes = {};
    std::array<std::uint32_t, 24> deepStates = {};
    for (std::size_t i = 0; i < deepTimes.size(); i++) {
        deepTimes[i] = 100 - static_cast<std::uint32_t>(i);
    }
    for (std::size_t i = 19; i <= 22; i++) {
        deepStates[i] = 1;
    }
    const auto deepSelection = selectReplaySamples(
            deepTimes.data(), deepStates.data(), deepTimes.size(), 100, true, 0);
    CHECK(deepSelection.oldestFirstIndices[0] == 22); // press outside the 16-sample cap
    CHECK(deepSelection.oldestFirstIndices[1] == 18); // release outside the 16-sample cap
    CHECK(deepSelection.oldestFirstIndices[deepSelection.count - 1] == 0);

    CHECK(PenHistory::MAX_HISTORY_SAMPLES == 128);
    CHECK(PenHistory::MAX_REPLAY_SAMPLES == 16);
    CHECK(PenHistory::MAX_REPLAY_AGE_MS == 16);
    return 0;
}
