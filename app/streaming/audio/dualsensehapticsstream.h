#pragma once

#include <cstdint>

#include <Limelight.h>

namespace dualsense_haptics {

class PcmStreamTracker
{
public:
    enum class Action
    {
        Accept,
        ResetAndAccept,
        End,
        Ignore,
    };

    Action observe(std::uint8_t flags, std::uint16_t controllerNumber,
                   std::uint32_t sequenceNumber)
    {
        if (flags & LI_DS5_HAPTICS_PCM_FLAG_STREAM_END) {
            if (!m_Active || controllerNumber != m_ControllerNumber) {
                return Action::Ignore;
            }

            m_Active = false;
            return Action::End;
        }

        if (m_Active && controllerNumber != m_ControllerNumber) {
            return Action::Ignore;
        }

        const bool streamStart = flags & LI_DS5_HAPTICS_PCM_FLAG_STREAM_START;
        const bool needsReset =
            streamStart ||
            (flags & LI_DS5_HAPTICS_PCM_FLAG_DISCONTINUITY) ||
            (m_Active && controllerNumber == m_ControllerNumber &&
             sequenceNumber != m_ExpectedSequence);

        m_Active = true;
        m_ControllerNumber = controllerNumber;
        m_ExpectedSequence = sequenceNumber + 1;
        return needsReset ? Action::ResetAndAccept : Action::Accept;
    }

    void reset()
    {
        m_Active = false;
    }

private:
    bool m_Active = false;
    std::uint16_t m_ControllerNumber = 0;
    std::uint32_t m_ExpectedSequence = 0;
};

}
