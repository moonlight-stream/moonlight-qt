#include "vrrratepolicy.h"

#include <algorithm>

int VrrRatePolicy::vrrRateForRefresh(int refreshHz)
{
    // SDL reports zero for an unknown refresh rate, and rates at or below one
    // cannot produce a meaningful stream-rate recommendation either.
    if (refreshHz <= 1) {
        return 0;
    }

    // Keep this integer-only so the documented floor behavior is stable on
    // every supported compiler. floor(r - r^2 / 3600) is not the same as
    // r - floor(r^2 / 3600) for rates such as 144 Hz.
    return static_cast<int>((static_cast<long long>(refreshHz) *
                             (3600LL - refreshHz)) / 3600LL);
}

int VrrRatePolicy::lowLatencyRateForRefresh(int refreshHz)
{
    if (refreshHz <= 1) {
        return 0;
    }

    return (refreshHz / 6) * 5;
}

bool VrrRatePolicy::hasAdaptiveHeadroom(int streamRateHz, int displayRefreshHz)
{
    if (streamRateHz <= 0 || displayRefreshHz <= 0) {
        return false;
    }

    constexpr long long microsecondsPerSecond = 1000000LL;
    const auto periodForRate = [](int rateHz) {
        const long long rate = static_cast<long long>(rateHz);
        return std::max(1LL,
                        (microsecondsPerSecond + rate / 2) / rate);
    };

    const long long displayPeriodUs = periodForRate(displayRefreshHz);
    const long long streamPeriodUs = periodForRate(streamRateHz);
    const long long guardUs = std::max(100LL,
                                      std::min(displayPeriodUs / 64, 250LL));
    return streamPeriodUs > displayPeriodUs + guardUs;
}
