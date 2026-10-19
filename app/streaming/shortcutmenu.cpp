#include "shortcutmenu.h"
#include "streaming/input/input.h"
#include "path.h"

#include <QtGlobal>

#include <cstdlib>
#include <cstring>

namespace {

const int kCollapsedWidth = 150;
const int kHeaderHeight = 28;
const int kPadding = 8;
const int kTitleHeight = 24;
const int kRowHeight = 26;
const int kColumnWidth = 290;
const int kDefaultTopMargin = 6;
const int kDragThresholdPx = 4;

const SDL_Color kTextColor = {0xEB, 0xEB, 0xF0, 0xFF};
const SDL_Color kHintColor = {0x9A, 0xA0, 0xB0, 0xFF};
const SDL_Color kTitleColor = {0x9C, 0xC8, 0xFF, 0xFF};
const SDL_Color kDangerColor = {0xF0, 0x6A, 0x6A, 0xFF};

bool pointInRect(const SDL_Rect& r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

} // namespace

bool ShortcutMenu::isSupportedOnThisPlatform()
{
#ifdef STEAM_LINK
    return false;
#else
    if (qEnvironmentVariableIsSet("ML_DISABLE_SHORTCUT_MENU")) {
        return false;
    }

    // We need a windowing system that supports multiple top-level windows that can be
    // positioned by the application and kept on top.
    const char* driver = SDL_GetCurrentVideoDriver();
    return driver != nullptr &&
           (strcmp(driver, "windows") == 0 ||
            strcmp(driver, "x11") == 0 ||
            strcmp(driver, "cocoa") == 0);
#endif
}

ShortcutMenu::ShortcutMenu(SDL_Window* parentWindow, SdlInputHandler* inputHandler)
    : m_Parent(parentWindow),
      m_Input(inputHandler),
      m_Window(nullptr),
      m_Font(nullptr),
      m_SmallFont(nullptr),
      m_FontData(Path::readDataFile("ModeSeven.ttf")),
      m_TtfInitialized(false),
      m_ExpandedWidth(0),
      m_ExpandedHeight(0),
      m_Expanded(false),
      m_Shown(false),
      m_HoverIndex(-1),
      m_HeaderHover(false),
      m_ConfirmIndex(-1),
      m_RecaptureOnClose(false),
      m_HasCustomPosition(false),
      m_OffsetX(0),
      m_OffsetY(kDefaultTopMargin),
      m_MouseDown(false),
      m_Dragging(false),
      m_DownGlobalX(0), m_DownGlobalY(0),
      m_GrabLocalX(0), m_GrabLocalY(0),
      m_HintOverridden(false),
      m_SavedHint(nullptr)
{
    m_ColumnTitles[0] = "Send to remote PC";
    m_ColumnTitles[1] = "Moonlight (Ctrl+Alt+Shift+key)";

    if (m_FontData.isEmpty()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Shortcut menu: font failed to load");
        return;
    }

    if (TTF_Init() != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Shortcut menu: TTF_Init() failed: %s", TTF_GetError());
        return;
    }
    m_TtfInitialized = true;

    m_Font = TTF_OpenFontRW(SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()), 1, 15);
    m_SmallFont = TTF_OpenFontRW(SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()), 1, 12);
    if (m_Font == nullptr || m_SmallFont == nullptr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Shortcut menu: TTF_OpenFont() failed: %s", TTF_GetError());
        return;
    }

    buildItems();
    layoutItems();

    Uint32 flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS;
#if SDL_VERSION_ATLEAST(2, 0, 5)
    flags |= SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR;
#endif
    SDL_Window* window = SDL_CreateWindow("Moonlight Shortcuts",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          kCollapsedWidth, kHeaderHeight, flags);
    if (window == nullptr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Shortcut menu: SDL_CreateWindow() failed: %s", SDL_GetError());
        return;
    }
    m_Window = window;
}

ShortcutMenu::~ShortcutMenu()
{
    endDrag();
    overrideMinimizeOnFocusLoss(false);

    if (m_Window != nullptr) {
        SDL_DestroyWindow(m_Window);
    }
    if (m_Font != nullptr) {
        TTF_CloseFont(m_Font);
    }
    if (m_SmallFont != nullptr) {
        TTF_CloseFont(m_SmallFont);
    }
    if (m_TtfInitialized) {
        TTF_Quit();
    }
}

