#pragma once

#include <QString>
#include <QPointF>
#include <QReadWriteLock>

#include "SDL_compat.h"
#include <SDL_ttf.h>
#ifdef Q_OS_WIN32
#include "clipboarduploadoverlay.h"
#endif

namespace Overlay {

enum OverlayType {
    OverlayDebug,
    OverlayStatusUpdate,
#ifdef Q_OS_WIN32
    OverlayFullscreenControls,
    OverlayClipboardUpload,
#endif
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

#ifdef Q_OS_WIN32
    enum class Control { None, Handle, LeaveFullscreen, FileTransfer, Disconnect };
    void configureControls(int streamWidth, int streamHeight, bool fileTransfer = true);
    void updateClipboardUpload(const ClipboardUploadProgress& upload, QSize window, qreal scale);
    ClipboardUploadOverlay::Input handleClipboardUpload(const SDL_Event& event);
    SDL_Rect panelRect(OverlayType type, int width, int height, int panelWidth, int panelHeight) const;
    SDL_Rect controlsRect(int width, int height, int panelWidth, int panelHeight) const;
    SDL_Rect dragReferenceRect(int width, int height) const;
    void moveHandle(int width, int height, int left, int top);
    Control controlAt(int width, int height, int x, int y) const;
    void showControls(bool expanded, Control hover = Control::None);
#endif
    void setOverlayRenderer(IOverlayRenderer* renderer);

private:
    void notifyOverlayUpdated(OverlayType type);
    SDL_Surface* RenderTextOutlinedWrapped(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color outlineColor, int outlineWidth, int wrapWidth);

#ifdef Q_OS_WIN32
    enum class HandleEdge { None, Left, Right };
    HandleEdge handleEdge() const;
    SDL_Rect videoRect(int width, int height) const;
    mutable QReadWriteLock m_HandlePositionLock;
    QPointF m_HandlePosition{0.5, 0.0}; // Fraction of the available travel on each axis
    ClipboardUploadOverlay m_ClipboardUpload;
    SDL_Surface* renderControls() const;
    int m_StreamWidth = 1, m_StreamHeight = 1;
    bool m_Expanded = false;
    bool m_FileTransfer = true;
    Control m_Hover = Control::None;
    static constexpr int HandleWidth = 52, HandleHeight = 18;
    static constexpr int MenuWidth = 396, MenuHeight = 40;
#endif
    struct {
        bool enabled;
        int fontSize;
        SDL_Color color;
        char text[1024];

        TTF_Font* font;
        SDL_Surface* surface;
    } m_Overlays[OverlayMax];
    IOverlayRenderer* m_Renderer;
    QByteArray m_FontData;
    QReadWriteLock m_OverlayRendererLock;
};

}
