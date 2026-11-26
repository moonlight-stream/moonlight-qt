#pragma once

#include <SDL_syswm.h>
#include "captionmenu.h"
#include "displaywindows.h"
#include "windowcaption.h"
#include "input/input.h"
#include <commctrl.h>
#include <QCoreApplication>
#include <QPainter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QThreadPool>
#include <array>
#include <functional>
#include <future>
#include <optional>
#include <utility>

// The SDL loop owns these non-activating windows. Menu input never
// starts a nested message loop or blocks the streaming thread.
class CaptionControls {
public:
    static constexpr int Fullscreen = 111;
    static constexpr int FileTransfer = 112;
    static constexpr int PrivacyReconnect = 114;
    static constexpr int Disconnect = 115;
    static constexpr int GameMouse = 116;
    static constexpr int DesktopMouse = 117;
    static constexpr int DisplayFramePresented = 118;
    static constexpr int RedrawDisplay = 119;
    using BitrateRequest = std::function<std::future<bool>(int)>;
    using DisplayRequest = std::function<std::future<QJsonObject>(QString, QString)>;
    using DisconnectLockRequest = std::function<std::future<std::optional<bool>>(bool)>;
    enum Field { Duration, Fps, Network, Decode, Upload, Download, Loss, Resolution, FieldCount };
    CaptionControls(SDL_Window* window, StreamingPreferences& preferences, SdlInputHandler& input,
                    int bitrateKbps, int initialBitrateKbps,
                    std::function<std::future<QString>()> lockHost,
                    std::function<std::future<std::optional<bool>>(std::optional<bool>)> hostMute,
                    bool hostMuteAvailable, const QString& hostUuid, bool hostPrivacyAvailable,
                    bool disconnectLockEnabled, DisconnectLockRequest disconnectLock, DisplayRequest displayRequest,
                    DisplayWindows::Launch launchDisplay, BitrateRequest bitrateRequest, std::function<void(bool)> requestFrame)
        : m_Window(window)
        , m_Preferences(preferences)
        , m_Input(input)
        , m_BitrateKbps(bitrateKbps)
        , m_InitialBitrateKbps(initialBitrateKbps)
        , m_RequestBitrate(std::move(bitrateRequest))
        , m_LockHost(std::move(lockHost))
        , m_RequestHostMute(std::move(hostMute))
        , m_HostMuteAvailable(hostMuteAvailable)
        , m_HostUuid(hostUuid)
        , m_HostPrivacyAvailable(hostPrivacyAvailable)
        , m_RequestDisconnectLock(std::move(disconnectLock))
        , m_LockOnDisconnect(disconnectLockEnabled)
        , m_DisplayRequest(std::move(displayRequest))
        , m_DisplayWindows(std::move(launchDisplay), std::move(requestFrame))
    {
        if (!m_DisplayWindows.output().isEmpty()) {
            // A prewarmed stream can become visible before its first host poll completes.
            m_Outputs = QJsonDocument::fromJson(qEnvironmentVariable("MOONLIGHT_DISPLAY_OUTPUTS").toUtf8()).array();
            m_IndependentOutputs = !m_Outputs.isEmpty();
            m_ActiveOutput = m_DisplayWindows.output();
        }
        QSettings settings;
        m_Fields = settings.value("streamHud/fields", 0xff).toInt() & 0xff;
        if (!m_Fields)
            m_Fields = 0xff;
        m_Corner = qBound(0, settings.value("streamHud/corner", 0).toInt(), 3);
        m_Opacity = qBound(1, settings.value("streamHud/opacity", 3).toInt(), 4);
        m_Values.fill(QStringLiteral("—"));
    }

    void framePresented() { m_DisplayWindows.framePresented(); }
    void endDisplayGroup(bool disconnectOnly = false) { m_DisplayWindows.endGroup(disconnectOnly); }
    int bitrateKbps() const { return m_BitrateKbps; }
    bool groupHasFocus() const { return m_DisplayWindows.hasFocus(); }
    HWND groupedWindow() const { return m_DisplayWindows.groupedWindow(); }

    ~CaptionControls() { close(); }
    void close()
    {
        cancelBitrateDrag();
        m_DisplayWindows.close();
        DisplayWindows::setTabBar(m_Owner, nullptr);
        hideDragPreview();
        m_ConnectingTab.clear();
        m_PressedOutput.clear();
        m_DraggingTab = false;
        m_CaptionMoveStart.reset();
        if (m_Owner)
            RemoveWindowSubclass(m_Owner, ownerProc, reinterpret_cast<UINT_PTR>(this));
        for (HWND* window : { &m_Menu, &m_Hud, &m_Button, &m_Tabs, &m_DragPreview, &m_DockTip, &m_Dock }) {
            if (*window && IsWindow(*window))
                DestroyWindow(*window);
            *window = nullptr;
        }
        m_Owner = nullptr;
        m_PlacedCount = 0;
        m_Open = false;
        m_Main.frame = m_Sub.frame = {};
        m_MenuSurface.reset();
        m_MenuBounds = {};
        m_MenuDamage = {};
        m_MenuLimits = {};
        m_PressedPanel = nullptr;
        m_MenuKeys.clear();
        m_ButtonHover = m_DockHover = false;
        m_DockCornerRadius = -1;
        m_HostMuteState.reset();
        m_HostMuteToggleAfterRead = false;
    }

    qreal windowScale() const { return m_Scale; }
    bool enabled() const { return m_Preferences.showPerformanceOverlay; }
    void toggle()
    {
        m_Preferences.showPerformanceOverlay = !enabled();
        QSettings().setValue("showperfoverlay", enabled());
        update();
    }

    void setValues(const std::array<QString, FieldCount>& values)
    {
        if (m_Values[Resolution] != values[Resolution])
            m_MenuContentChanged = true;
        m_Values = values;
    }
    void requestLock()
    {
        if (m_Input.isViewOnly() || m_LockResult.valid()) return;
        m_LockResult = m_LockHost();
        if (!m_LockResult.valid()) return;
        m_LockStatus = tr("Requesting host lock…");
        SetTimer(m_Menu, LockStatusTimer, 100, nullptr);
        if (m_Open) update();
    }

    void dismiss()
    {
        if (!m_Open) return;
        if (m_Button)
            InvalidateRect(m_Button, nullptr, FALSE);
        cancelBitrateDrag();
        m_Open = false;
        m_Page = Page::Main;
        m_Main.hover = m_Sub.hover = -1;
        m_Main.scroll = m_Sub.scroll = 0;
        m_PressedPanel = nullptr;
        m_HostMuteToggleAfterRead = false;
        if (m_Menu) {
            KillTimer(m_Menu, HostMuteRefreshTimer);
            ShowWindow(m_Menu, SW_HIDE);
        }
    }

    void update(bool refreshMenu = true)
    {
        if (m_BitrateResult.valid() && m_BitrateResult.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const bool applied = m_BitrateResult.get();
            if (applied)
                m_BitrateKbps = m_PreviewKbps;
            m_QualityStatus = applied ? QString{} : tr("The host could not confirm the bitrate change.");
            refreshMenu = true;
        }
        m_MenuContentChanged |= refreshMenu;
        SDL_SysWMinfo info { };
        SDL_VERSION(&info.version);
        if (!SDL_GetWindowWMInfo(m_Window, &info)) {
            close();
            return;
        }
        HWND owner = info.info.win.window;
        if (m_Owner != owner || !IsWindow(m_Button)) {
            close();
            m_Owner = owner;
            m_DisplayWindows.bind(owner, m_DisplayWindows.output());
            m_UsesLightCaption = applyLightWindowCaption(owner);
            m_Button = create(0);
            // Keep the popup on its own composited surface above the streaming video.
            m_Menu = create(WS_EX_LAYERED, L"MoonlightControlCenterMenu", WS_POPUP, CS_DROPSHADOW);
            if (m_DisplayRequest) {
                m_Tabs = create(0);
                DisplayWindows::setTabBar(owner, m_Tabs);
                m_DragPreview = create(0, L"MoonlightDisplayDragPreview", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME);
                if (m_DragPreview) {
                    applyLightWindowCaption(m_DragPreview);
                    const BOOL disableTransitions = TRUE;
                    DwmSetWindowAttribute(m_DragPreview, DWMWA_TRANSITIONS_FORCEDISABLED,
                                          &disableTransitions, sizeof(disableTransitions));
                }
                m_Dock = create(0);
                if (m_Dock) {
                    m_DockLabel = tr("Return to tabs").toStdWString();
                    SetWindowTextW(m_Dock, m_DockLabel.c_str());
                    m_DockTip = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, TOOLTIPS_CLASSW, nullptr,
                        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0, m_Dock, nullptr, GetModuleHandleW(nullptr), nullptr);
                    TOOLINFOW tool{};
                    tool.cbSize = sizeof(tool);
                    tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
                    tool.hwnd = m_Dock;
                    tool.uId = reinterpret_cast<UINT_PTR>(m_Dock);
                    tool.lpszText = m_DockLabel.data();
                    SendMessageW(m_DockTip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
                }
                sendDisplayRequest({}, {});
                SetTimer(m_Menu, DisplayTimer, 30, nullptr);
            }
            if (m_LockResult.valid()) SetTimer(m_Menu, LockStatusTimer, 100, nullptr);
            if (m_HostMuteResult.valid()) SetTimer(m_Menu, HostMuteResultTimer, 75, nullptr);
            if (m_DisconnectLockResult.valid()) SetTimer(m_Menu, DisconnectLockTimer, 75, nullptr);
            m_Hud = create(WS_EX_LAYERED | WS_EX_TRANSPARENT);
            SetWindowSubclass(owner, ownerProc, reinterpret_cast<UINT_PTR>(this), reinterpret_cast<DWORD_PTR>(this));
        }
        layout(owner);
    }

