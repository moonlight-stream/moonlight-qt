#include "overlaymanager.h"
#include "path.h"

#ifdef Q_OS_WIN32
#include "streaming/streamutils.h"
#include <QCoreApplication>
#include <QPainter>
#endif

using namespace Overlay;

OverlayManager::OverlayManager() :
    m_Renderer(nullptr),
    m_FontData(Path::readDataFile("ModeSeven.ttf"))
{
    memset(m_Overlays, 0, sizeof(m_Overlays));

    m_Overlays[OverlayType::OverlayDebug].color = {0xD0, 0xD0, 0x00, 0xFF};
    m_Overlays[OverlayType::OverlayDebug].fontSize = 20;

    m_Overlays[OverlayType::OverlayStatusUpdate].color = {0xCC, 0x00, 0x00, 0xFF};
    m_Overlays[OverlayType::OverlayStatusUpdate].fontSize = 36;

    // While TTF will usually not be initialized here, it is valid for that not to
    // be the case, since Session destruction is deferred and could overlap with
    // the lifetime of a new Session object.
    //SDL_assert(TTF_WasInit() == 0);

    if (TTF_Init() != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "TTF_Init() failed: %s",
                    TTF_GetError());
        return;
    }
}

OverlayManager::~OverlayManager()
{
    for (int i = 0; i < OverlayType::OverlayMax; i++) {
        if (m_Overlays[i].surface != nullptr) {
            SDL_FreeSurface(m_Overlays[i].surface);
        }
        if (m_Overlays[i].font != nullptr) {
            TTF_CloseFont(m_Overlays[i].font);
        }
    }

    TTF_Quit();

    // For similar reasons to the comment in the constructor, this will usually,
    // but not always, deinitialize TTF. In the cases where Session objects overlap
    // in lifetime, there may be an additional reference on TTF for the new Session
    // that means it will not be cleaned up here.
    //SDL_assert(TTF_WasInit() == 0);
}

bool OverlayManager::isOverlayEnabled(OverlayType type)
{
    return m_Overlays[type].enabled;
}

char* OverlayManager::getOverlayText(OverlayType type)
{
    return m_Overlays[type].text;
}

void OverlayManager::updateOverlayText(OverlayType type, const char* text)
{
    SDL_utf8strlcpy(m_Overlays[type].text, text, sizeof(m_Overlays[0].text));
    setOverlayTextUpdated(type);
}

int OverlayManager::getOverlayMaxTextLength()
{
    return sizeof(m_Overlays[0].text);
}

int OverlayManager::getOverlayFontSize(OverlayType type)
{
    return m_Overlays[type].fontSize;
}

SDL_Surface* OverlayManager::getUpdatedOverlaySurface(OverlayType type)
{
    // If a new surface is available, return it. If not, return nullptr.
    // Caller must free the surface on success.
    return (SDL_Surface*)SDL_AtomicSetPtr((void**)&m_Overlays[type].surface, nullptr);
}

void OverlayManager::setOverlayTextUpdated(OverlayType type)
{
    // Only update the overlay state if it's enabled. If it's not enabled,
    // the renderer has already been notified by setOverlayState().
    if (m_Overlays[type].enabled) {
        notifyOverlayUpdated(type);
    }
}

void OverlayManager::setOverlayState(OverlayType type, bool enabled)
{
    bool stateChanged = m_Overlays[type].enabled != enabled;

    m_Overlays[type].enabled = enabled;

    if (stateChanged) {
        if (!enabled) {
            // Set the text to empty string on disable
            m_Overlays[type].text[0] = 0;
        }

        notifyOverlayUpdated(type);
    }
}

SDL_Color OverlayManager::getOverlayColor(OverlayType type)
{
    return m_Overlays[type].color;
}

void OverlayManager::setOverlayRenderer(IOverlayRenderer* renderer)
{
    {
        QWriteLocker lock(&m_OverlayRendererLock);
        m_Renderer = renderer;
    }
#ifdef Q_OS_WIN32
    if (renderer && isOverlayEnabled(OverlayFullscreenControls)) {
        notifyOverlayUpdated(OverlayFullscreenControls);
    }
    if (renderer && isOverlayEnabled(OverlayClipboardUpload)) notifyOverlayUpdated(OverlayClipboardUpload);
#endif
}

