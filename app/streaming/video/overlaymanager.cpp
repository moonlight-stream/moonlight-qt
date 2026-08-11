#include "overlaymanager.h"
#include "path.h"

#include <QFile>

using namespace Overlay;

OverlayManager::OverlayManager() :
    m_Renderer(nullptr),
    m_FontData(Path::readDataFile("ModeSeven.ttf"))
{
    memset(m_Overlays, 0, sizeof(m_Overlays));

#ifdef Q_OS_WIN32
    // Prefer a clean system UI font for the stats panel when available
    QFile segoeFont("C:/Windows/Fonts/segoeui.ttf");
    if (segoeFont.open(QIODevice::ReadOnly)) {
        m_DebugFontData = segoeFont.readAll();
    }
#endif

    m_Overlays[OverlayType::OverlayDebug].color = {0xF0, 0xF3, 0xF8, 0xFF};
    m_Overlays[OverlayType::OverlayDebug].fontSize = 17;

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
    m_Renderer = renderer;
}

void OverlayManager::notifyOverlayUpdated(OverlayType type)
{
    if (m_Renderer == nullptr) {
        return;
    }

    // Construct the required font to render the overlay
    if (m_Overlays[type].font == nullptr) {
        // The debug overlay prefers the system UI font when one was loaded
        QByteArray& fontData = (type == OverlayType::OverlayDebug && !m_DebugFontData.isEmpty()) ?
                    m_DebugFontData : m_FontData;

        if (fontData.isEmpty()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL overlay font failed to load");
            return;
        }

        // fontData must stay around until the font is closed
        m_Overlays[type].font = TTF_OpenFontRW(SDL_RWFromConstMem(fontData.constData(), fontData.size()),
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
    SDL_Surface* newSurface = nullptr;
    if (m_Overlays[type].enabled) {
        if (type == OverlayType::OverlayDebug) {
            // The stats overlay renders as a compact panel instead of outlined text
            newSurface = RenderStatsPanel(m_Overlays[type].font,
                                          m_Overlays[type].text,
                                          m_Overlays[type].color,
                                          1024);
        }
        else {
            // The _Wrapped variant is required for line breaks to work
            newSurface = RenderTextOutlinedWrapped(m_Overlays[type].font,
                                                   m_Overlays[type].text,
                                                   m_Overlays[type].color,
                                                   {0, 0, 0, 255},
                                                   4,
                                                   1024);
        }
    }
    SDL_Surface* oldSurface = (SDL_Surface*)SDL_AtomicSetPtr(
        (void**)&m_Overlays[type].surface, newSurface);

    // Notify the renderer
    m_Renderer->notifyOverlayUpdated(type);

    // Free the old surface
    if (oldSurface != nullptr) {
        SDL_FreeSurface(oldSurface);
    }
}

// Render overlay text as a compact panel: translucent dark card with rounded
// corners and an accent bar, in the style of typical performance HUDs.
SDL_Surface* OverlayManager::RenderStatsPanel(TTF_Font* font, const char* text, SDL_Color textColor, int wrapWidth)
{
    if (text == nullptr || text[0] == '\0') {
        return nullptr;
    }

    SDL_Surface* textSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, wrapWidth);
    if (textSurface == nullptr) {
        return nullptr;
    }

    constexpr int kPadX = 14;
    constexpr int kPadY = 10;
    constexpr int kAccent = 3;   // width of the accent bar on the left edge
    constexpr int kRadius = 9;   // corner radius

    int w = textSurface->w + kAccent + 2 * kPadX;
    int h = textSurface->h + 2 * kPadY;

    SDL_Surface* panel = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (panel == nullptr) {
        SDL_FreeSurface(textSurface);
        return nullptr;
    }

    // Card background
    SDL_FillRect(panel, nullptr, SDL_MapRGBA(panel->format, 0x0C, 0x0E, 0x14, 0xD0));

    // Accent bar (left edge)
    SDL_Rect accentRect = { 0, 0, kAccent, h };
    SDL_FillRect(panel, &accentRect, SDL_MapRGBA(panel->format, 0x10, 0x89, 0x3E, 0xFF));

    // Round the corners by clearing pixels outside each corner's arc
    SDL_LockSurface(panel);
    Uint32* pixels = (Uint32*)panel->pixels;
    int pitch = panel->pitch / 4;
    for (int cy = 0; cy < kRadius; cy++) {
        for (int cx = 0; cx < kRadius; cx++) {
            int dx = kRadius - 1 - cx;
            int dy = kRadius - 1 - cy;
            if (dx * dx + dy * dy > kRadius * kRadius) {
                pixels[cy * pitch + cx] = 0;                          // top-left
                pixels[cy * pitch + (w - 1 - cx)] = 0;                // top-right
                pixels[(h - 1 - cy) * pitch + cx] = 0;                // bottom-left
                pixels[(h - 1 - cy) * pitch + (w - 1 - cx)] = 0;      // bottom-right
            }
        }
    }
    SDL_UnlockSurface(panel);

    SDL_Rect dst = { kAccent + kPadX, kPadY, textSurface->w, textSurface->h };
    SDL_BlitSurface(textSurface, nullptr, panel, &dst);
    SDL_FreeSurface(textSurface);

    return panel;
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