void ShortcutMenu::buildItems()
{
    // Windows virtual-key codes
    const unsigned short CTRL = 0xA2, SHIFT = 0xA0, ALT = 0xA4, WIN = 0x5B;

    auto remote = [this](const char* label, const char* hint, std::vector<unsigned short> keys) {
        m_Items.push_back({label, hint, keys, -1, false, 0, {0, 0, 0, 0}});
    };
    auto client = [this](const char* label, const char* hint, SdlInputHandler::KeyCombo combo, bool dangerous) {
        m_Items.push_back({label, hint, {}, (int)combo, dangerous, 1, {0, 0, 0, 0}});
    };

    // Column 0: keys sent to the remote PC
    remote("Ctrl+Alt+Del",             "",              {CTRL, ALT, 0x2E});
    remote("Alt+Tab",                  "",              {ALT, 0x09});
    remote("Alt+F4",                   "",              {ALT, 0x73});
    remote("Task Manager",             "Ctrl+Shift+Esc",{CTRL, SHIFT, 0x1B});
    remote("Win (Start menu)",         "",              {WIN});
    remote("Show desktop",             "Win+D",         {WIN, 0x44});
    remote("File Explorer",            "Win+E",         {WIN, 0x45});
    remote("Lock PC",                  "Win+L",         {WIN, 0x4C});
    remote("Run",                      "Win+R",         {WIN, 0x52});
    remote("Task view",                "Win+Tab",       {WIN, 0x09});
    remote("Snipping tool",            "Win+Shift+S",   {WIN, SHIFT, 0x53});
    remote("Print Screen",             "",              {0x2C});

    // Column 1: Moonlight's own client-side shortcuts
    client("Release/grab mouse",       "Z", SdlInputHandler::KeyComboUngrabInput, false);
    client("Toggle fullscreen",        "X", SdlInputHandler::KeyComboToggleFullScreen, false);
    client("Performance stats",        "S", SdlInputHandler::KeyComboToggleStatsOverlay, false);
    client("Mouse mode",               "M", SdlInputHandler::KeyComboToggleMouseMode, false);
    client("Show/hide cursor",         "C", SdlInputHandler::KeyComboToggleCursorHide, false);
    client("Minimize",                 "D", SdlInputHandler::KeyComboToggleMinimize, false);
    client("Paste clipboard as typing","V", SdlInputHandler::KeyComboPasteText, false);
    client("Pointer region lock",      "L", SdlInputHandler::KeyComboTogglePointerRegionLock, false);
    client("System key capture",       "K", SdlInputHandler::KeyComboToggleKeyboardGrab, false);
    client("Quit stream",              "Q", SdlInputHandler::KeyComboQuit, true);
    client("Quit stream + exit app",   "E", SdlInputHandler::KeyComboQuitAndExit, true);
}

void ShortcutMenu::layoutItems()
{
    int rowY[2];
    rowY[0] = rowY[1] = kHeaderHeight + kPadding + kTitleHeight;

    for (Item& item : m_Items) {
        item.rect.x = kPadding + item.column * (kColumnWidth + kPadding);
        item.rect.y = rowY[item.column];
        item.rect.w = kColumnWidth;
        item.rect.h = kRowHeight;
        rowY[item.column] += kRowHeight;
    }

    m_ExpandedWidth = kPadding * 3 + kColumnWidth * 2;
    m_ExpandedHeight = qMax(rowY[0], rowY[1]) + kPadding;
}

void ShortcutMenu::showWithoutActivation()
{
#ifdef SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN
    SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, "1");
#endif
    SDL_ShowWindow(m_Window);
}

void ShortcutMenu::overrideMinimizeOnFocusLoss(bool enable)
{
    // While the pointer is over our window, a click will move keyboard focus away from the
    // stream window. SDL would normally minimize a full-screen window in that case, which
    // is the last thing we want when the user is just clicking a shortcut.
    if (enable == m_HintOverridden) {
        return;
    }

    if (enable) {
        const char* old = SDL_GetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS);
        m_SavedHint = old != nullptr ? SDL_strdup(old) : nullptr;
        SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    }
    else {
        SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, m_SavedHint);
        SDL_free(m_SavedHint);
        m_SavedHint = nullptr;
    }
    m_HintOverridden = enable;
}

