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
            m_FastEthernet = ethernetBitsPerSecond >= 1'000'000'000;
            const auto bytes = (std::min<std::uint64_t>)(ethernetBitsPerSecond / 8, 1024 * MiB);
            // The far end may have a slower port. Only expand the request size
            // once the peer has actually acknowledged data at the higher rate.
            m_Ceiling = (std::max)(1, static_cast<int>(m_FastEthernet ? bytes * 9 / 10 : bytes * 3 / 4));
            m_Initial = (std::max)(1, static_cast<int>(m_FastEthernet ?
                (std::min<std::uint64_t>)(bytes / 2, 64 * MiB) : bytes / 4));
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
            const int growth = m_FastEthernet && rtt <= 12 && variance <= 12 ? m_Rate / 2 : m_Rate / 4;
            m_Rate = (std::min)(m_Ceiling, m_Rate + (std::max)(1, growth));
        }
    }

    void acknowledge(int bytes, std::uint64_t elapsedMs)
    {
        // A short tail of a file is not a useful full-request timing sample.
        if (!m_FastEthernet || bytes != chunkBytes()) return;
        const auto budget = static_cast<std::uint64_t>(bytes) * 32 / (std::max<std::uint64_t>)(1, elapsedMs);
        m_AckChunkLimit = static_cast<int>((std::clamp<std::uint64_t>)(budget, 64 * 1024, MaxChunk));
    }

    int sampleIntervalMs() const { return m_Wired ? 250 : 500; }
    int bytesPerSecond() const { return m_Rate; }
    int chunkBytes() const
    {
        // On gigabit links each TLS request costs a round trip. Size it for
        // ~32 ms at the lower of the pacing rate and observed delivery rate.
        if (m_FastEthernet && m_Rate > DefaultRate) return (std::clamp)(m_Rate / 32, 1, m_AckChunkLimit);
        return m_Wired ? (std::clamp)(m_Rate / 64, 1, MiB) :
                         (std::clamp)(m_Rate / 32, 1, 256 * 1024);
    }

private:
    static constexpr int MiB = 1024 * 1024;
    static constexpr int DefaultRate = 2 * MiB;
    static constexpr int MaxChunk = 4 * MiB;  // Sunshine clipboard/batch append limit.
    int m_Ceiling;
    int m_Initial = DefaultRate;
    int m_Rate = DefaultRate;
    std::uint32_t m_BaseRtt = 0;
    bool m_Wired = false;
    bool m_FastEthernet = false;
    int m_AckChunkLimit = MiB / 2;
};
