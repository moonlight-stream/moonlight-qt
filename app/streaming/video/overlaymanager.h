#pragma once

#include <QString>

#include "SDL_compat.h"
#include <SDL_ttf.h>

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
    ~OverlayManager();

    bool isOverlayEnabled(OverlayType type);
    char* getOverlayText(OverlayType type);
    void updateOverlayText(OverlayType type, const char* text);
    int getOverlayMaxTextLength();
    void setOverlayTextUpdated(OverlayType type);
    void setOverlayState(OverlayType type, bool enabled);
    SDL_Color getOverlayColor(OverlayType type);
    int getOverlayFontSize(OverlayType type);
    SDL_Surface* getUpdatedOverlaySurface(OverlayType type);

    void setOverlayRenderer(IOverlayRenderer* renderer);
    void setOverlayBackgroundRGBA(Uint8 r, Uint8 g, Uint8 b, Uint8 a);
    void setOverlayTextColorRGBA(Uint8 r, Uint8 g, Uint8 b, Uint8 a);

private:
    void notifyOverlayUpdated(OverlayType type);

    struct {
        bool enabled;
        int fontSize;
        char text[512];

        TTF_Font* font;
        SDL_Surface* surface;
    } m_Overlays[OverlayMax];
    SDL_Color m_overlayBackgroundColor = {0, 0, 0, 128}; // See through grey background
    SDL_Color m_overlayTextColor = {255, 255, 255, 255}; // White text
    IOverlayRenderer* m_Renderer;
    QByteArray m_FontData;
};

}
