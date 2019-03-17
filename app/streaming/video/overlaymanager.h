#pragma once

#include <QString>

#include <SDL.h>

namespace Overlay {

enum OverlayType {
    OverlayDebug,
    OverlayStatusUpdate,
    OverlayMax
};

class IOverlayRenderer
{
public:
    virtual ~IOverlayRenderer() = default;

    virtual void notifyOverlayUpdated(OverlayType type) = 0;
};

class OverlayManager
{
public:
    OverlayManager();

    bool isOverlayEnabled(OverlayType type);
    char* getOverlayText(OverlayType type);
    void setOverlayTextUpdated(OverlayType type);
    void setOverlayState(OverlayType type, bool enabled);
    SDL_Color getOverlayColor(OverlayType type);
    int getOverlayFontSize(OverlayType type);

    void setOverlayRenderer(IOverlayRenderer* renderer);

    struct {
        bool enabled;
        int fontSize;
        SDL_Color color;
        char text[512];
    } m_Overlays[OverlayMax];
    IOverlayRenderer* m_Renderer;
};

}
