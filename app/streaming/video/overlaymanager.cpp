#include "overlaymanager.h"

OverlayManager::OverlayManager()
{
    memset(m_Overlays, 0, sizeof(m_Overlays));
}

bool OverlayManager::isOverlayEnabled(OverlayManager::OverlayType type)
{
    return m_Overlays[type].enabled;
}

char* OverlayManager::getOverlayText(OverlayType type)
{
    return m_Overlays[type].text;
}

void OverlayManager::setOverlayState(OverlayManager::OverlayType type, bool enabled)
{
    m_Overlays[type].enabled = enabled;
    if (!enabled) {
        // Set the text to empty string on disable
        m_Overlays[type].text[0] = 0;
    }
}