void ShortcutMenu::updatePosition()
{
    if (m_Window == nullptr) {
        return;
    }

    Uint32 parentFlags = SDL_GetWindowFlags(m_Parent);
    if (parentFlags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) {
        if (m_Shown) {
            SDL_HideWindow(m_Window);
            m_Shown = false;
        }
        return;
    }

    int px, py, pw, ph, w, h;
    SDL_GetWindowPosition(m_Parent, &px, &py);
    SDL_GetWindowSize(m_Parent, &pw, &ph);
    SDL_GetWindowSize(m_Window, &w, &h);

    int x, y;
    if (m_HasCustomPosition) {
        x = px + m_OffsetX;
        y = py + m_OffsetY;
    }
    else {
        x = px + (pw - w) / 2;
        y = py + kDefaultTopMargin;
    }

    // Keep the whole menu inside the stream window so it can't get lost off-screen
    // when the stream window is resized or moved to another display.
    x = qMax(px, qMin(x, px + pw - w));
    y = qMax(py, qMin(y, py + ph - h));

    int cx, cy;
    SDL_GetWindowPosition(m_Window, &cx, &cy);
    if (cx != x || cy != y) {
        SDL_SetWindowPosition(m_Window, x, y);
    }

    if (!m_Shown) {
        showWithoutActivation();
        m_Shown = true;

        // Make sure the stream window keeps focus after we appear
        SDL_RaiseWindow(m_Parent);
        render();
    }

#if SDL_VERSION_ATLEAST(2, 0, 16)
    // Re-stack above the (possibly topmost, full-screen) stream window without activating
    SDL_SetWindowAlwaysOnTop(m_Window, SDL_TRUE);
#endif
}

void ShortcutMenu::setExpanded(bool expanded)
{
    m_Expanded = expanded;
    m_HoverIndex = -1;
    m_ConfirmIndex = -1;

    if (expanded) {
        SDL_SetWindowSize(m_Window, m_ExpandedWidth, m_ExpandedHeight);
    }
    else {
        SDL_SetWindowSize(m_Window, kCollapsedWidth, kHeaderHeight);
    }

    updatePosition();
    render();
}

void ShortcutMenu::close(bool refocusStream)
{
    if (m_Expanded) {
        setExpanded(false);
    }

    if (!m_HeaderHover) {
        overrideMinimizeOnFocusLoss(false);
    }

    if (refocusStream) {
        SDL_RaiseWindow(m_Parent);
    }

    if (m_RecaptureOnClose) {
        m_RecaptureOnClose = false;
        m_Input->setCaptureActive(true);
    }
}

void ShortcutMenu::toggleFromHotkey()
{
    if (m_Window == nullptr) {
        return;
    }

    if (m_Expanded) {
        close(true);
        return;
    }

    // Free the mouse so the user can actually reach the menu
    m_RecaptureOnClose = m_Input->isCaptureActive();
    if (m_RecaptureOnClose) {
        m_Input->setCaptureActive(false);
    }

    overrideMinimizeOnFocusLoss(true);
    setExpanded(true);
    SDL_RaiseWindow(m_Window);
}

void ShortcutMenu::execute(const Item& itemRef)
{
    // Copy what we need, since close() may trigger changes
    Item item = itemRef;

    // Give focus (and mouse capture, if we took it) back before acting so that
    // the action sees the same state the user had.
    close(true);

    if (item.clientCombo >= 0) {
        m_Input->performSpecialKeyCombo((SdlInputHandler::KeyCombo)item.clientCombo);
    }
    else {
        m_Input->sendHostKeyCombo(item.keys.data(), (int)item.keys.size());
    }
}

int ShortcutMenu::hitTest(int x, int y) const
{
    if (!m_Expanded) {
        return -1;
    }

    for (size_t i = 0; i < m_Items.size(); i++) {
        if (pointInRect(m_Items[i].rect, x, y)) {
            return (int)i;
        }
    }
    return -1;
}

