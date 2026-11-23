#pragma once

#include <algorithm>
#include <cstdint>

// Acknowledgements clock the normal upload. Apply a separate pacing limit only
// while the streaming control channel reports queueing.
class ClipboardUploadRate
{
public:
    explicit ClipboardUploadRate(std::uint64_t ethernetBitsPerSecond)
    {
        if (ethernetBitsPerSecond) {
            m_Chunk = static_cast<int>((std::clamp<std::uint64_t>)(
                ethernetBitsPerSecond / 8 / 32, MinChunk, MiB));
        }
    }

    void sample(std::uint32_t rtt, std::uint32_t variance)
    {
        if (!rtt) return;
        m_BaseRtt = m_BaseRtt ? (std::min)(m_BaseRtt, rtt) : rtt;
        const auto tolerance = (std::max)(12U, m_BaseRtt / 3);
        if (rtt - m_BaseRtt > tolerance || variance > tolerance) {
            m_HealthySamples = 0;
            const int previous = m_Limit ? m_Limit : m_DeliveryRate;
            m_Limit = (std::max)(128 * 1024, previous / 2);
        } else if (m_Limit && ++m_HealthySamples >= 3) {
            // Probe gradually after congestion. Once ACKs are the limiting
            // factor again, let the next request follow the ACK immediately.
            const int next = m_Limit * 2;
            m_Limit = next >= m_DeliveryRate ? 0 : next;
            m_HealthySamples = 0;
        }
    }

    void acknowledge(int bytes, std::uint64_t elapsedMs)
    {
        if (bytes != chunkBytes()) return;
        const auto rate = static_cast<std::uint64_t>(bytes) * 1000 /
                          (std::max<std::uint64_t>)(1, elapsedMs);
        m_DeliveryRate = static_cast<int>((std::clamp<std::uint64_t>)(rate, 128 * 1024, 512 * MiB));
        m_Chunk = (std::clamp)(m_DeliveryRate / 20, MinChunk, 4 * MiB);
    }

    int bytesPerSecond() const { return m_Limit; } // Zero permits an immediate send.
    int chunkBytes() const
    {
        return m_Limit ? (std::min)(m_Chunk, m_Limit / 32) : m_Chunk;
    }

private:
    static constexpr int MiB = 1024 * 1024;
    static constexpr int MinChunk = 64 * 1024;
    int m_Chunk = MiB;
    int m_Limit = 0;
    int m_DeliveryRate = 2 * MiB;
    int m_HealthySamples = 0;
    std::uint32_t m_BaseRtt = 0;
};
