#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace dualsense_haptics {

struct LocalControllerCandidate
{
    int logicalNumber;
    bool dualSense;
};

inline int selectUniqueLocalDualSense(const LocalControllerCandidate* controllers,
                                     std::size_t controllerCount,
                                     bool multiController)
{
    if (controllers == nullptr || controllerCount == 0 ||
        (!multiController && controllerCount != 1)) {
        return -1;
    }

    int selected = -1;
    for (std::size_t i = 0; i < controllerCount; i++) {
        if (!controllers[i].dualSense) {
            continue;
        }
        if (selected >= 0) {
            return -1;
        }
        selected = controllers[i].logicalNumber;
    }
    return selected;
}

inline bool canUseNativeController(std::uint16_t controllerNumber,
                                   int selectedLocalController,
                                   std::size_t nativeControllerCount)
{
    return selectedLocalController >= 0 &&
           controllerNumber == static_cast<std::uint16_t>(selectedLocalController) &&
           nativeControllerCount == 1;
}

class IrBackendLatch
{
public:
    bool shouldAttemptNative(std::uint16_t controllerNumber, bool streamEnd)
    {
        if (streamEnd) {
            m_FallbackControllers.erase(controllerNumber);
            return false;
        }
        return m_FallbackControllers.find(controllerNumber) == m_FallbackControllers.end();
    }

    void useFallback(std::uint16_t controllerNumber)
    {
        m_FallbackControllers.insert(controllerNumber);
    }

    void reset()
    {
        m_FallbackControllers.clear();
    }

private:
    std::unordered_set<std::uint16_t> m_FallbackControllers;
};

} // namespace dualsense_haptics
