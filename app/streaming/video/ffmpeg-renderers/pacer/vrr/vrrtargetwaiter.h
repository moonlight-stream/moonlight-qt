#pragma once

#include <cstdint>

struct VrrTargetWaitResult {
    uint64_t finalNowUs = 0;
    // Additional early wake needed beyond the fixed active margin, measured
    // at the coarse sleep boundary. Unlike final overshoot, this observation
    // remains meaningful after an early-wake correction succeeds.
    uint64_t schedulerDelayUs = 0;
    bool schedulerDelayValid = false;
    bool deadlineAlreadyElapsed = false;
};

class VrrTargetWaiter {
public:
    // Wait until an absolute deadline in the LiGetMicroseconds() epoch, which
    // is also the epoch PacedFrame decode-completion times are stamped in.
    // Learned scheduler delay may extend the early-wake/active region, but it
    // remains bounded even if the platform scheduler fails to advance.
    VrrTargetWaitResult waitUntil(uint64_t deadlineUs,
                                  uint64_t additionalWakeLeadUs = 0) const;
};