void ShortcutMenu::handleClick(int x, int y)
{
    if (y < kHeaderHeight) {
        if (m_Expanded) {
            close(true);
        }
        else {
            m_RecaptureOnClose = false;
            setExpanded(true);
        }
        return;
    }

    int index = hitTest(x, y);
    if (index < 0) {
        return;
    }

    const Item& item = m_Items[index];
    if (item.dangerous && m_ConfirmIndex != index) {
        // Require a second click for destructive actions
        m_ConfirmIndex = index;
        render();
        return;
    }

    execute(item);
}

void ShortcutMenu::beginDrag(int localX, int localY)
{
    m_MouseDown = true;
    m_Dragging = false;
    m_GrabLocalX = localX;
    m_GrabLocalY = localY;
    SDL_GetGlobalMouseState(&m_DownGlobalX, &m_DownGlobalY);

    // Keep receiving motion/button events even if the pointer leaves our small window
    SDL_CaptureMouse(SDL_TRUE);
}

void ShortcutMenu::updateDrag()
{
    int gx, gy;
    SDL_GetGlobalMouseState(&gx, &gy);

    if (!m_Dragging) {
        if (qAbs(gx - m_DownGlobalX) < kDragThresholdPx && qAbs(gy - m_DownGlobalY) < kDragThresholdPx) {
            return;
        }
        m_Dragging = true;
    }

    int px, py;
    SDL_GetWindowPosition(m_Parent, &px, &py);

    m_HasCustomPosition = true;
    m_OffsetX = gx - m_GrabLocalX - px;
    m_OffsetY = gy - m_GrabLocalY - py;
    updatePosition();
}

void ShortcutMenu::endDrag()
{
    if (m_MouseDown) {
        SDL_CaptureMouse(SDL_FALSE);
    }
    m_MouseDown = false;
    m_Dragging = false;
}

bool ShortcutMenu::handleEvent(const SDL_Event& event)
{
    if (m_Window == nullptr) {
        return false;
    }

    const Uint32 id = SDL_GetWindowID(m_Window);

    switch (event.type) {
    case SDL_WINDOWEVENT:
        if (event.window.windowID != id) {
            return false;
        }

        switch (event.window.event) {
        case SDL_WINDOWEVENT_EXPOSED:
        case SDL_WINDOWEVENT_SHOWN:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            render();
            break;
        case SDL_WINDOWEVENT_ENTER:
            overrideMinimizeOnFocusLoss(true);
            break;
        case SDL_WINDOWEVENT_LEAVE:
            m_HoverIndex = -1;
            m_HeaderHover = false;
            if (!m_Expanded && !m_MouseDown) {
                overrideMinimizeOnFocusLoss(false);
            }
            render();
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            // Clicking somewhere else dismisses the dropdown (but the button stays)
            if (m_Expanded && !m_MouseDown) {
                m_RecaptureOnClose = false;
                setExpanded(false);
                if (!m_HeaderHover) {
                    overrideMinimizeOnFocusLoss(false);
                }
            }
            break;
        }
        return true;

    case SDL_MOUSEMOTION:
        if (event.motion.windowID != id) {
            return false;
        }

        if (m_MouseDown) {
            updateDrag();
        }
        else {
            int hover = hitTest(event.motion.x, event.motion.y);
            bool headerHover = event.motion.y < kHeaderHeight;
            if (hover != m_HoverIndex || headerHover != m_HeaderHover) {
                m_HoverIndex = hover;
                m_HeaderHover = headerHover;
                render();
            }
        }
        return true;

    case SDL_MOUSEBUTTONDOWN:
        if (event.button.windowID != id) {
            return false;
        }
        if (event.button.button == SDL_BUTTON_LEFT) {
            if (event.button.y < kHeaderHeight) {
                // Might become a drag or a click; decided on button up
                beginDrag(event.button.x, event.button.y);
            }
            else {
                handleClick(event.button.x, event.button.y);
            }
        }
        return true;

    case SDL_MOUSEBUTTONUP:
        if (event.button.windowID != id) {
            return false;
        }
        if (event.button.button == SDL_BUTTON_LEFT && m_MouseDown) {
            bool wasDragging = m_Dragging;
            endDrag();
            if (!wasDragging) {
                handleClick(event.button.x, event.button.y);
            }
        }
        return true;

    case SDL_MOUSEWHEEL:
        return event.wheel.windowID == id;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if (event.key.windowID != id) {
            return false;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && m_Expanded) {
            close(true);
        }
        return true;

    default:
        return false;
    }
}

