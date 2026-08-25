#pragma once

#include <atomic>

// Coalesces repeated overlay wake requests while preserving the first edge.
// The SDL loop consumes the pending edge immediately before pumping Qt so a
// request raised during event processing remains pending for the next pass.
class OverlayEventWakeState
{
public:
    bool request()
    {
        bool expected = false;
        return m_Pending.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel);
    }

    bool isPending() const
    {
        return m_Pending.load(std::memory_order_acquire);
    }

    bool take()
    {
        return m_Pending.exchange(false, std::memory_order_acq_rel);
    }

    void clear()
    {
        m_Pending.store(false, std::memory_order_release);
    }

private:
    std::atomic_bool m_Pending{false};
};
