#pragma once

#include <algorithm>
#include <cstdint>

// Rates are payload bytes/second. Only a directly attached physical Ethernet
// interface supplies a nonzero link speed. Keep a single bounded request in flight.
class ClipboardUploadRate
{
public:
    explicit ClipboardUploadRate(std::uint64_t ethernetBitsPerSecond, bool loopback = false)
        : m_Ceiling(loopback ? 16 * MiB : DefaultRate)
    {
        if (ethernetBitsPerSecond) {
            // A negotiated link speed is an upper bound; a switch or the peer
            // may be slower. Start at 1/4 of it and leave at least 1/4 free.
            const auto bytes = (std::min<std::uint64_t>)(ethernetBitsPerSecond / 8, 128 * MiB);
            m_Ceiling = (std::max)(1, static_cast<int>(bytes * 3 / 4));
            m_Initial = (std::max)(1, static_cast<int>(bytes / 4));
            m_Rate = (std::min)(DefaultRate, m_Initial);
            m_Wired = true;
        }
    }

    // Zero means that the streaming control channel has no current RTT estimate.
    void sample(std::uint32_t rtt, std::uint32_t variance)
    {
        if (!rtt) {
            m_Rate = (std::min)(m_Rate, DefaultRate);
            return;
        }
        if (!m_BaseRtt) {
            m_BaseRtt = rtt;
            if (rtt <= 12 && variance <= 12) m_Rate = m_Initial;
            return;
        }
        m_BaseRtt = (std::min)(m_BaseRtt, rtt);
        const auto tolerance = (std::max)(12U, m_BaseRtt / 3);
        if (rtt - m_BaseRtt > tolerance || variance > tolerance) {
            const auto floor = (std::min)(m_Rate, 128 * 1024);
            m_Rate = (std::max)(floor, m_Rate / 2);
        } else {
            m_Rate = (std::min)(m_Ceiling, m_Rate + (std::max)(1, m_Rate / 4));
        }
    }

    int sampleIntervalMs() const { return m_Wired ? 250 : 500; }
    int bytesPerSecond() const { return m_Rate; }
    int chunkBytes() const
    {
        // Limit a fast LAN burst to ~16 ms of budget. Keep the existing chunk
        // policy on indirect routes, where latency is a larger part of each ACK.
        return m_Wired ? (std::clamp)(m_Rate / 64, 1, MiB) :
                         (std::clamp)(m_Rate / 32, 1, 256 * 1024);
    }

private:
    static constexpr int MiB = 1024 * 1024;
    static constexpr int DefaultRate = 2 * MiB;
    int m_Ceiling;
    int m_Initial = DefaultRate;
    int m_Rate = DefaultRate;
    std::uint32_t m_BaseRtt = 0;
    bool m_Wired = false;
};
