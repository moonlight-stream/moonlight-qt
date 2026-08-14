#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <Limelight.h>

namespace dualsense_haptics {
struct RumbleOutput
{
    std::uint16_t lowFrequency;
    std::uint16_t highFrequency;
};

inline RumbleOutput renderIrV2(const LI_DS5_HAPTICS_IR_FRAME_V2& frame)
{
    if (frame.flags & LI_DS5_HAPTICS_IR_FLAG_STREAM_END) {
        return {0, 0};
    }

    float low = 0.0f;
    float high = 0.0f;
    for (const auto& lane : frame.lanes) {
        low += lane.rmsAmplitude * (0.35f + 0.65f * lane.lowBandRatio);
        high += lane.rmsAmplitude * (1.0f - lane.lowBandRatio) * 0.65f +
                lane.transientStrength * 0.35f;
    }
    low = std::sqrt(std::clamp(low * 0.5f, 0.0f, 1.0f));
    high = std::sqrt(std::clamp(high * 0.5f, 0.0f, 1.0f));
    return {
        static_cast<std::uint16_t>(low * 65535.0f),
        static_cast<std::uint16_t>(high * 65535.0f),
    };
}
} // namespace dualsense_haptics