    bool handleMenuKey(const SDL_KeyboardEvent& key)
    {
        if (key.type == SDL_KEYUP)
            return m_MenuKeys.remove(key.keysym.scancode);
        // Focus changes can lose the key-up. A fresh press belongs to the new context.
        if (!key.repeat)
            m_MenuKeys.remove(key.keysym.scancode);
        else if (!m_MenuKeys.contains(key.keysym.scancode))
            return false;
        if (!m_Open || (key.keysym.mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)))
            return false;
        auto& panel = m_Page == Page::Main ? m_Main : m_Sub;
        if ((key.keysym.sym == SDLK_LEFT || key.keysym.sym == SDLK_RIGHT) &&
            !m_DraggingBitrate && panel.hover >= 0 && panel.hover < panel.rows.size()) {
            const auto& row = panel.rows[panel.hover];
            if (row.action == Action::BitrateSlider && row.enabled) {
                setBitrate(qBound(Row::SliderMin, m_BitrateKbps + (key.keysym.sym == SDLK_RIGHT ? 500 : -500), row.sliderMaximum));
                m_MenuKeys.insert(key.keysym.scancode);
                return true;
            }
        }
        switch (key.keysym.sym) {
        case SDLK_UP:
        case SDLK_DOWN: {
            const int previousScroll = panel.scroll;
            panel.moveSelection(key.keysym.sym == SDLK_UP ? -1 : 1);
            if (panel.scroll != previousScroll) update(false);
            else queueMenuPaint(panel.frame);
            break;
        }
        case SDLK_LEFT:
            if (!key.repeat) showPage(parent(m_Page));
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
        case SDLK_RIGHT:
            if (!key.repeat && panel.hover >= 0 && panel.hover < panel.rows.size()) {
                const auto row = panel.rows[panel.hover];
                if (key.keysym.sym != SDLK_RIGHT || row.action == Action::Submenu)
                    select(row);
            }
            break;
        case SDLK_ESCAPE:
            dismiss();
            break;
        default:
            return false;
        }
        m_MenuKeys.insert(key.keysym.scancode);
        return true;
    }