void ShortcutMenu::drawText(SDL_Surface* dst, TTF_Font* font, const char* text, SDL_Color color,
                            int x, int y, bool rightAlign)
{
    if (text == nullptr || text[0] == 0) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (surface == nullptr) {
        return;
    }

    SDL_Rect dstRect = { rightAlign ? x - surface->w : x, y, surface->w, surface->h };
    SDL_BlitSurface(surface, nullptr, dst, &dstRect);
    SDL_FreeSurface(surface);
}

void ShortcutMenu::drawChevron(SDL_Surface* dst, int cx, int cy, bool up, Uint32 color)
{
    for (int i = 0; i < 5; i++) {
        int half = up ? i : 4 - i;
        SDL_Rect row = { cx - half, cy - 2 + i, half * 2 + 1, 1 };
        SDL_FillRect(dst, &row, color);
    }
}

void ShortcutMenu::render()
{
    if (m_Window == nullptr || !m_Shown) {
        return;
    }

    SDL_Surface* s = SDL_GetWindowSurface(m_Window);
    if (s == nullptr) {
        return;
    }

    Uint32 border = SDL_MapRGB(s->format, 0x5A, 0x5E, 0x70);
    Uint32 background = SDL_MapRGB(s->format, 0x1E, 0x1F, 0x26);
    Uint32 headerColor = SDL_MapRGB(s->format, m_HeaderHover ? 0x3A : 0x2A, m_HeaderHover ? 0x5A : 0x3C, m_HeaderHover ? 0xA0 : 0x78);
    Uint32 hoverColor = SDL_MapRGB(s->format, 0x3A, 0x5A, 0xA0);
    Uint32 white = SDL_MapRGB(s->format, 0xFF, 0xFF, 0xFF);

    SDL_FillRect(s, nullptr, border);
    SDL_Rect inner = { 1, 1, s->w - 2, s->h - 2 };
    SDL_FillRect(s, &inner, background);

    // Header / button (also the drag handle)
    SDL_Rect header = { 1, 1, s->w - 2, kHeaderHeight - 2 };
    SDL_FillRect(s, &header, headerColor);
    drawText(s, m_Font, "Shortcuts", kTextColor, 12, (kHeaderHeight - TTF_FontHeight(m_Font)) / 2);
    drawChevron(s, s->w - 16, kHeaderHeight / 2, m_Expanded, white);

    if (m_Expanded) {
        for (int c = 0; c < 2; c++) {
            drawText(s, m_SmallFont, m_ColumnTitles[c], kTitleColor,
                     kPadding + c * (kColumnWidth + kPadding) + 4,
                     kHeaderHeight + kPadding + (kTitleHeight - TTF_FontHeight(m_SmallFont)) / 2);
        }

        for (size_t i = 0; i < m_Items.size(); i++) {
            const Item& item = m_Items[i];

            if ((int)i == m_HoverIndex) {
                SDL_FillRect(s, const_cast<SDL_Rect*>(&item.rect), hoverColor);
            }

            bool confirming = ((int)i == m_ConfirmIndex);
            SDL_Color color = item.dangerous ? kDangerColor : kTextColor;
            const char* label = confirming ? "Click again to confirm" : item.label;

            drawText(s, m_Font, label, color,
                     item.rect.x + 6, item.rect.y + (item.rect.h - TTF_FontHeight(m_Font)) / 2);
            if (!confirming) {
                drawText(s, m_SmallFont, item.hint, kHintColor,
                         item.rect.x + item.rect.w - 6,
                         item.rect.y + (item.rect.h - TTF_FontHeight(m_SmallFont)) / 2, true);
            }
        }
    }

    SDL_UpdateWindowSurface(m_Window);
}