void OverlayManager::notifyOverlayUpdated(OverlayType type)
{
    QReadLocker lock(&m_OverlayRendererLock);

    if (m_Renderer == nullptr) {
        return;
    }

#ifdef Q_OS_WIN32
    if (type == OverlayFullscreenControls || type == OverlayClipboardUpload) {
        SDL_Surface* surface = nullptr;
        if (isOverlayEnabled(type)) {
            surface = type == OverlayClipboardUpload ? m_ClipboardUpload.render() : renderControls();
        }
        SDL_FreeSurface(static_cast<SDL_Surface*>(SDL_AtomicSetPtr(
            reinterpret_cast<void**>(&m_Overlays[type].surface), surface)));
        m_Renderer->notifyOverlayUpdated(type);
        return;
    }
#endif

    // Construct the required font to render the overlay
    if (m_Overlays[type].font == nullptr) {
        if (m_FontData.isEmpty()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL overlay font failed to load");
            return;
        }

        // m_FontData must stay around until the font is closed
        m_Overlays[type].font = TTF_OpenFontRW(SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()),
                                               1,
                                               m_Overlays[type].fontSize);
        if (m_Overlays[type].font == nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "TTF_OpenFont() failed: %s",
                        TTF_GetError());

            // Can't proceed without a font
            return;
        }
    }

    // Exchange the old surface with the new one
    SDL_Surface* oldSurface = (SDL_Surface*)SDL_AtomicSetPtr(
        (void**)&m_Overlays[type].surface,
        m_Overlays[type].enabled ?
            // The _Wrapped variant is required for line breaks to work
            RenderTextOutlinedWrapped(m_Overlays[type].font,
                                      m_Overlays[type].text,
                                      m_Overlays[type].color,
                                      {0, 0, 0, 255},
                                      4,
                                      1024)
            : nullptr);

    // Notify the renderer
    m_Renderer->notifyOverlayUpdated(type);

    // Free the old surface
    if (oldSurface != nullptr) {
        SDL_FreeSurface(oldSurface);
    }
}

SDL_Surface* OverlayManager::RenderTextOutlinedWrapped(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color outlineColor, int outlineWidth, int wrapWidth) {
    if (text == nullptr || text[0] == '\0') {
        return nullptr;
    }

    int oldOutline = TTF_GetFontOutline(font);
    TTF_SetFontOutline(font, outlineWidth);

    // Verify that the string won't require wrapping (which could cause the outline and the text
    // to diverge due to different wrapping positions).
    //
    // FIXME: We do this rather than just disabling wrapping entirely (wrapWidth = 0) because we
    // need further testing to ensure that all renderers can handle non-NPOT overlay textures.
    for (const QString& line : QString(text).split('\n')) {
        int extent, count;
        if (TTF_MeasureUTF8(font, line.toUtf8(), wrapWidth, &extent, &count) == 0 && count < line.size()) {
            // If it requires wrapping, render it without the outline
            TTF_SetFontOutline(font, oldOutline);
            return TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, wrapWidth);
        }
    }

    // Draw text twice, but outline is a bit bigger
    auto outlineSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, outlineColor, wrapWidth);
    TTF_SetFontOutline(font, 0);
    auto textSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, wrapWidth);
    TTF_SetFontOutline(font, oldOutline);

    if (outlineSurface == nullptr || textSurface == nullptr) {
        SDL_FreeSurface(outlineSurface);
        SDL_FreeSurface(textSurface);
        return nullptr;
    }

    // Merge the texts
    SDL_Rect dst = { outlineWidth, outlineWidth, textSurface->w, textSurface->h };
    SDL_BlitSurface(textSurface, nullptr, outlineSurface, &dst);

    SDL_FreeSurface(textSurface);
    return outlineSurface;
}



#ifdef Q_OS_WIN32
void OverlayManager::configureControls(int streamWidth, int streamHeight)
{
    m_StreamWidth = streamWidth;
    m_StreamHeight = streamHeight;
}

SDL_Rect OverlayManager::videoRect(int width, int height) const
{
    SDL_Rect source = {0, 0, m_StreamWidth, m_StreamHeight};
    SDL_Rect video = {0, 0, width, height};
    StreamUtils::scaleSourceToDestinationSurface(&source, &video);
    return video;
}

void OverlayManager::updateClipboardUpload(const ClipboardUploadProgress& upload, QSize window, qreal scale)
{
    if (!m_ClipboardUpload.update(upload, window, scale)) return;
    const bool visible = !upload.names.isEmpty();
    if (isOverlayEnabled(OverlayClipboardUpload) != visible) setOverlayState(OverlayClipboardUpload, visible);
    else if (visible) setOverlayTextUpdated(OverlayClipboardUpload);
}

ClipboardUploadOverlay::Input OverlayManager::handleClipboardUpload(const SDL_Event& event)
{
    bool hoverChanged;
    const auto result = m_ClipboardUpload.handle(event, hoverChanged);
    if (hoverChanged) setOverlayTextUpdated(OverlayClipboardUpload);
    return result;
}

SDL_Rect OverlayManager::panelRect(OverlayType type, int width, int height, int panelWidth, int panelHeight) const
{
    if (type == OverlayClipboardUpload) return m_ClipboardUpload.destinationRect(width, height);
    return controlsRect(width, height, panelWidth, panelHeight);
}

