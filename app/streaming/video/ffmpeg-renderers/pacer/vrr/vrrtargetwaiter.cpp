#include "vrrtargetwaiter.h"

#include <Limelight.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

namespace {

constexpr uint64_t kMaximumActiveWaitUs = 500;
// Cap the active region so learned scheduler correction cannot turn a
// near-refresh stream into a multi-millisecond TIME_CRITICAL yield loop.
constexpr uint64_t kMaximumAdditionalWakeLeadUs = 500;

uint64_t saturatingAdd(uint64_t left, uint64_t right)
{
    const uint64_t maximum = std::numeric_limits<uint64_t>::max();
    return left > maximum - right ? maximum : left + right;
}

void sleepForUs(uint64_t durationUs)
{
    if (durationUs == 0) {
        return;
    }

#ifdef _WIN32
    struct WaitableTimer {
        WaitableTimer() :
            handle(CreateWaitableTimerExW(nullptr, nullptr,
                                          CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_MODIFY_STATE | SYNCHRONIZE))
        {
        }

        ~WaitableTimer()
        {
            if (handle != nullptr) {
                CloseHandle(handle);
            }
        }

        HANDLE handle;
    };

    // std::this_thread::sleep_for() can overshoot sub-frame deadlines by a
    // large fraction of Windows' timer tick. Reuse one high-resolution timer
    // per pacing thread so the bounded final yield starts before the target
    // instead of after the presentation window has already closed.
    thread_local WaitableTimer timer;
    if (timer.handle != nullptr) {
        LARGE_INTEGER dueTime;
        const uint64_t maximumUs =
            static_cast<uint64_t>(std::numeric_limits<LONGLONG>::max() / 10);
        const uint64_t boundedDurationUs = std::min(durationUs, maximumUs);
        dueTime.QuadPart = -static_cast<LONGLONG>(boundedDurationUs * 10);
        if (SetWaitableTimerEx(timer.handle, &dueTime, 0, nullptr, nullptr,
                               nullptr, 0) &&
                WaitForSingleObject(timer.handle, INFINITE) == WAIT_OBJECT_0) {
            return;
        }
    }
#endif

    std::this_thread::sleep_for(std::chrono::microseconds(durationUs));
}

} // namespace

VrrTargetWaitResult VrrTargetWaiter::waitUntil(
    uint64_t deadlineUs, uint64_t additionalWakeLeadUs) const
{
    VrrTargetWaitResult result;
    uint64_t nowUs = LiGetMicroseconds();
    const uint64_t boundedWakeLeadUs = std::min(
        additionalWakeLeadUs, kMaximumAdditionalWakeLeadUs);
    const uint64_t activeWaitUs = saturatingAdd(kMaximumActiveWaitUs,
                                                 boundedWakeLeadUs);
    if (nowUs >= deadlineUs) {
        result.deadlineAlreadyElapsed = true;
        result.finalNowUs = nowUs;
        return result;
    }

    unsigned int stagnantCoarseSleeps = 0;
    while (nowUs < deadlineUs) {
        const uint64_t remainingUs = deadlineUs - nowUs;
        if (remainingUs <= activeWaitUs) {
            break;
        }

        // Wake before the target by the fixed active margin plus any bounded
        // delay learned from earlier scheduler overshoots. The remaining
        // interval is deliberately delegated to the active path below.
        const uint64_t coarseSleepUs =
            remainingUs > activeWaitUs ? remainingUs - activeWaitUs : 1;
        const uint64_t requestedWakeUs = saturatingAdd(nowUs, coarseSleepUs);
        sleepForUs(coarseSleepUs);

        const uint64_t afterSleepUs = LiGetMicroseconds();
        result.schedulerDelayValid = true;
        if (afterSleepUs > requestedWakeUs) {
            const uint64_t coarseOvershootUs =
                afterSleepUs - requestedWakeUs;
            const uint64_t additionalLeadUs =
                coarseOvershootUs > kMaximumActiveWaitUs ?
                    coarseOvershootUs - kMaximumActiveWaitUs : 0;
            result.schedulerDelayUs = std::max(
                result.schedulerDelayUs, additionalLeadUs);
        }
        if (afterSleepUs <= nowUs) {
            // A platform sleep that was interrupted before it yielded must
            // not leave a pacing worker spinning for an unbounded interval.
            if (++stagnantCoarseSleeps >= 2) {
                nowUs = afterSleepUs;
                break;
            }
        }
        else {
            stagnantCoarseSleeps = 0;
            nowUs = afterSleepUs;
        }
    }

    if (nowUs < deadlineUs) {
        const uint64_t activeLimitUs = saturatingAdd(nowUs, activeWaitUs);
        unsigned int stagnantYields = 0;

        // The monotonic active-time limit is the real safety bound. A fixed
        // yield-count limit is CPU- and scheduler-dependent: on a fast
        // machine it can expire hundreds of microseconds early even though
        // the clock is advancing normally. The stagnant-clock detector below
        // remains the bounded escape from a stopped clock.
        while (nowUs < deadlineUs && nowUs < activeLimitUs) {
            std::this_thread::yield();
            const uint64_t afterYieldUs = LiGetMicroseconds();
            if (afterYieldUs <= nowUs) {
                if (++stagnantYields >= 64) {
                    break;
                }
            }
            else {
                stagnantYields = 0;
                nowUs = afterYieldUs;
            }
        }
    }

    result.finalNowUs = nowUs;
    return result;
}