private:
    using Page = CaptionMenu::Page;
    using Action = CaptionMenu::Action;
    using Icon = CaptionMenu::Icon;
    using Panel = CaptionMenu::Panel;
    using Row = CaptionMenu::Row;
    // Native move/size loops dispatch window messages while the SDL event loop is blocked.
    static LRESULT CALLBACK ownerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
    {
        auto* self = reinterpret_cast<CaptionControls*>(data);
        if (msg == DisplayWindows::DragMoveFenceMessage) {
            self->m_DisplayWindows.acknowledgeDragMove(reinterpret_cast<HWND>(lp), wp);
            return 0;
        }
        if (msg == DisplayWindows::DragMoveAppliedMessage) {
            self->m_DisplayWindows.dragMoveApplied(wp);
            return 0;
        }
        if (msg == DisplayWindows::LayoutMessage) {
            self->layout(hwnd);
            self->m_DisplayWindows.dragWindowPositioned();
            return 0;
        }
        if (msg == WM_CLOSE || (msg == WM_SYSCOMMAND && (wp & 0xfff0) == SC_CLOSE)) {
            if (self->m_DisplayWindows.canDock()) {
                self->m_DisplayWindows.dock();
                return 0;
            }
            self->endDisplayGroup();
        }
        if (msg == DisplayWindows::DisconnectMessage) {
            SDL_Event event{};
            event.type = SDL_USEREVENT;
            event.user.code = Disconnect;
            SDL_PushEvent(&event);
            return 0;
        }
        if (msg == DisplayWindows::ReadyMessage) {
            self->pollDisplayWindows();
            return 0;
        }
        if (msg == DisplayWindows::SwitchedMessage) {
            self->m_DisplayWindows.switched(reinterpret_cast<HWND>(wp));
            self->pollDisplayWindows();
            return 0;
        }
        if (msg == DisplayWindows::SwitchMessage) {
            self->m_DisplayWindows.prepareSwitch(reinterpret_cast<HWND>(wp));
            return 0;
        }
        if (msg == WM_NCDESTROY)
            RemoveWindowSubclass(hwnd, ownerProc, id);
        if (msg == WM_ENTERSIZEMOVE) {
            RECT start{};
            self->m_CaptionMoved = false;
            self->m_CaptionMoveStart.reset();
            if (self->m_DisplayWindows.canDock() && primaryButtonDown() && GetWindowRect(hwnd, &start))
                self->m_CaptionMoveStart = start;
        } else if (msg == WM_MOVING) {
            self->m_CaptionMoved = true;
        } else if (msg == WM_CANCELMODE || (msg == WM_CAPTURECHANGED && lp)) {
            self->m_CaptionMoveStart.reset();
        }
        if ((msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE) || msg == WM_NCLBUTTONDOWN)
            self->dismiss();
        if (msg == WM_MOUSEWHEEL && self->menuWheel(wp, lp)) return 0;
        LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
        if (msg == WM_NCHITTEST && result == HTCLIENT && !IsZoomed(hwnd) &&
            (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_THICKFRAME) &&
            !(SDL_GetWindowFlags(self->m_Window) & SDL_WINDOW_FULLSCREEN) &&
            !SDL_GetRelativeMouseMode() && !SDL_GetWindowMouseRect(self->m_Window) && !GetCapture())
            return streamClientEdge(hwnd, lp);
        if (msg == WM_EXITSIZEMOVE && self->m_CaptionMoveStart) {
            const auto start = *std::exchange(self->m_CaptionMoveStart, std::nullopt);
            RECT end{};
            POINT cursor{};
            // A canceled native move restores the original bounds. Resizing
            // and keyboard moves also finish with WM_EXITSIZEMOVE.
            if (self->m_CaptionMoved && !primaryButtonDown() && !(GetAsyncKeyState(VK_ESCAPE) & 0x8000) &&
                GetWindowRect(hwnd, &end) && (start.left != end.left || start.top != end.top) && GetCursorPos(&cursor))
                self->m_DisplayWindows.finishCaptionMove(cursor);
        }
        if (self->m_Dock && (msg == WM_NCACTIVATE || msg == WM_SYSCOLORCHANGE))
            InvalidateRect(self->m_Dock, nullptr, FALSE);
        if (msg == WM_WINDOWPOSCHANGED) {
            const auto* pos = reinterpret_cast<const WINDOWPOS*>(lp);
            if ((pos->flags & (SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_HIDEWINDOW | SWP_FRAMECHANGED)) == SWP_NOSIZE &&
                self->moveCaption(hwnd, pos->x, pos->y))
                return result;
            if ((pos->flags & (SWP_NOMOVE | SWP_NOSIZE)) != (SWP_NOMOVE | SWP_NOSIZE) ||
                (pos->flags & (SWP_SHOWWINDOW | SWP_HIDEWINDOW | SWP_FRAMECHANGED)))
                self->layout(hwnd);
        }
        return result;
    }

    static bool primaryButtonDown()
    {
        return (GetAsyncKeyState(GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON) & 0x8000) != 0;
    }

    bool moveCaption(HWND owner, int x, int y)
    {
        if (!m_PlacedCount || m_Open || IsIconic(owner) || IsZoomed(owner) ||
            MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST) != m_LayoutMonitor ||
            windowDpi(owner) != m_LayoutDpi) return false;
        // The caption geometry is unchanged on an ordinary move. Let DWM
        // translate the existing popup surfaces together with no repaint.
        HDWP batch = BeginDeferWindowPos(m_PlacedCount);
        for (int i = 0; batch && i < m_PlacedCount; ++i) {
            const auto& control = m_PlacedControls[i];
            batch = DeferWindowPos(batch, control.window, nullptr, x + control.offset.x, y + control.offset.y,
                                   0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        }
        return batch && EndDeferWindowPos(batch);
    }

    void layout(HWND owner)
    {
        UINT flags = SDL_GetWindowFlags(m_Window);
        RECT bounds { }, client { }, win { };
        POINT origin { };
        GetClientRect(owner, &client);
        ClientToScreen(owner, &origin);
        GetWindowRect(owner, &win);
        m_LayoutMonitor = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
        m_LayoutDpi = windowDpi(owner);
        m_Scale = m_LayoutDpi / 96.0;
        const bool visible = IsWindowVisible(owner) && !IsIconic(owner);
        const bool caption = visible && !(flags & SDL_WINDOW_FULLSCREEN)
            && SUCCEEDED(DwmGetWindowAttribute(owner, DWMWA_CAPTION_BUTTON_BOUNDS, &bounds, sizeof(bounds)))
            && !IsRectEmpty(&bounds);
        const int right = win.left + bounds.left - px(3);
        int top = win.top + bounds.top;
        const int bottom = qMin(int(origin.y), int(win.top + bounds.bottom));
        // A maximized HWND extends beyond the visible frame by the resize border.
        RECT frame{};
        if (caption && IsZoomed(owner) && SUCCEEDED(DwmGetWindowAttribute(owner, DWMWA_EXTENDED_FRAME_BOUNDS,
                                                                         &frame, sizeof(frame))))
            top = qMax(top, int(frame.top));
        if (m_Dock) {
            if (caption && m_DisplayWindows.canDock()) {
                const int width = (bounds.right - bounds.left) / 3;
                layoutDock(owner, {win.left + bounds.right - width, top, win.left + bounds.right, bottom});
            } else {
                ShowWindow(m_Dock, SW_HIDE);
            }
        }
        if (caption && bounds.left > px(128)) {
            const int buttonWidth = px(36 + QFontMetrics(CaptionMenu::font(13)).horizontalAdvance(tr("Control center")));
            const int buttonHeight = qMin(px(28), bottom - top - 2 * px(2));
            place(m_Button, right - buttonWidth, (top + bottom - buttonHeight) / 2, buttonWidth, buttonHeight);
            if (m_Tabs) {
                const int left = win.left + px(48);
                const int space = right - buttonWidth - px(8) - left;
                const int count = m_Outputs.size();
                if (canSelectDisplay() && space >= count * px(64)) {
                    const int width = qMin(space, count * px(164));
                    place(m_Tabs, right - buttonWidth - px(8) - width, (top + bottom - buttonHeight) / 2, width, buttonHeight);
                } else {
                    ShowWindow(m_Tabs, SW_HIDE);
                }
            }
            if (m_Open) {
                if (GetForegroundWindow() != owner)
                    dismiss();
                else
                    layoutMenu(right, origin.y + px(2));
            }
        } else {
            if (m_Tabs) ShowWindow(m_Tabs, SW_HIDE);
            ShowWindow(m_Button, SW_HIDE);
            dismiss();
        }
        if (visible && enabled()) {
            int count = 0;
            for (int i = 0; i < FieldCount; ++i)
                if (m_Fields & (1 << i))
                    ++count;
            int w = px(200), h = px(12 + count * 23);
            int x = (m_Corner & 1) ? client.right - w - px(12) : px(12);
            int y = (m_Corner & 2) ? client.bottom - h - px(12) : px(12);
            SetLayeredWindowAttributes(m_Hud, 0, BYTE(255 * m_Opacity / 4), LWA_ALPHA);
            place(m_Hud, origin.x + qMax(0, x), origin.y + qMax(0, y), w, h);
            if (m_Hud) InvalidateRect(m_Hud, nullptr, FALSE);
        } else
            ShowWindow(m_Hud, SW_HIDE);

        m_PlacedCount = 0;
        for (HWND control : {m_Button, m_Tabs, m_Dock, m_Hud}) {
            RECT rect{};
            if (IsWindowVisible(control) && GetWindowRect(control, &rect))
                m_PlacedControls[m_PlacedCount++] = {control, {rect.left - win.left, rect.top - win.top}};
        }
    }

    void layoutDock(HWND owner, const RECT& fallback)
    {
        TITLEBARINFOEX info{};
        info.cbSize = sizeof(info);
        SendMessageW(owner, WM_GETTITLEBARINFOEX, 0, reinterpret_cast<LPARAM>(&info));
        const RECT& slot = IsRectEmpty(&info.rgrect[5]) ? fallback : info.rgrect[5];
        RECT button = slot;
        button.top = qMax(button.top, fallback.top);
        button.bottom = qMin(button.bottom, fallback.bottom);
        RECT frame{};
        if (SUCCEEDED(DwmGetWindowAttribute(owner, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame)))) {
            UINT border = 0;
            if (!IsZoomed(owner))
                DwmGetWindowAttribute(owner, DWMWA_VISIBLE_FRAME_BORDER_THICKNESS, &border, sizeof(border));
            // The close button bounds can include a row of the visible DWM border at higher DPI.
            frame.top += LONG(border);
            frame.right -= LONG(border);
            IntersectRect(&button, &button, &frame);
        }
        if (IsRectEmpty(&button)) {
            ShowWindow(m_Dock, SW_HIDE);
            return;
        }

        // The native caption glyphs sit one physical pixel above the middle of the slot.
        const QPointF center((slot.left + slot.right) / 2.0 - button.left,
                             slot.top + (slot.bottom - slot.top) / 2 - 1 - button.top);
        if (m_DockGlyphCenter != center) {
            m_DockGlyphCenter = center;
            InvalidateRect(m_Dock, nullptr, FALSE);
        }
        place(m_Dock, button.left, button.top, button.right - button.left, button.bottom - button.top);
    }

    static QString tr(const char* s) { return QCoreApplication::translate("StreamControlCenter", s); }
    static QString settingsText(const char* s) { return QCoreApplication::translate("SettingsView", s); }
    int px(int v) const { return qRound(v * m_Scale); }
    QStringList labels() const
    {
        return { tr("Duration"), tr("Frame rate"), tr("Network RTT"), tr("Frame latency"), tr("Adapter upload"),
            tr("Adapter download"), tr("Network frame loss"), tr("Resolution") };
    }

    static Page parent(Page page)
    {
        return page == Page::PrivacyResolution || page == Page::Fields || page == Page::Corner || page == Page::Opacity ? Page::Display : Page::Main;
    }
    static Page category(Page page) { return parent(page) == Page::Display ? Page::Display : page; }

    QString pageTitle(Page page) const
    {
        switch (page) {
        case Page::Display: {
            for (int i = 0; i < m_Outputs.size(); ++i) {
                if (!m_ActiveOutput.isEmpty() && outputId(i) == m_ActiveOutput)
                    return tr("Display %1").arg(i + 1);
            }
            return tr("Display");
        }
        case Page::Quality: return tr("Video quality");
        case Page::Sound: return tr("Sound");
        case Page::Security: return tr("Security");
        case Page::MouseMode: return tr("Mouse capture mode");
        case Page::SystemKeys: return tr("System shortcuts");
        case Page::PrivacyResolution: return tr("Privacy screen resolution");
        case Page::Fields: return tr("Displayed metrics");
        case Page::Corner: return tr("Position");
        case Page::Opacity: return tr("Opacity");
        case Page::Main: return tr("Control center");
        }
        return {};
    }

    void populate(Panel& panel, Page page)
    {
        panel.rows.clear();
        panel.footer.clear();
        if (page != Page::Main && (m_Inline || parent(page) != Page::Main)) {
            panel.add(Action::Back, pageTitle(page));
            panel.add(Action::Divider);
        }
        switch (page) {
        case Page::Main:
            panel.submenu(Page::Display, pageTitle(Page::Display), Icon::Display).detail = m_Values[Resolution];
            panel.submenu(Page::Quality, pageTitle(Page::Quality), Icon::Quality);
            if (!m_Input.isViewOnly()) {
                panel.add(Action::Divider);
                panel.submenu(Page::Sound, pageTitle(Page::Sound), Icon::Sound);
                panel.submenu(Page::Security, pageTitle(Page::Security), Icon::Security);
                panel.submenu(Page::MouseMode, pageTitle(Page::MouseMode), Icon::Mouse);
                panel.submenu(Page::SystemKeys, pageTitle(Page::SystemKeys), Icon::Keyboard);
                panel.add(Action::Files, tr("File transfer"), Icon::Files);
            }
            panel.add(Action::Divider);
            panel.add(Action::Fullscreen, tr("Fullscreen"), Icon::Fullscreen).detail = QStringLiteral("Ctrl+Alt+Shift+X");
            panel.add(Action::Divider);
            panel.add(Action::Disconnect, m_Input.isViewOnly() ? tr("End viewing") :
                      QCoreApplication::translate("Session", "Disconnect"), Icon::Disconnect);
            break;
        case Page::Display: {
            SDL_DisplayMode mode{};
            const int display = SDL_GetWindowDisplayIndex(m_Window);
            if (display >= 0 && SDL_GetDesktopDisplayMode(display, &mode) == 0)
                panel.add(Action::Info, tr("Local display")).detail = QStringLiteral("%1 × %2").arg(mode.w).arg(mode.h);
            panel.add(Action::Info, tr("Stream resolution")).detail = m_Values[Resolution];
            if (canSelectDisplay()) {
                for (int i = 0; i < m_Outputs.size(); ++i)
                    panel.choice(Action::SelectDisplay, displayLabel(i), i, outputId(i) == m_ActiveOutput);
            }
            if (m_PrivateDesktop && !m_Input.isViewOnly())
                panel.submenu(Page::PrivacyResolution, pageTitle(Page::PrivacyResolution));
            if (!m_DisplayStatus.isEmpty()) panel.footer = m_DisplayStatus;
            panel.add(Action::Divider);
            panel.add(Action::Hud, tr("Statistics HUD")).checked = enabled();
            for (Page child : {Page::Fields, Page::Corner, Page::Opacity})
                panel.submenu(child, pageTitle(child));
            break;
        }
        case Page::Quality: {
            bool includesStartingQuality = false;
            auto addPreset = [&](const QString& name, int bitrate) {
                auto& row = panel.add(Action::SetQuality, name);
                row.detail = QStringLiteral("%1 Mbps").arg(bitrate / 1000.0);
                row.value = bitrate;
                row.checked = bitrate == m_BitrateKbps;
                row.enabled = m_RequestBitrate && !m_BitrateResult.valid();
                includesStartingQuality |= bitrate == m_InitialBitrateKbps;
            };
            addPreset(tr("Clear"), 2000);
            addPreset(tr("HD"), 8000);
            addPreset(tr("Original"), 20000);
            if (!includesStartingQuality)
                addPreset(tr("Starting quality"), m_InitialBitrateKbps);
            auto& slider = panel.add(Action::BitrateSlider, tr("Bitrate"));
            slider.enabled = m_RequestBitrate && !m_BitrateResult.valid();
            slider.value = m_DraggingBitrate || m_BitrateResult.valid() ? m_PreviewKbps : m_BitrateKbps;
            slider.sliderMaximum = m_Preferences.unlockBitrate || m_InitialBitrateKbps > 150000 ? 500000 : 150000;
            slider.detail = QStringLiteral("%1 Mbps").arg(slider.value / 1000.0);
            if (!m_RequestBitrate)
                panel.footer = tr("Update Sunshine to change bitrate during streaming.");
            else if (m_BitrateResult.valid())
                panel.footer = tr("Applying bitrate...");
            else
                panel.footer = tr("Drag and release to apply.");
            if (!m_QualityStatus.isEmpty())
                panel.footer = m_QualityStatus;
            break;
        }
        case Page::PrivacyResolution: {
            const auto sizes = privacySizes();
            for (int i = 0; i < sizes.size(); ++i)
                panel.choice(Action::ResizePrivacy, sizes[i], i, sizes[i] == m_DesktopSize);
            panel.footer = m_DisplayStatus.isEmpty() ? tr("Changes the virtual desktop during this stream.") : m_DisplayStatus;
            break;
        }
        case Page::Fields: {
            const auto names = labels();
            for (int i = 0; i < names.size(); ++i)
                panel.choice(Action::Field, names[i], i, m_Fields & (1 << i));
            break;
        }
        case Page::Corner: {
            const QStringList names{tr("Top left"), tr("Top right"), tr("Bottom left"), tr("Bottom right")};
            for (int i = 0; i < names.size(); ++i)
                panel.choice(Action::Corner, names[i], i, m_Corner == i);
            break;
        }
        case Page::Opacity:
            for (int i = 1; i <= 4; ++i)
                panel.choice(Action::Opacity, QString::number(i * 25) + QLatin1Char('%'), i, m_Opacity == i);
            break;
        case Page::MouseMode:
            panel.choice(Action::MouseMode, tr("Game priority"), GameMouse, !m_Input.usesAbsoluteMouse());
            panel.choice(Action::MouseMode, tr("Desktop priority"), DesktopMouse, m_Input.usesAbsoluteMouse());
            panel.footer = QStringLiteral("Ctrl+Alt+Shift+M");
            break;
        case Page::SystemKeys: {
            const QStringList names{settingsText("Off"), settingsText("Fullscreen only"), settingsText("Always")};
            for (int i = 0; i < names.size(); ++i)
                panel.choice(Action::SystemKeys, names[i], i, m_Input.captureSystemKeysMode() == i);
            panel.footer = tr("Send system keyboard shortcuts to the host.");
            break;
        }
        case Page::Sound: {
            auto& mute = panel.add(Action::HostMute, tr("Host mute"));
            mute.checked = m_HostMuteState.value_or(false);
            mute.enabled = !m_HostMuteWriting;
            mute.detail = hostMuteStatus();
            break;
        }
        case Page::Security: {
            auto& lock = panel.add(Action::DisconnectLock, tr("Lock on disconnect"));
            lock.checked = m_LockOnDisconnect;
            lock.enabled = !m_DisconnectLockResult.valid();
            auto& privacy = panel.add(Action::Privacy, tr("Privacy screen (reconnect now)"));
            privacy.checked = m_PrivacyScreen;
            privacy.enabled = m_HostPrivacyAvailable && !m_DisconnectLockResult.valid();
            panel.add(Action::Divider);
            panel.add(Action::LockNow, tr("Request screen lock now")).enabled = !m_LockResult.valid();
            panel.footer = m_HostPrivacyAvailable
                    ? tr("Privacy screen reconnects using the virtual display configured in Sunshine.")
                    : tr("Update Sunshine to use the privacy screen from Moonlight.");
            panel.footer += QLatin1Char('\n') + (m_LockStatus.isEmpty() ? tr("Shortcut: Ctrl+Alt+Shift+End") : m_LockStatus);
            break;
        }
        }
    }

    void layoutMenu(int right, int top)
    {
        MONITORINFO monitor{sizeof(monitor)};
        if (!GetMonitorInfo(MonitorFromPoint({right - 1, top}, MONITOR_DEFAULTTONEAREST), &monitor)) {
            dismiss();
            return;
        }
        const RECT& work = monitor.rcWork;
        const int maxWidth = int((work.right - work.left) / m_Scale);
        const int maxHeight = int((work.bottom - work.top) / m_Scale);
        const int mainWidth = qMin(246, maxWidth), subWidth = qMin(288, maxWidth);
        const int x = qBound(int(work.left), right - px(mainWidth), int(work.right) - px(mainWidth));
        const int gap = px(2);
        const bool left = x - gap - px(subWidth) >= work.left;
        const bool inlinePage = !left && x + px(mainWidth) + gap + px(subWidth) > work.right;
        const QSize limits(maxWidth, maxHeight);
        const int display = SDL_GetWindowDisplayIndex(m_Window);
        const bool geometryChanged = limits != m_MenuLimits || m_MenuScale != m_Scale || m_MenuDisplay != display;
        const bool inlineChanged = m_Inline != inlinePage;
        if (inlineChanged) {
            m_Sub.hover = -1;
            m_PressedPanel = nullptr;
            m_Inline = inlinePage;
        }

        if (geometryChanged || m_MenuContentChanged) {
            populate(m_Main, Page::Main);
            m_Main.measure(mainWidth, maxHeight);
        }
        const bool refreshSub = m_MenuContentChanged || geometryChanged || inlineChanged || m_RenderedPage != m_Page;
        m_MenuContentChanged = false;
        m_MenuLimits = limits;
        m_MenuScale = m_Scale;
        m_MenuDisplay = display;
        const int y = qBound(int(work.top), top, int(work.bottom) - px(m_Main.size.height()));
        QRect main(x, y, px(mainWidth), px(m_Main.size.height())), sub;
        if (m_Page != Page::Main) {
            if (refreshSub) {
                populate(m_Sub, m_Page);
                m_Sub.measure(subWidth, maxHeight);
            }
            int subX = left ? x - gap - px(subWidth) : main.right() + 1 + gap;
            int subTop = y;
            if (m_Inline) {
                main = {};
                subX = qBound(int(work.left), right - px(subWidth), int(work.right) - px(subWidth));
            } else {
                for (const auto& row : m_Main.rows) {
                    if (row.action == Action::Submenu && row.target == category(m_Page)) {
                        subTop += px(row.top - Panel::Padding - m_Main.scroll);
                        break;
                    }
                }
            }
            const int subHeight = px(m_Sub.size.height());
            sub = QRect(subX, qBound(int(work.top), subTop, int(work.bottom) - subHeight), px(subWidth), subHeight);
        }

        const QRect popup = main.united(sub);
        if (!main.isEmpty()) main.translate(-popup.topLeft());
        if (!sub.isEmpty()) sub.translate(-popup.topLeft());
        const bool redraw = geometryChanged || refreshSub || main != m_Main.frame || sub != m_Sub.frame ||
                            popup != m_MenuBounds || m_Main.scroll != m_RenderedMainScroll ||
                            m_Sub.scroll != m_RenderedSubScroll || !IsWindowVisible(m_Menu);
        m_Main.frame = main;
        m_Sub.frame = sub;
        m_MenuBounds = popup;
        m_RenderedPage = m_Page;
        m_RenderedMainScroll = m_Main.scroll;
        m_RenderedSubScroll = m_Sub.scroll;
        if (redraw) {
            m_MenuDamage = {};
            // Publish position, size and pixels together. Fully transparent pixels
            // between the panels also pass mouse input through to the stream.
            if (!paintMenu(QRect(QPoint(), popup.size()))) {
                dismiss();
                return;
            }
            if (!IsWindowVisible(m_Menu)) ShowWindow(m_Menu, SW_SHOWNOACTIVATE);
        }
    }

    void showPage(Page page)
    {
        if (page == m_Page) return;
        cancelBitrateDrag();
        if (m_Page == Page::Sound) {
            KillTimer(m_Menu, HostMuteRefreshTimer);
        }
        m_Page = page;
        m_Sub.hover = -1;
        m_Sub.scroll = 0;
        m_PressedPanel = nullptr;
        if (page == Page::Security)
            m_PrivacyScreen = m_Preferences.privacyScreenForHost(m_HostUuid);
        if (page == Page::Sound && m_HostMuteAvailable) {
            requestHostMute(std::nullopt);
            SetTimer(m_Menu, HostMuteRefreshTimer, 1500, nullptr);
        }
        update();
    }

    void select(const Row& row)
    {
        if (!row.selectable()) return;
        switch (row.action) {
        case Action::Submenu:
            showPage(row.target);
            return;
        case Action::Back:
            showPage(parent(m_Page));
            return;
        case Action::SetQuality:
            setBitrate(row.value);
            return;
        case Action::SelectDisplay:
            selectDisplay(row.value);
            return;
        case Action::ResizePrivacy:
            if (m_PrivateDesktop && row.value >= 0 && row.value < privacySizes().size())
                sendDisplayRequest({}, privacySizes().at(row.value));
            return;
        case Action::Hud:
            toggle();
            return;
        case Action::Field: {
            const int next = m_Fields ^ (1 << row.value);
            if (next) {
                m_Fields = next;
                QSettings().setValue("streamHud/fields", next);
            }
            break;
        }
        case Action::Corner:
            m_Corner = row.value;
            QSettings().setValue("streamHud/corner", m_Corner);
            break;
        case Action::Opacity:
            m_Opacity = row.value;
            QSettings().setValue("streamHud/opacity", m_Opacity);
            break;
        case Action::MouseMode: {
            SDL_Event event{};
            event.type = SDL_USEREVENT;
            event.user.code = row.value;
            if (SDL_PushEvent(&event) == 1) dismiss();
            return;
        }
        case Action::SystemKeys: {
            const auto mode = static_cast<StreamingPreferences::CaptureSysKeysMode>(row.value);
            m_Input.setCaptureSystemKeysMode(mode);
            m_Preferences.setCaptureSysKeysMode(mode);
            break;
        }
        case Action::DisconnectLock:
            updateDisconnectLock(!m_LockOnDisconnect, false);
            break;
        case Action::Privacy:
            if (m_LockOnDisconnect)
                updateDisconnectLock(false, true);
            else
                reconnectForPrivacy();
            break;
        case Action::LockNow:
            requestLock();
            return;
        case Action::HostMute:
            if (!m_HostMuteAvailable) {
                if (!m_Input.toggleHostMute())
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to queue host mute key from control center");
            } else if (m_HostMuteResult.valid()) {
                if (!m_HostMuteWriting) m_HostMuteToggleAfterRead = !m_HostMuteToggleAfterRead;
            } else if (m_HostMuteState.has_value()) {
                requestHostMute(!*m_HostMuteState);
            } else {
                requestHostMute(std::nullopt);
                m_HostMuteToggleAfterRead = m_HostMuteResult.valid();
            }
            break;
        case Action::Fullscreen:
        case Action::Files:
        case Action::Disconnect: {
            SDL_Event event{};
            event.type = SDL_USEREVENT;
            event.user.code = row.action == Action::Fullscreen ? Fullscreen : (row.action == Action::Files ? FileTransfer : Disconnect);
            if (SDL_PushEvent(&event) == 1) dismiss();
            return;
        }
        default: return;
        }
        update();
    }
    static QStringList privacySizes()
    {
        return { "1280x720", "1920x1080", "1920x1200", "2560x1440", "2560x1600", "3440x1440", "3840x2160" };
    }

    QString outputId(int index) const { return m_Outputs.at(index).toObject().value("id").toString(); }
    QString displayLabel(int index) const
    {
        const auto label = m_Outputs.at(index).toObject().value("label").toString();
        const auto number = tr("Display %1").arg(index + 1);
        return label.isEmpty() ? number : number + " (" + label + ")";
    }

    bool canSelectDisplay() const
    {
        // Older hosts switch the shared capture source for everyone watching.
        return m_Outputs.size() > 1 && !m_PrivateDesktop && (m_IndependentOutputs || !m_Input.isViewOnly());
    }

    void selectDisplay(int index)
    {
        if (!canSelectDisplay() || index < 0 || index >= m_Outputs.size()) return;
        const auto output = outputId(index);
        if (m_IndependentOutputs) {
            if (!m_DisplayWindows.select(output)) {
                displayWindowFailed();
            }
            updateDisplayFeedback();
        } else if (output != m_ActiveOutput) {
            sendDisplayRequest(output, {});
        }
    }

    static QString connectingText() { return QCoreApplication::translate("PcView", "Connecting…"); }

    void hideDragPreview()
    {
        if (IsWindowVisible(m_DragPreview))
            SetWindowPos(m_DragPreview, HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_HIDEWINDOW);
        if (m_DragThumbnail) DwmUnregisterThumbnail(std::exchange(m_DragThumbnail, nullptr));
    }

    void updateDisplayFeedback()
    {
        const QString pending = m_DisplayWindows.pendingTab();
        if (pending != m_ConnectingTab) {
            m_ConnectingTab = pending;
            if (m_Tabs) InvalidateRect(m_Tabs, nullptr, FALSE);
        }
        RECT desired{}, current{};
        if (!m_DragPreview || !m_DisplayWindows.previewBounds(m_DragPreview, desired)) {
            hideDragPreview();
            return;
        }
        const bool shown = IsWindowVisible(m_DragPreview);
        GetWindowRect(m_DragPreview, &current);
        const bool raised = GetWindowLongPtrW(m_DragPreview, GWL_EXSTYLE) & WS_EX_TOPMOST;
        if (shown && EqualRect(&current, &desired) && raised == m_DraggingTab) return;
        const int w = desired.right - desired.left, h = desired.bottom - desired.top;
        const bool resize = !shown || w != current.right - current.left || h != current.bottom - current.top;
        if (!shown) {
            SetWindowTextW(m_DragPreview, m_DragLabel.toStdWString().c_str());
            if (m_PreviewActiveTab) DwmRegisterThumbnail(m_DragPreview, m_Owner, &m_DragThumbnail);
        }
        SetWindowPos(m_DragPreview, m_DraggingTab ? HWND_TOPMOST : HWND_NOTOPMOST, desired.left, desired.top, w, h,
                     SWP_NOACTIVATE | SWP_NOOWNERZORDER | (shown ? 0 : SWP_SHOWWINDOW) |
                     (shown && raised == m_DraggingTab ? SWP_NOZORDER : 0) |
                     (resize ? 0 : SWP_NOSIZE));
        if (resize && m_DragThumbnail) {
            DWM_THUMBNAIL_PROPERTIES thumbnail{};
            thumbnail.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_SOURCECLIENTAREAONLY;
            GetClientRect(m_DragPreview, &thumbnail.rcDestination);
            thumbnail.fVisible = thumbnail.fSourceClientAreaOnly = TRUE;
            DwmUpdateThumbnailProperties(m_DragThumbnail, &thumbnail);
        }
        if (!shown) UpdateWindow(m_DragPreview);
    }

    void pollDisplayWindows()
    {
        const bool ok = m_DisplayWindows.tick();
        updateDisplayFeedback();
        if (!ok) displayWindowFailed();
    }

    void focusForTabPress()
    {
        const HWND previous = GetForegroundWindow();
        if (previous == m_Owner) return;
        // Activating a background stream can wait on another input queue.
        // Keep dispatching mouse moves, and discard a focus request that outlived its press.
        QThreadPool::globalInstance()->start([owner = m_Owner, tabs = m_Tabs, previous] {
            GUITHREADINFO input{sizeof(input)};
            if (GetForegroundWindow() == previous && GetWindow(tabs, GW_OWNER) == owner &&
                GetGUIThreadInfo(GetWindowThreadProcessId(tabs, nullptr), &input) && input.hwndCapture == tabs)
                SetForegroundWindow(owner);
        });
    }

    bool pulledOutsideTabBar(POINT cursor) const
    {
        // Sliding along the tabs stays in the strip. A press near an edge
        // still needs deliberate travel before it can open another window.
        const bool outsideY = cursor.y < m_PressedTabBar.top || cursor.y >= m_PressedTabBar.bottom;
        const bool outsideX = cursor.x < m_PressedTabBar.left || cursor.x >= m_PressedTabBar.right;
        return (outsideY && std::abs(cursor.y - m_PressPoint.y) >= qMax(px(24), GetSystemMetrics(SM_CYDRAG))) ||
               (outsideX && std::abs(cursor.x - m_PressPoint.x) >= qMax(px(48), GetSystemMetrics(SM_CXDRAG)));
    }

    bool tabMouse(UINT msg, LPARAM lp)
    {
        if (msg == WM_CAPTURECHANGED || msg == WM_CANCELMODE) {
            if (m_DraggingTab) m_DisplayWindows.cancelPending();
            m_DraggingTab = false;
            m_PressedOutput.clear();
            updateDisplayFeedback();
            if (msg == WM_CANCELMODE && GetCapture() == m_Tabs) ReleaseCapture();
            return false;
        }
        if (msg != WM_LBUTTONDOWN && msg != WM_MOUSEMOVE && msg != WM_LBUTTONUP) return false;
        POINT cursor{short(LOWORD(lp)), short(HIWORD(lp))};
        RECT bar{};
        if (msg == WM_LBUTTONDOWN) {
            GetWindowRect(m_Tabs, &bar);
            ClientToScreen(m_Tabs, &cursor);
            if (!PtInRect(&bar, cursor) || m_Outputs.isEmpty()) return false;
            const auto index = (cursor.x - bar.left) * m_Outputs.size() / (bar.right - bar.left);
            m_PressedOutput = outputId(index);
            m_PressPoint = cursor;
            m_PressedTabBar = bar;
            SetCapture(m_Tabs);
            focusForTabPress();
            // Input may already have moved past the grip while the button-down was in the queue.
            if (!GetCursorPos(&cursor) || (cursor.x == m_PressPoint.x && cursor.y == m_PressPoint.y) ||
                !primaryButtonDown()) return true;
            msg = WM_MOUSEMOVE;
        } else if (GetCapture() != m_Tabs) {
            return false;
        } else if (!m_DraggingTab) {
            // A busy event loop can dispatch a click after the mouse has moved on.
            ClientToScreen(m_Tabs, &cursor);
        } else if (msg == WM_LBUTTONUP) {
            const DWORD position = GetMessagePos();
            cursor = {short(LOWORD(position)), short(HIWORD(position))};
        } else {
            GetCursorPos(&cursor); // The source window may itself be moving.
            m_DisplayWindows.moveDrag(cursor);
            SetCursor(LoadCursor(nullptr, m_DisplayWindows.overDockTarget(cursor, m_DragPreview) ? IDC_HAND : IDC_SIZEALL));
            updateDisplayFeedback();
            return true;
        }
        int index = 0;
        while (index < m_Outputs.size() && m_PressedOutput != outputId(index)) ++index;
        if (!m_DraggingTab && m_IndependentOutputs && !m_PrivateDesktop &&
            index < m_Outputs.size() && m_Outputs.size() > 1 && pulledOutsideTabBar(cursor)) {
            hideDragPreview();
            m_DragLabel = displayLabel(index);
            m_PreviewActiveTab = m_PressedOutput == m_ActiveOutput;
            m_DraggingTab = true;
            QStringList outputs;
            for (int i = 0; i < m_Outputs.size(); ++i) outputs.append(outputId(i));
            if (!m_DisplayWindows.beginDrag(m_PressedOutput, outputs, m_PressPoint, cursor))
                displayWindowFailed();
        }
        if (msg == WM_LBUTTONUP) {
            GetWindowRect(m_Tabs, &bar);
            const bool inside = PtInRect(&bar, cursor);
            const bool dragging = std::exchange(m_DraggingTab, false);
            m_PressedOutput.clear();
            ReleaseCapture();
            if (dragging) m_DisplayWindows.finishDrag(cursor, m_DragPreview);
            else if (inside) {
                if (GetForegroundWindow() != m_Owner) SetForegroundWindow(m_Owner);
                selectDisplay(index);
            }
        } else if (m_DraggingTab) {
            SetCursor(LoadCursor(nullptr, m_DisplayWindows.overDockTarget(cursor, m_DragPreview) ? IDC_HAND : IDC_SIZEALL));
        }
        updateDisplayFeedback();
        return true;
    }

    void displayWindowFailed()
    {
        m_DisplayStatus = tr("Could not open the display window.");
        m_Open = true;
        showPage(Page::Display);
        update();
    }

    void sendDisplayRequest(QString output, QString size)
    {
        if (!m_DisplayRequest || (m_Input.isViewOnly() && (!output.isEmpty() || !size.isEmpty()))) return;
        if (m_DisplayResult.valid()) {
            m_QueuedOutput = output;
            m_QueuedSize = size;
            return;
        }
        m_RequestSize = size;
        m_RequestOutput = output;
        m_DisplayResult = m_DisplayRequest(output, size);
    }

    void refreshDisplays()
    {
        pollDisplayWindows();
        if (m_DisplayResult.valid()) {
            if (m_DisplayResult.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
            const auto oldOutputs = m_Outputs;
            const auto oldActive = m_ActiveOutput, oldSize = m_DesktopSize, oldStatus = m_DisplayStatus;
            const bool wasPrivate = m_PrivateDesktop;
            const bool couldSelectDisplay = canSelectDisplay();
            const auto state = m_DisplayResult.get();
            SetTimer(m_Menu, DisplayTimer, 200, nullptr);
            if (state.isEmpty()) {
                m_DisplayStatus = tr("Sunshine could not confirm the display settings.");
            } else {
                m_Outputs = state.value("outputs").toArray();
                m_IndependentOutputs = state.value("independentOutputs").toBool();
                m_ActiveOutput = m_DisplayWindows.output().isEmpty() ? state.value("active").toString() : m_DisplayWindows.output();
                m_PrivateDesktop = state.value("privacy").toBool();
                QStringList warmOutputs;
                if (m_IndependentOutputs) {
                    m_DisplayWindows.bind(m_Owner, m_ActiveOutput);
                    if (!m_PrivateDesktop)
                        m_Outputs = m_DisplayWindows.selectableOutputs(m_Outputs, oldOutputs);
                    if (!m_PrivateDesktop && m_DisplayWindows.output().isEmpty() && !m_ActiveOutput.isEmpty()) {
                        for (int i = 0; i < m_Outputs.size(); ++i) warmOutputs.append(outputId(i));
                    }
                }
                m_DisplayWindows.prewarm(warmOutputs, QString::fromUtf8(QJsonDocument(m_Outputs).toJson(QJsonDocument::Compact)));
                m_DesktopSize = QStringLiteral("%1x%2").arg(state.value("width").toInt()).arg(state.value("height").toInt());
                if (!m_RequestSize.isEmpty()) {
                    m_Preferences.setPrivacyDesktopSizeForHost(m_HostUuid, m_RequestSize);
                    m_DisplayStatus.clear();
                } else if (!m_RequestOutput.isEmpty()) {
                    m_DisplayStatus.clear();
                }
            }
            if (!m_PrivateDesktop && m_Page == Page::PrivacyResolution) m_Page = Page::Display;
            const bool tabsChanged = oldOutputs != m_Outputs || oldActive != m_ActiveOutput ||
                                     wasPrivate != m_PrivateDesktop || couldSelectDisplay != canSelectDisplay();
            const int reportedCount = state.isEmpty() ? -1 : int(state.value("outputs").toArray().size());
            if (tabsChanged || oldStatus != m_DisplayStatus || reportedCount != m_LastReportedCount) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Display controls: output=%s reported=%d selectable=%d independent=%d privacy=%d confirmed=%d",
                            qUtf8Printable(m_ActiveOutput), reportedCount, int(m_Outputs.size()), m_IndependentOutputs, m_PrivateDesktop, !state.isEmpty());
            }
            m_LastReportedCount = reportedCount;
            if (tabsChanged && m_Tabs) InvalidateRect(m_Tabs, nullptr, FALSE);
            if (tabsChanged || oldSize != m_DesktopSize || oldStatus != m_DisplayStatus)
                update(tabsChanged || m_Page == Page::Display || m_Page == Page::PrivacyResolution);
            if (!m_Outputs.isEmpty()) m_DiscoveryRetries = 0;
            // Sunshine publishes its output list only after opening the capture
            // device. The first query can legitimately return an empty snapshot.
            int pollDelay = m_RequestOutput.isEmpty() && m_RequestSize.isEmpty() ? 3000 : 400;
            if (m_DiscoveryRetries > 0) {
                --m_DiscoveryRetries;
                pollDelay = 150;
            }
            m_NextDisplayPoll = SDL_GetTicks() + pollDelay;
        }
        if (!m_QueuedOutput.isEmpty() || !m_QueuedSize.isEmpty()) {
            sendDisplayRequest(std::exchange(m_QueuedOutput, QString()), std::exchange(m_QueuedSize, QString()));
        } else if (SDL_TICKS_PASSED(SDL_GetTicks(), m_NextDisplayPoll)) {
            sendDisplayRequest({}, {});
        }
    }

    HWND create(DWORD extra, const wchar_t* name = L"MoonlightControlCenter", DWORD style = WS_POPUP, UINT classStyle = 0)
    {
        WNDCLASSW wc { };
        wc.lpfnWndProc = proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = name;
        wc.style = classStyle;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        // An owned preview already stays out of the taskbar. Give it the full
        // caption and frame metrics of the stream window it will become.
        const DWORD extendedStyle = WS_EX_NOACTIVATE | extra | ((style & WS_CAPTION) ? 0 : WS_EX_TOOLWINDOW);
        return CreateWindowExW(extendedStyle, wc.lpszClassName, L"", style, 0, 0,
            0, 0, m_Owner, nullptr, wc.hInstance, this);
    }
    void place(HWND hwnd, int x, int y, int w, int h)
    {
        if (!hwnd || w <= 0 || h <= 0)
            return;
        RECT rect { };
        GetWindowRect(hwnd, &rect);
        const bool resized = w != rect.right - rect.left || h != rect.bottom - rect.top;
        if (hwnd == m_Dock) {
            const int radius = IsZoomed(m_Owner) ? 0 : px(10);
            if (resized || radius != m_DockCornerRadius) {
                // Leave the rounded corner and its antialiased edge to DWM.
                HRGN region = radius ? CreateRoundRectRgn(-radius, 0, w, h + radius, radius * 2, radius * 2) : nullptr;
                if (SetWindowRgn(hwnd, region, FALSE))
                    m_DockCornerRadius = radius;
                else if (region)
                    DeleteObject(region);
            }
        } else if (resized) {
            HRGN region = CreateRoundRectRgn(0, 0, w + 1, h + 1, px(12), px(12));
            if (!SetWindowRgn(hwnd, region, FALSE))
                DeleteObject(region);
        }
        const bool hidden = !IsWindowVisible(hwnd);
        if (rect.left != x || rect.top != y || resized || hidden) {
            SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOACTIVATE | SWP_NOZORDER |
                         (hidden ? SWP_SHOWWINDOW : 0) | (resized ? 0 : SWP_NOSIZE));
            if (resized || hidden) InvalidateRect(hwnd, nullptr, FALSE);
        }
    }
    void reconnectForPrivacy()
    {
        SDL_Event event{};
        event.type = SDL_USEREVENT;
        event.user.code = PrivacyReconnect;
        if (SDL_PushEvent(&event) != 1) {
            m_LockStatus = tr("Could not start the privacy screen reconnect.");
            return;
        }
        m_Preferences.setPrivacyScreenForHost(m_HostUuid, !m_PrivacyScreen);
        dismiss();
    }

    void updateDisconnectLock(bool enabled, bool privacyReconnect)
    {
        if (!m_RequestDisconnectLock || m_DisconnectLockResult.valid()) {
            if (!m_RequestDisconnectLock)
                m_LockStatus = tr("Update Sunshine to change the lock setting during a stream.");
            return;
        }
        m_DisconnectLockResult = m_RequestDisconnectLock(enabled);
        if (!m_DisconnectLockResult.valid())
            return;
        m_LockSettingRequested = enabled;
        m_ReconnectAfterUnlock = privacyReconnect;
        m_LockStatus = tr("Applying disconnect lock setting…");
        SetTimer(m_Menu, DisconnectLockTimer, 75, nullptr);
    }

    void finishDisconnectLock()
    {
        const auto result = m_DisconnectLockResult.get();
        if (!result.has_value()) {
            m_LockStatus = tr("Sunshine did not confirm the disconnect lock setting.");
            return;
        }
        m_LockOnDisconnect = *result;
        if (m_LockOnDisconnect != m_LockSettingRequested) {
            m_LockStatus = tr("Sunshine requires locking when the stream disconnects.");
        }
        else if (m_ReconnectAfterUnlock) {
            reconnectForPrivacy();
        }
        else {
            m_Preferences.setLockHostOnDisconnect(m_LockSettingRequested);
            m_LockStatus = tr("Disconnect lock updated for this stream.");
        }
    }

    void requestHostMute(std::optional<bool> desired)
    {
        if (m_HostMuteResult.valid())
            return;
        m_HostMuteResult = m_RequestHostMute(desired);
        if (m_HostMuteResult.valid()) {
            m_HostMuteWriting = desired.has_value();
            SetTimer(m_Menu, HostMuteResultTimer, 75, nullptr);
        }
    }

    QString hostMuteStatus() const
    {
        if (m_HostMuteWriting) return tr("Updating…");
        if (m_HostMuteState.has_value()) return *m_HostMuteState ? tr("Muted") : tr("Unmuted");
        return m_HostMuteResult.valid() ? tr("Checking…") : tr("State unavailable");
    }

    // A DIB selected into a memory DC lets Qt draw directly into the bitmap
    // consumed by UpdateLayeredWindow. Keep it until the popup size changes.
    struct MenuSurface {
        HDC dc = nullptr;
        HBITMAP bitmap = nullptr;
        HGDIOBJ previous = nullptr;
        QImage image;

        ~MenuSurface() { reset(); }
        void reset()
        {
            image = {};
            if (dc && previous) SelectObject(dc, previous);
            if (bitmap) DeleteObject(bitmap);
            if (dc) DeleteDC(dc);
            dc = nullptr;
            bitmap = nullptr;
            previous = nullptr;
        }
        bool resize(QSize size)
        {
            if (!image.isNull() && image.size() == size) return true;
            reset();
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = size.width();
            info.bmiHeader.biHeight = -size.height();
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            void* pixels = nullptr;
            dc = CreateCompatibleDC(nullptr);
            if (!dc) return false;
            bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            if (!bitmap) return false;
            previous = SelectObject(dc, bitmap);
            if (!previous) return false;
            image = QImage(static_cast<uchar*>(pixels), size.width(), size.height(),
                           size.width() * 4, QImage::Format_ARGB32_Premultiplied);
            return !image.isNull();
        }
    };

    bool paintMenu(QRect dirty)
    {
        if (!m_MenuSurface.resize(m_MenuBounds.size())) return false;
        auto& image = m_MenuSurface.image;
        dirty = dirty.intersected(image.rect());
        if (dirty.isEmpty()) return true;
        QPainter painter(&image);
        painter.setClipRect(dirty);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(dirty, Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        if (dirty.intersects(m_Main.frame)) m_Main.paint(painter, m_Scale, category(m_Page));
        if (dirty.intersects(m_Sub.frame)) m_Sub.paint(painter, m_Scale, Page::Main);
        painter.end();

        POINT dst{m_MenuBounds.x(), m_MenuBounds.y()}, src{};
        SIZE size{image.width(), image.height()};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        return UpdateLayeredWindow(m_Menu, nullptr, &dst, &size, m_MenuSurface.dc, &src, 0, &blend, ULW_ALPHA);
    }

    void paint(HWND hwnd, HDC dc)
    {
        RECT r{};
        GetClientRect(hwnd, &r);
        if (hwnd == m_DragPreview) {
            // Let the compositor move the surface without painting a video-sized bitmap on every mouse move.
            SetDCBrushColor(dc, RGB(32, 34, 36));
            FillRect(dc, &r, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(237, 239, 241));
            const auto font = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
            DrawTextW(dc, connectingText().toStdWString().c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, font);
            return;
        }
        QImage image(r.right, r.bottom, QImage::Format_RGB32);
        if (image.isNull()) return;
        image.fill(hwnd == m_Button ? QColor(m_Open || m_ButtonHover ? "#c9cdd1" : "#e2e5e8") : QColor("#f8f9fa"));
        QPainter p(&image);
        p.scale(m_Scale, m_Scale);
        p.setFont(CaptionMenu::font(13));
        const int w = qRound(r.right / m_Scale), h = qRound(r.bottom / m_Scale);
        if (hwnd == m_Dock) {
            const bool active = GetForegroundWindow() == m_Owner;
            const auto color = [](COLORREF value) {
                return QColor(GetRValue(value), GetGValue(value), GetBValue(value));
            };
            const QColor background = color(m_UsesLightCaption ? LightWindowCaptionColor :
                                            GetSysColor(active ? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION));
            const QColor ink = m_UsesLightCaption
                ? (active ? QColor(Qt::black) : background.darker(167))
                : color(GetSysColor(active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT));
            p.resetTransform();
            p.fillRect(image.rect(), background);
            if (m_DockHover) {
                QColor highlight = m_UsesLightCaption ? Qt::black : ink;
                highlight.setAlpha(GetCapture() == hwnd ? 20 : 10);
                p.fillRect(image.rect(), highlight);
            }
            // Snap the straight strokes to the physical pixel grid used by the neighboring buttons.
            const int size = px(10);
            const int left = qRound(m_DockGlyphCenter.x() - size / 2.0);
            const int axis = int(m_DockGlyphCenter.y());
            const int tip = left + px(3);
            const int wing = px(3);
            const int stroke = qMax(1, qFloor(m_Scale));
            p.setRenderHint(QPainter::Antialiasing);
            p.fillRect(QRectF(left, axis - size / 2, m_Scale, size), ink);
            p.fillRect(QRect(tip, axis - (stroke - 1) / 2, left + size - tip, stroke), ink);
            p.setPen(QPen(ink, m_Scale, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
            const QPointF point(tip + 0.5, axis + 0.5);
            const QPointF arrow[] = {point + QPointF(wing, -wing), point, point + QPointF(wing, wing)};
            p.drawPolyline(arrow, 3);
        } else if (hwnd == m_Tabs && !m_Outputs.isEmpty()) {
            const int count = m_Outputs.size();
            for (int i = 0; i < count; ++i) {
                QRect tab(i * w / count, 0, (i + 1) * w / count - i * w / count, h);
                const bool active = outputId(i) == m_ActiveOutput;
                p.fillRect(tab, QColor(active ? "#ffffff" : "#e2e5e8"));
                if (active) p.fillRect(tab.left(), h - 2, tab.width(), 2, QColor("#2765ad"));
                p.setPen(QColor("#25282b"));
                const QString label = outputId(i) == m_ConnectingTab ?
                            tr("Display %1").arg(i + 1) + QLatin1Char(' ') + connectingText() : displayLabel(i);
                p.drawText(tab.adjusted(5, 0, -5, 0), Qt::AlignCenter,
                           p.fontMetrics().elidedText(label, Qt::ElideRight, qMax(0, tab.width() - 10)));
            }
        } else if (hwnd == m_Button) {
            const QString label = tr("Control center");
            const int left = (w - p.fontMetrics().horizontalAdvance(label) - 20) / 2;
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(QColor("#25282b"), 1.25));
            for (int i = 0; i < 2; ++i) {
                QRectF track(left, h * 0.5 - 6 + 7 * i, 13, 5);
                p.drawRoundedRect(track, 2.5, 2.5);
                p.fillRect(QRectF(left + (i ? 8 : 3), track.top() + 1.75, 1.5, 1.5), p.pen().color());
            }
            p.drawText(QRect(left + 20, 0, w - left - 20, h), Qt::AlignLeft | Qt::AlignVCenter, label);
        } else {
            const auto names = labels();
            int y = 6;
            for (int i = 0; i < FieldCount; ++i) {
                if (!(m_Fields & (1 << i))) continue;
                const QRect line(12, y, w - 24, 23);
                p.setPen(QColor("#52647a"));
                p.drawText(line, Qt::AlignLeft | Qt::AlignVCenter, names[i]);
                p.setPen(QColor("#172b45"));
                p.drawText(line, Qt::AlignRight | Qt::AlignVCenter, m_Values[i]);
                y += 23;
            }
        }
        p.end();
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = image.width();
        bi.bmiHeader.biHeight = -image.height();
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        SetDIBitsToDevice(dc, 0, 0, image.width(), image.height(), 0, 0, 0, image.height(), image.constBits(), &bi,
            DIB_RGB_COLORS);
    }

    Panel* panelAt(QPoint pos)
    {
        if (m_Main.frame.contains(pos)) return &m_Main;
        if (m_Sub.frame.contains(pos)) return &m_Sub;
        return nullptr;
    }
    bool menuWheel(WPARAM wp, LPARAM lp)
    {
        if (!m_Open) return false;
        POINT pt{short(LOWORD(lp)), short(HIWORD(lp))};
        ScreenToClient(m_Menu, &pt);
        auto* panel = panelAt(QPoint(pt.x, pt.y));
        if (!panel) return false;
        const int delta = qRound(short(HIWORD(wp)) * 68.0 / WHEEL_DELTA);
        panel->scroll = qBound(0, panel->scroll - delta, panel->contentHeight - panel->size.height());
        setMenuHover(*panel, -1);
        update(false);
        return true;
    }
    void queueMenuPaint(QRect rect)
    {
        rect = rect.intersected(QRect(QPoint(), m_MenuBounds.size()));
        if (rect.isEmpty()) return;
        if (m_MenuDamage.isEmpty() && !PostMessageW(m_Menu, MenuPaintMessage, 0, 0)) {
            paintMenu(rect);
            return;
        }
        m_MenuDamage |= rect;
    }

    void setMenuHover(Panel& panel, int row)
    {
        if (panel.hover == row) return;
        const int previous = std::exchange(panel.hover, row);
        for (int index : {previous, row}) {
            if (index < 0 || index >= panel.rows.size()) continue;
            const auto& item = panel.rows[index];
            const int top = item.top - panel.scroll;
            const int y = px(top) - 1;
            queueMenuPaint(QRect(panel.frame.x(), panel.frame.y() + y, panel.frame.width(),
                                 px(top + item.height()) + 1 - y).intersected(panel.frame));
        }
    }
    void setBitrate(int kbps)
    {
        if (kbps != m_BitrateKbps && m_RequestBitrate && !m_BitrateResult.valid()) {
            m_PreviewKbps = kbps;
            m_QualityStatus.clear();
            m_BitrateResult = m_RequestBitrate(kbps);
        }
        update();
    }

    void cancelBitrateDrag()
    {
        if (std::exchange(m_DraggingBitrate, false) && GetCapture() == m_Menu)
            ReleaseCapture();
    }

    void menuMouse(UINT msg, QPoint pos)
    {
        Panel* panel = panelAt(pos);
        const int row = panel ? panel->rowAt(QPoint(int((pos.x() - panel->frame.x()) / m_Scale),
                                                    int((pos.y() - panel->frame.y()) / m_Scale))) : -1;
        if (msg == WM_LBUTTONDOWN && panel == &m_Sub && row >= 0 && panel->rows[row].action == Action::BitrateSlider) {
            m_PressedPanel = nullptr;
            m_DraggingBitrate = true;
            m_Sub.hover = row;
            SetCapture(m_Menu);
        }
        if (m_DraggingBitrate) {
            const auto& slider = m_Sub.rows.last();
            m_PreviewKbps = slider.sliderValue((pos.x() - m_Sub.frame.x()) / m_Scale, m_Sub.size.width());
            if (msg == WM_LBUTTONUP) {
                cancelBitrateDrag();
                setBitrate(m_PreviewKbps);
            } else {
                update();
            }
            return;
        }
        if (msg == WM_LBUTTONDOWN) {
            m_PressedPanel = panel;
            m_PressedRow = row;
        } else if (msg == WM_LBUTTONUP) {
            Panel* pressed = std::exchange(m_PressedPanel, nullptr);
            if (panel && row >= 0 && panel == pressed && row == m_PressedRow)
                select(panel->rows.at(row));
        } else {
            const int mainHover = panel == &m_Main ? row : -1;
            const int subHover = panel == &m_Sub ? row : -1;
            if (mainHover == m_Main.hover && subHover == m_Sub.hover) return;
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, m_Menu, 0};
            TrackMouseEvent(&track);
            if (mainHover >= 0 && !m_Inline && !m_PressedPanel) {
                const auto& item = m_Main.rows[mainHover];
                if (item.target != category(m_Page)) {
                    // showPage paints both panels before this mouse move returns.
                    m_Main.hover = mainHover;
                    showPage(item.target);
                    return;
                }
            }
            setMenuHover(m_Main, mainHover);
            setMenuHover(m_Sub, subHover);
        }
    }
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        if (msg == WM_NCCREATE)
            SetWindowLongPtr(hwnd, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams));
        auto* self = reinterpret_cast<CaptionControls*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (!self)
            return DefWindowProcW(hwnd, msg, wp, lp);
        if (msg == WM_NCHITTEST && (hwnd == self->m_Tabs || hwnd == self->m_Button || hwnd == self->m_Dock)) {
            // Caption popups can overlap the top sizing border at some DPIs.
            const auto hit = SendMessageW(self->m_Owner, WM_NCHITTEST, wp, lp);
            if (hit >= HTLEFT && hit <= HTBOTTOMRIGHT) return HTTRANSPARENT;
        }
        if (msg == WM_MOUSEACTIVATE) {
            // Tabs handle activation after taking capture in WM_LBUTTONDOWN.
            if (hwnd == self->m_Button && HIWORD(lp) == WM_LBUTTONDOWN &&
                self->m_Owner && GetForegroundWindow() != self->m_Owner)
                SetForegroundWindow(self->m_Owner);
            return MA_NOACTIVATE;
        }
        if (hwnd == self->m_Dock) {
            if (msg == WM_MOUSEMOVE && !self->m_DockHover) {
                TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd, 0};
                self->m_DockHover = TrackMouseEvent(&track);
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (msg == WM_MOUSELEAVE || (msg == WM_SHOWWINDOW && !wp)) {
                self->m_DockHover = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if (msg == WM_LBUTTONDOWN) {
                SendMessageW(self->m_DockTip, TTM_POP, 0, 0);
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (msg == WM_CAPTURECHANGED) InvalidateRect(hwnd, nullptr, FALSE);
            if (msg == WM_LBUTTONUP && GetCapture() == hwnd) {
                RECT rect{};
                GetClientRect(hwnd, &rect);
                ReleaseCapture();
                if (PtInRect(&rect, {short(LOWORD(lp)), short(HIWORD(lp))}))
                    self->m_DisplayWindows.dock();
                return 0;
            }
        }
        if (hwnd == self->m_Tabs && self->tabMouse(msg, lp)) return 0;
        if (hwnd == self->m_Menu && msg == WM_TIMER && wp == DisplayTimer) {
            self->refreshDisplays();
            return 0;
        }
        if (hwnd == self->m_DragPreview && msg == WM_CLOSE) {
            self->m_DisplayWindows.cancelPending();
            self->updateDisplayFeedback();
            return 0;
        }
        if (hwnd == self->m_DragPreview && msg == WM_NCLBUTTONDOWN) {
            if (wp == HTCAPTION) {
                if (self->m_DisplayWindows.grabPreview(hwnd, {short(LOWORD(lp)), short(HIWORD(lp))})) {
                    // The source keeps capture as the preview gives way to the stream window.
                    self->m_DraggingTab = true;
                    SetCapture(self->m_Tabs);
                    self->focusForTabPress();
                }
                return 0;
            }
            if (wp >= HTLEFT && wp <= HTBOTTOMRIGHT) return 0;
        }
        if (msg == WM_NCHITTEST && hwnd == self->m_Hud)
            return HTTRANSPARENT;
        if (hwnd == self->m_Menu && msg == WM_TIMER && wp == LockStatusTimer) {
            if (self->m_LockResult.valid() && self->m_LockResult.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                KillTimer(hwnd, LockStatusTimer);
                const auto error = self->m_LockResult.get();
                self->m_LockStatus = error.isEmpty() ? tr("Host accepted the lock request.") : error;
                if (self->m_Open) self->update();
            }
            return 0;
        }
        if (hwnd == self->m_Menu && msg == WM_TIMER && wp == DisconnectLockTimer) {
            if (self->m_DisconnectLockResult.valid() &&
                self->m_DisconnectLockResult.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                KillTimer(hwnd, DisconnectLockTimer);
                self->finishDisconnectLock();
                if (self->m_Open) self->update();
            }
            return 0;
        }
        if (hwnd == self->m_Menu && msg == WM_TIMER && wp == HostMuteResultTimer) {
            if (self->m_HostMuteResult.valid() && self->m_HostMuteResult.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                KillTimer(hwnd, HostMuteResultTimer);
                const auto oldStatus = self->hostMuteStatus();
                const auto oldMute = self->m_HostMuteState;
                const bool wasWriting = self->m_HostMuteWriting;
                self->m_HostMuteState = self->m_HostMuteResult.get();
                self->m_HostMuteWriting = false;
                if (std::exchange(self->m_HostMuteToggleAfterRead, false) &&
                    self->m_Open && self->m_HostMuteState.has_value()) {
                    self->requestHostMute(!*self->m_HostMuteState);
                }
                if (self->m_Open && self->m_Page == Page::Sound &&
                    (oldStatus != self->hostMuteStatus() || oldMute != self->m_HostMuteState ||
                     wasWriting != self->m_HostMuteWriting)) self->update();
            }
            return 0;
        }
        if (hwnd == self->m_Menu && msg == WM_TIMER && wp == HostMuteRefreshTimer) {
            if (self->m_Open && self->m_Page == Page::Sound) {
                self->requestHostMute(std::nullopt);
            }
            return 0;
        }
        if (hwnd == self->m_Menu && msg == MenuPaintMessage) {
            const QRect dirty = std::exchange(self->m_MenuDamage, QRect());
            if (self->m_Open && !dirty.isEmpty()) self->paintMenu(dirty);
            return 0;
        }
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps { };
            HDC dc = BeginPaint(hwnd, &ps);
            if (hwnd != self->m_Menu) self->paint(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        if (hwnd == self->m_Menu && (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP)) {
            if (self->m_Open)
                self->menuMouse(msg, QPoint(short(LOWORD(lp)), short(HIWORD(lp))));
            return 0;
        }
        if (hwnd == self->m_Menu && msg == WM_MOUSEWHEEL && self->menuWheel(wp, lp))
            return 0;
        if (hwnd == self->m_Menu && msg == WM_CAPTURECHANGED && self->m_DraggingBitrate) {
            self->m_DraggingBitrate = false;
            self->update();
            return 0;
        }
        if (hwnd == self->m_Menu && msg == WM_MOUSELEAVE && !self->m_DraggingBitrate) {
            self->m_PressedPanel = nullptr;
            self->setMenuHover(self->m_Main, -1);
            self->setMenuHover(self->m_Sub, -1);
            return 0;
        }
        if (hwnd == self->m_Button && msg == WM_MOUSEMOVE && !self->m_ButtonHover) {
            TRACKMOUSEEVENT track { sizeof(track), TME_LEAVE, hwnd, 0 };
            self->m_ButtonHover = TrackMouseEvent(&track);
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (hwnd == self->m_Button && msg == WM_MOUSELEAVE) {
            self->m_ButtonHover = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        if (hwnd == self->m_Button && msg == WM_LBUTTONUP) {
            if (self->m_Open)
                self->dismiss();
            else {
                // Handle clicks delivered without WM_MOUSEACTIVATE as well.
                if (GetForegroundWindow() != self->m_Owner && !SetForegroundWindow(self->m_Owner))
                    return 0;
                self->m_Open = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                self->m_Page = Page::Main;
                self->m_Main.hover = self->m_Sub.hover = -1;
                self->m_Main.scroll = self->m_Sub.scroll = 0;
                self->update();
                if (!self->m_Open || !IsWindowVisible(self->m_Menu)) {
                    self->dismiss();
                    return 0;
                }
                SetWindowPos(self->m_Menu, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    SDL_Window* m_Window;
    StreamingPreferences& m_Preferences;
    SdlInputHandler& m_Input;
    int m_BitrateKbps;
    const int m_InitialBitrateKbps;
    BitrateRequest m_RequestBitrate;
    std::future<bool> m_BitrateResult;
    int m_PreviewKbps = 0;
    bool m_DraggingBitrate = false;
    QString m_QualityStatus;
    std::function<std::future<QString>()> m_LockHost;
    std::function<std::future<std::optional<bool>>(std::optional<bool>)> m_RequestHostMute;
    const bool m_HostMuteAvailable;
    const QString m_HostUuid;
    const bool m_HostPrivacyAvailable;
    bool m_PrivacyScreen = false;
    const DisconnectLockRequest m_RequestDisconnectLock;
    bool m_LockOnDisconnect = false;
    DisplayRequest m_DisplayRequest;
    DisplayWindows m_DisplayWindows;
    bool m_IndependentOutputs = false, m_DraggingTab = false;
    bool m_CaptionMoved = false;
    std::optional<RECT> m_CaptionMoveStart;
    QString m_PressedOutput, m_DragLabel, m_ConnectingTab;
    bool m_PreviewActiveTab = false;
    HTHUMBNAIL m_DragThumbnail = nullptr;
    POINT m_PressPoint{};
    RECT m_PressedTabBar{};
    std::future<QJsonObject> m_DisplayResult;
    int m_DiscoveryRetries = 12;
    QJsonArray m_Outputs;
    int m_LastReportedCount = -1;
    QString m_ActiveOutput, m_DesktopSize, m_DisplayStatus;
    QString m_RequestSize, m_RequestOutput, m_QueuedSize, m_QueuedOutput;
    bool m_PrivateDesktop = false;
    Uint32 m_NextDisplayPoll = 0;
    HWND m_Tabs = nullptr, m_Dock = nullptr, m_DockTip = nullptr, m_DragPreview = nullptr;
    std::wstring m_DockLabel;
    bool m_UsesLightCaption = false;
    bool m_DockHover = false;
    int m_DockCornerRadius = -1;
    QPointF m_DockGlyphCenter;
    static constexpr UINT_PTR DisplayTimer = 6;
    static constexpr UINT_PTR LockStatusTimer = 1;
    static constexpr UINT_PTR HostMuteResultTimer = 2;
    static constexpr UINT_PTR HostMuteRefreshTimer = 3;
    static constexpr UINT_PTR DisconnectLockTimer = 4;
    std::future<std::optional<bool>> m_DisconnectLockResult;
    bool m_LockSettingRequested = false;
    bool m_ReconnectAfterUnlock = false;
    std::future<std::optional<bool>> m_HostMuteResult;
    std::optional<bool> m_HostMuteState;
    bool m_HostMuteWriting = false;
    bool m_HostMuteToggleAfterRead = false;
    std::future<QString> m_LockResult;
    QString m_LockStatus;
    int m_Fields, m_Corner, m_Opacity;
    struct PlacedControl { HWND window; POINT offset; };
    std::array<PlacedControl, 4> m_PlacedControls{}; // Button, tabs, dock and HUD
    int m_PlacedCount = 0;
    HMONITOR m_LayoutMonitor = nullptr;
    UINT m_LayoutDpi = 96;
    double m_Scale = 1;
    bool m_Open = false, m_ButtonHover = false;
    Page m_Page = Page::Main;
    Panel m_Main, m_Sub;
    MenuSurface m_MenuSurface;
    static constexpr UINT MenuPaintMessage = WM_APP + 1;
    QRect m_MenuDamage;
    QRect m_MenuBounds;
    QSize m_MenuLimits;
    double m_MenuScale = 0;
    int m_MenuDisplay = -1;
    bool m_MenuContentChanged = true;
    Page m_RenderedPage = Page::Main;
    int m_RenderedMainScroll = 0, m_RenderedSubScroll = 0;
    bool m_Inline = false;
    Panel* m_PressedPanel = nullptr;
    int m_PressedRow = -1;
    QSet<SDL_Scancode> m_MenuKeys;
    HWND m_Owner = nullptr, m_Button = nullptr, m_Menu = nullptr, m_Hud = nullptr;
    std::array<QString, FieldCount> m_Values;
};
