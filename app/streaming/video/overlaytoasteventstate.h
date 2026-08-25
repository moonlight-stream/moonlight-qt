#pragma once

#include <climits>
#include <cstdint>

// Tracks only the scheduling state for OverlayToast. Keeping this separate
// from QRasterWindow makes the idle/deadline behavior deterministic and easy
// to test without running a platform GUI event loop.
class OverlayToastEventState
{
public:
    enum class Phase {
        Hidden,
        Static,
        Fading,
    };

    void show(std::int64_t nowMs, int durationMs)
    {
        m_Phase = Phase::Static;
        m_DeadlineMs = nowMs + (durationMs > 0 ? durationMs : 0);
        m_Pending = true;
    }

    void cancel()
    {
        m_Phase = Phase::Hidden;
        m_DeadlineMs = 0;
        m_Pending = false;
    }

    bool needsEventProcessing(std::int64_t nowMs) const
    {
        return m_Pending ||
                m_Phase == Phase::Fading ||
                (m_Phase == Phase::Static && nowMs >= m_DeadlineMs);
    }

    int nextEventDelayMs(std::int64_t nowMs) const
    {
        if (m_Phase != Phase::Static || m_Pending) {
            return -1;
        }

        const std::int64_t remainingMs = m_DeadlineMs - nowMs;
        if (remainingMs <= 0) {
            return 0;
        }
        if (remainingMs > INT_MAX) {
            return INT_MAX;
        }
        return static_cast<int>(remainingMs);
    }

    // Consumes the current pending edge and transitions an expired static
    // toast into its fade phase. Returns true only when fading should start.
    bool beginEventProcessing(std::int64_t nowMs)
    {
        m_Pending = false;
        if (m_Phase == Phase::Static && nowMs >= m_DeadlineMs) {
            m_Phase = Phase::Fading;
            return true;
        }
        return false;
    }

    void finishFade()
    {
        cancel();
    }

    bool isVisible() const { return m_Phase != Phase::Hidden; }
    bool isFading() const { return m_Phase == Phase::Fading; }

private:
    Phase m_Phase = Phase::Hidden;
    std::int64_t m_DeadlineMs = 0;
    bool m_Pending = false;
};