SDL_Rect OverlayManager::controlsRect(int width, int height, int panelWidth, int panelHeight) const
{
    auto video = videoRect(width, height);
    QReadLocker lock(&m_HandlePositionLock);
    int left = qRound(m_HandlePosition.x() * qMax(0, video.w - HandleWidth));
    int top = qRound(m_HandlePosition.y() * qMax(0, video.h - HandleHeight));
    left = qBound(0, left + (HandleWidth - panelWidth) / 2, qMax(0, video.w - panelWidth));
    top = qBound(0, top + (HandleHeight - panelHeight) / 2, qMax(0, video.h - panelHeight));
    if (m_HandlePosition.x() == 0.0) {
        left = 0;
    }
    else if (m_HandlePosition.x() == 1.0) {
        left = qMax(0, video.w - panelWidth);
    }
    return {video.x + left, video.y + top, panelWidth, panelHeight};
}

OverlayManager::HandleEdge OverlayManager::handleEdge() const
{
    QReadLocker lock(&m_HandlePositionLock);
    if (m_HandlePosition.x() == 0.0) {
        return HandleEdge::Left;
    }
    if (m_HandlePosition.x() == 1.0) {
        return HandleEdge::Right;
    }
    return HandleEdge::None;
}

// Keep drag offsets in the same coordinate space even when the tab rotates.
SDL_Rect OverlayManager::dragReferenceRect(int width, int height) const
{
    return controlsRect(width, height, HandleWidth, HandleHeight);
}

void OverlayManager::moveHandle(int width, int height, int left, int top)
{
    auto video = videoRect(width, height);
    QPointF position(qBound(0.0, double(left - video.x) / qMax(1, video.w - HandleWidth), 1.0),
                     qBound(0.0, double(top - video.y) / qMax(1, video.h - HandleHeight), 1.0));
    {
        QWriteLocker lock(&m_HandlePositionLock);
        if (position == m_HandlePosition) {
            return;
        }
        m_HandlePosition = position;
    }
    setOverlayTextUpdated(OverlayFullscreenControls);
}

OverlayManager::Control OverlayManager::controlAt(int width, int height, int x, int y) const
{
    bool sideTab = handleEdge() != HandleEdge::None;
    auto rect = controlsRect(width, height, m_Expanded ? MenuWidth : (sideTab ? HandleHeight : HandleWidth),
                             m_Expanded ? MenuHeight : (sideTab ? HandleWidth : HandleHeight));
    SDL_Point point = {x, y};
    if (!SDL_PointInRect(&point, &rect)) {
        return Control::None;
    }
    if (!m_Expanded) {
        return Control::Handle;
    }
    return x < rect.x + rect.w / 2 ? Control::LeaveFullscreen : Control::Disconnect;
}

void OverlayManager::showControls(bool expanded, Control hover)
{
    if (m_Expanded != expanded || m_Hover != hover) {
        m_Expanded = expanded;
        m_Hover = hover;
        setOverlayTextUpdated(OverlayFullscreenControls);
    }
}

SDL_Surface* OverlayManager::renderControls() const
{
    auto edge = m_Expanded ? HandleEdge::None : handleEdge();
    bool sideTab = edge != HandleEdge::None;
    int width = m_Expanded ? MenuWidth : (sideTab ? HandleHeight : HandleWidth);
    int height = m_Expanded ? MenuHeight : (sideTab ? HandleWidth : HandleHeight);
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        return nullptr;
    }
    QImage image(static_cast<uchar*>(surface->pixels), width, height, surface->pitch, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(248, 250, 252, 245));
    // Draw the collapsed tab in its original coordinates. Rotating the whole
    // tab puts the flat edge against the video boundary and points the chevron inward.
    if (edge == HandleEdge::Left) {
        painter.translate(0, height);
        painter.rotate(-90);
    }
    else if (edge == HandleEdge::Right) {
        painter.translate(width, 0);
        painter.rotate(90);
    }
    int faceWidth = sideTab ? height : width;
    int faceHeight = sideTab ? width : height;
    painter.drawRoundedRect(QRectF(0, -5, faceWidth, faceHeight + 5), 6, 6);
    if (m_Expanded) {
        QFont font;
        font.setPixelSize(15);
        painter.setFont(font);
        for (int i = 0; i < 2; ++i) {
            QRect rect(i * width / 2, 0, width / 2, height);
            if (m_Hover == (i == 0 ? Control::LeaveFullscreen : Control::Disconnect)) {
                painter.fillRect(rect.adjusted(3, 3, -3, -3), QColor("#dce8fa"));
            }
            painter.setPen(QColor("#20364e"));
            painter.drawText(rect, Qt::AlignCenter, i == 0 ?
                QCoreApplication::translate("Session", "Exit fullscreen") :
                QCoreApplication::translate("Session", "Disconnect"));
        }
    }
    else {
        painter.setPen(QPen(m_Hover == Control::Handle ? QColor("#2468d5") : QColor("#20364e"), 2));
        painter.drawPolyline(QPolygonF({QPointF(faceWidth/2-5, 6), QPointF(faceWidth/2, 11), QPointF(faceWidth/2+5, 6)}));
    }
    return surface;
}
#endif
