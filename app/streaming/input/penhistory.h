#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace PenHistory {

// Keep the replay window small enough that common-c's 1 ms pen pacing cannot
// put the live pen tip behind a long burst of coalesced Windows samples.
constexpr std::size_t MAX_HISTORY_SAMPLES = 128;
constexpr std::size_t MAX_REPLAY_SAMPLES = 16;
constexpr std::uint32_t MAX_REPLAY_AGE_MS = 16;

struct ReplaySelection {
    std::array<std::size_t, MAX_HISTORY_SAMPLES> oldestFirstIndices{};
    std::size_t count = 0;
};

// Windows returns pointer history newest-first. Return the number of leading
// samples that fit within the latency budget. The caller sends that prefix in
// reverse order to preserve stroke order. A zero newest timestamp means the
// device did not provide usable timing, so retain the already bounded buffer.
inline std::size_t selectRecentSampleCount(const std::uint32_t* newestFirstTimestamps,
                                           std::size_t count,
                                           std::uint32_t nowTimestamp,
                                           std::uint32_t maxAgeMs = MAX_REPLAY_AGE_MS)
{
    if (newestFirstTimestamps == nullptr || count == 0) {
        return 0;
    }

    const std::uint32_t newestTimestamp = newestFirstTimestamps[0];
    if (newestTimestamp == 0 || nowTimestamp == 0) {
        return count;
    }

    std::size_t selected = 1;
    while (selected < count) {
        const std::uint32_t timestamp = newestFirstTimestamps[selected];
        if (timestamp == 0 ||
                newestTimestamp - timestamp > maxAgeMs ||
                nowTimestamp - timestamp > maxAgeMs) {
            break;
        }
        selected++;
    }

    return selected;
}

// Select recent motion samples while retaining all state transitions. The
// state key represents fields that must not be coalesced away, such as pen
// buttons, tool type, contact, and cancellation state.
inline ReplaySelection selectReplaySamples(const std::uint32_t* newestFirstTimestamps,
                                           const std::uint32_t* newestFirstStateKeys,
                                           std::size_t count,
                                           std::uint32_t nowTimestamp,
                                           bool previousStateValid,
                                           std::uint32_t previousStateKey)
{
    ReplaySelection selection;
    if (newestFirstTimestamps == nullptr || newestFirstStateKeys == nullptr || count == 0) {
        return selection;
    }

    count = std::min(count, MAX_HISTORY_SAMPLES);
    std::array<bool, MAX_HISTORY_SAMPLES> keep{};

    const std::size_t recentCapacity = std::min(count, MAX_REPLAY_SAMPLES);
    const std::size_t recentCount = selectRecentSampleCount(
            newestFirstTimestamps, recentCapacity, nowTimestamp);
    for (std::size_t i = 0; i < recentCount; i++) {
        keep[i] = true;
    }

    bool stateValid = previousStateValid;
    std::uint32_t stateKey = previousStateKey;
    for (std::size_t i = count; i > 0; i--) {
        const std::size_t index = i - 1;
        const std::uint32_t sampleStateKey = newestFirstStateKeys[index];
        if (!stateValid) {
            stateKey = sampleStateKey;
            stateValid = true;
        }
        else if (sampleStateKey != stateKey) {
            keep[index] = true;
            stateKey = sampleStateKey;
        }
    }

    for (std::size_t i = count; i > 0; i--) {
        const std::size_t index = i - 1;
        if (keep[index]) {
            selection.oldestFirstIndices[selection.count++] = index;
        }
    }

    return selection;
}

} // namespace PenHistory
