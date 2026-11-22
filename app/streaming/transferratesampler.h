#pragma once

#include <QtGlobal>
#include <chrono>
#include <deque>

// Feed bytes confirmed during this attempt, including stationary samples while a transfer stalls.
class TransferRateSampler
{
public:
    void reset()
    {
        m_Samples.clear();
        m_Rate = -1;
    }

    qint64 sample(quint64 bytes)
    {
        using namespace std::chrono;
        const auto now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        if (m_Samples.empty() || bytes < m_Samples.back().bytes) {
            reset();
            m_Samples.push_back({now, bytes});
            return m_Rate;
        }
        if (now - m_Samples.back().timeMs < 250) return m_Rate;
        while (m_Samples.size() > 1 && now - m_Samples[1].timeMs >= 2000) m_Samples.pop_front();
        m_Samples.push_back({now, bytes});
        const auto& oldest = m_Samples.front();
        const auto elapsed = now - oldest.timeMs;
        m_Rate = elapsed < 500 ? -1 : qint64(double(bytes - oldest.bytes) * 1000.0 / elapsed);
        return m_Rate;
    }

private:
    struct Sample { qint64 timeMs; quint64 bytes; };
    std::deque<Sample> m_Samples;
    qint64 m_Rate = -1;
};
