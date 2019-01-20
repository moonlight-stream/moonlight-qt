#pragma once

#include <QString>

class OverlayManager
{
public:
    enum OverlayType {
        OverlayDebug,
        OverlayMinorNotification,
        OverlayMajorNotification,
        OverlayMax
    };

    OverlayManager();

    bool isOverlayEnabled(OverlayType type);
    char* getOverlayText(OverlayType type);
    void setOverlayState(OverlayType type, bool enabled);

    struct {
        bool enabled;
        int updateSeq;
        char text[512];
    } m_Overlays[OverlayMax];
};
