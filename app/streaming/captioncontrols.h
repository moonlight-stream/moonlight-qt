#pragma once

#include <SDL_syswm.h>
#include "captionmenu.h"
#include "windowcaption.h"
#include "input/input.h"
#include <commctrl.h>
#include <QCoreApplication>
#include <QPainter>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <array>
#include <functional>
#include <future>
#include <optional>
#include <utility>

// The SDL loop owns these three non-activating windows. Menu input never
// starts a nested message loop or blocks the streaming thread.
class CaptionControls {
public:
    static constexpr int Fullscreen = 111;
    static constexpr int FileTransfer = 112;
    static constexpr int PrivacyReconnect = 114;
    static constexpr int Disconnect = 115;
    using DisconnectLockRequest = std::function<std::future<std::optional<bool>>(bool)>;
    enum Field { Duration, Fps, Network, Decode, Upload, Download, Loss, Resolution, FieldCount };
    CaptionControls(SDL_Window* window, StreamingPreferences& preferences, SdlInputHandler& input,
                    std::function<std::future<QString>()> lockHost,
                    std::function<std::future<std::optional<bool>>(std::optional<bool>)> hostMute,
                    bool hostMuteAvailable, const QString& hostUuid, bool hostPrivacyAvailable,
                    bool disconnectLockEnabled, DisconnectLockRequest disconnectLock)
        : m_Window(window)
        , m_Preferences(preferences)
        , m_Input(input)
        , m_LockHost(std::move(lockHost))
        , m_RequestHostMute(std::move(hostMute))
        , m_HostMuteAvailable(hostMuteAvailable)
        , m_HostUuid(hostUuid)
        , m_HostPrivacyAvailable(hostPrivacyAvailable)
        , m_RequestDisconnectLock(std::move(disconnectLock))
        , m_LockOnDisconnect(disconnectLockEnabled)
    {
        QSettings settings;
        m_Fields = settings.value("streamHud/fields", 0xff).toInt() & 0xff;
        if (!m_Fields)
            m_Fields = 0xff;
        m_Corner = qBound(0, settings.value("streamHud/corner", 0).toInt(), 3);
        m_Opacity = qBound(1, settings.value("streamHud/opacity", 3).toInt(), 4);
        m_Values.fill(QStringLiteral("—"));
    }

    ~CaptionControls() { close(); }
    void close()
    {
        if (m_Owner)
            RemoveWindowSubclass(m_Owner, ownerProc, reinterpret_cast<UINT_PTR>(this));
        for (HWND* window : { &m_Menu, &m_Hud, &m_Button }) {
            if (*window && IsWindow(*window))
                DestroyWindow(*window);
            *window = nullptr;
        }
        m_Owner = nullptr;
        m_Open = false;
        m_Main.frame = m_Sub.frame = {};
        m_PressedPanel = nullptr;
        m_MenuKeys.clear();
        m_ButtonHover = false;
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

    void setValues(const std::array<QString, FieldCount>& values) { m_Values = values; }
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
        m_Open = false;
        m_Page = Page::Main;
        m_Main.hover = m_Sub.hover = -1;
        m_Main.scroll = m_Sub.scroll = 0;
        m_PressedPanel = nullptr;
        m_HostMuteToggleAfterRead = false;
        if (m_Menu) {
            KillTimer(m_Menu, SubmenuTimer);
            KillTimer(m_Menu, HostMuteRefreshTimer);
            ShowWindow(m_Menu, SW_HIDE);
        }
    }

    void update()
    {
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
            applyLightWindowCaption(owner);
            m_Button = create(0);
            m_Menu = create(0, true);
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
        switch (key.keysym.sym) {
        case SDLK_UP:
        case SDLK_DOWN:
            panel.moveSelection(key.keysym.sym == SDLK_UP ? -1 : 1);
            InvalidateRect(m_Menu, nullptr, FALSE);
            break;
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
        KillTimer(m_Menu, SubmenuTimer);
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
        if (msg == WM_NCDESTROY)
            RemoveWindowSubclass(hwnd, ownerProc, id);
        if ((msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE) || msg == WM_NCLBUTTONDOWN)
            self->dismiss();
        if (msg == WM_MOUSEWHEEL && self->menuWheel(wp, lp)) return 0;
        LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
        if (msg == WM_WINDOWPOSCHANGED) {
            const auto* pos = reinterpret_cast<const WINDOWPOS*>(lp);
            if ((pos->flags & (SWP_NOMOVE | SWP_NOSIZE)) != (SWP_NOMOVE | SWP_NOSIZE))
                self->layout(hwnd);
        }
        return result;
    }

    void layout(HWND owner)
    {
        UINT flags = SDL_GetWindowFlags(m_Window);
        RECT bounds { }, client { }, win { };
        POINT origin { };
        GetClientRect(owner, &client);
        ClientToScreen(owner, &origin);
        GetWindowRect(owner, &win);
        using DpiForWindow = UINT(WINAPI*)(HWND);
        static const auto dpiForWindow = reinterpret_cast<DpiForWindow>(
            GetProcAddress(GetModuleHandleW(L"user32"), "GetDpiForWindow"));
        m_Scale = dpiForWindow ? dpiForWindow(owner) / 96.0 : 1.0;
        const bool visible = !(flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN));
        const bool caption = visible && !(flags & SDL_WINDOW_FULLSCREEN)
            && SUCCEEDED(DwmGetWindowAttribute(owner, DWMWA_CAPTION_BUTTON_BOUNDS, &bounds, sizeof(bounds)))
            && bounds.left > px(128) && bounds.bottom > bounds.top;
        if (caption) {
            int right = win.left + bounds.left - px(3);
            int top = win.top + bounds.top;
            int bottom = qMin(int(origin.y), int(win.top + bounds.bottom));
            // A maximized HWND extends beyond the visible frame by the resize border.
            RECT frame{};
            if (IsZoomed(owner) && SUCCEEDED(DwmGetWindowAttribute(owner, DWMWA_EXTENDED_FRAME_BOUNDS,
                                                                   &frame, sizeof(frame))))
                top = qMax(top, int(frame.top));
            const int buttonWidth = px(36 + QFontMetrics(CaptionMenu::font(13)).horizontalAdvance(tr("Control center")));
            const int buttonHeight = qMin(px(28), bottom - top - 2 * px(2));
            place(m_Button, right - buttonWidth, (top + bottom - buttonHeight) / 2, buttonWidth, buttonHeight);
            if (m_Open) {
                if (GetForegroundWindow() != owner)
                    dismiss();
                else
                    layoutMenu(right, origin.y + px(2));
            }
        } else {
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
        } else
            ShowWindow(m_Hud, SW_HIDE);
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
        return page == Page::Fields || page == Page::Corner || page == Page::Opacity ? Page::Display : Page::Main;
    }
    static Page category(Page page) { return parent(page) == Page::Display ? Page::Display : page; }

    QString pageTitle(Page page) const
    {
        switch (page) {
        case Page::Display: {
            const int display = SDL_GetWindowDisplayIndex(m_Window);
            return display >= 0 ? tr("Display %1").arg(display + 1) : tr("Display");
        }
        case Page::Sound: return tr("Sound");
        case Page::Security: return tr("Security");
        case Page::SystemKeys: return tr("System shortcuts");
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
            panel.submenu(Page::Display, pageTitle(Page::Display), Icon::Display);
            if (!m_Input.isViewOnly()) {
                panel.add(Action::Divider);
                panel.submenu(Page::Sound, pageTitle(Page::Sound), Icon::Sound);
                panel.submenu(Page::Security, pageTitle(Page::Security), Icon::Security);
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
            panel.add(Action::Divider);
            panel.add(Action::Hud, tr("Statistics HUD")).checked = enabled();
            for (Page child : {Page::Fields, Page::Corner, Page::Opacity})
                panel.submenu(child, pageTitle(child));
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
        if (m_Inline != inlinePage) {
            m_Sub.hover = -1;
            m_PressedPanel = nullptr;
            KillTimer(m_Menu, SubmenuTimer);
            m_Inline = inlinePage;
        }

        populate(m_Main, Page::Main);
        m_Main.measure(mainWidth, maxHeight);
        const int y = qBound(int(work.top), top, int(work.bottom) - px(m_Main.size.height()));
        QRect main(x, y, px(mainWidth), px(m_Main.size.height())), sub;
        if (m_Page != Page::Main) {
            populate(m_Sub, m_Page);
            m_Sub.measure(subWidth, maxHeight);
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

        // A union of rounded regions leaves both the gap and the space below the
        // shorter panel click-through to the stream or desktop.
        const QRect popup = main.united(sub);
        if (!main.isEmpty()) main.translate(-popup.topLeft());
        if (!sub.isEmpty()) sub.translate(-popup.topLeft());
        if (main != m_Main.frame || sub != m_Sub.frame) {
            m_Main.frame = main;
            m_Sub.frame = sub;
            HRGN region = CreateRectRgn(0, 0, 0, 0);
            for (const QRect& rect : {main, sub}) {
                if (rect.isEmpty()) continue;
                HRGN part = CreateRoundRectRgn(rect.left(), rect.top(), rect.right() + 2, rect.bottom() + 2, px(14), px(14));
                CombineRgn(region, region, part, RGN_OR);
                DeleteObject(part);
            }
            if (!SetWindowRgn(m_Menu, region, FALSE)) DeleteObject(region);
        }
        place(m_Menu, popup.x(), popup.y(), popup.width(), popup.height());
    }

    void showPage(Page page)
    {
        KillTimer(m_Menu, SubmenuTimer);
        if (page == m_Page) return;
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
            m_HostMuteState.reset();
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
    HWND create(DWORD extra, bool menu = false)
    {
        WNDCLASSW wc { };
        wc.lpfnWndProc = proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = menu ? L"MoonlightControlCenterMenu" : L"MoonlightControlCenter";
        wc.style = menu ? CS_DROPSHADOW : 0;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | extra, wc.lpszClassName, L"", WS_POPUP, 0, 0,
            0, 0, m_Owner, nullptr, wc.hInstance, this);
    }
    void place(HWND hwnd, int x, int y, int w, int h)
    {
        if (!hwnd || w <= 0 || h <= 0)
            return;
        RECT rect { };
        GetWindowRect(hwnd, &rect);
        if (hwnd != m_Menu && (w != rect.right - rect.left || h != rect.bottom - rect.top)) {
            HRGN region = CreateRoundRectRgn(0, 0, w + 1, h + 1, px(12), px(12));
            if (!SetWindowRgn(hwnd, region, FALSE))
                DeleteObject(region);
        }
        if (rect.left != x || rect.top != y || rect.right - rect.left != w || rect.bottom - rect.top != h ||
                !IsWindowVisible(hwnd))
            SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
        InvalidateRect(hwnd, nullptr, FALSE);
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

    void paint(HWND hwnd, HDC dc)
    {
        RECT r{};
        GetClientRect(hwnd, &r);
        QImage image(r.right, r.bottom, QImage::Format_RGB32);
        if (image.isNull()) return;
        image.fill(hwnd == m_Button ? QColor(m_Open || m_ButtonHover ? "#c9cdd1" : "#e2e5e8") : QColor("#f8f9fa"));
        QPainter p(&image);
        if (hwnd == m_Menu) {
            m_Main.paint(p, m_Scale, category(m_Page));
            m_Sub.paint(p, m_Scale, Page::Main);
        } else {
            p.scale(m_Scale, m_Scale);
            p.setFont(CaptionMenu::font(13));
            const int w = qRound(r.right / m_Scale), h = qRound(r.bottom / m_Scale);
            if (hwnd == m_Button) {
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
        panel->hover = -1;
        KillTimer(m_Menu, SubmenuTimer);
        update();
        return true;
    }
    void menuMouse(UINT msg, QPoint pos)
    {
        Panel* panel = panelAt(pos);
        const int row = panel ? panel->rowAt(QPoint(int((pos.x() - panel->frame.x()) / m_Scale),
                                                    int((pos.y() - panel->frame.y()) / m_Scale))) : -1;
        if (msg == WM_LBUTTONDOWN) {
            KillTimer(m_Menu, SubmenuTimer);
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
            KillTimer(m_Menu, SubmenuTimer);
            m_Main.hover = mainHover;
            m_Sub.hover = subHover;
            if (mainHover >= 0 && !m_Inline && !m_PressedPanel) {
                const auto& item = m_Main.rows[mainHover];
                if (item.target != category(m_Page))
                    SetTimer(m_Menu, SubmenuTimer, 170, nullptr);
            }
            InvalidateRect(m_Menu, nullptr, FALSE);
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
        if (msg == WM_MOUSEACTIVATE) {
            // The caption button is a separate no-activate window. Give its
            // owner the same focus a click on the native caption would give it.
            if (hwnd == self->m_Button && HIWORD(lp) == WM_LBUTTONDOWN && self->m_Owner)
                SetForegroundWindow(self->m_Owner);
            return MA_NOACTIVATE;
        }
        if (msg == WM_NCHITTEST && hwnd == self->m_Hud)
            return HTTRANSPARENT;
        if (hwnd == self->m_Menu && msg == WM_TIMER && wp == SubmenuTimer) {
            KillTimer(hwnd, SubmenuTimer);
            if (self->m_Open && self->m_Main.hover >= 0 && !self->m_Inline)
                self->showPage(self->m_Main.rows[self->m_Main.hover].target);
            return 0;
        }
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
                self->m_HostMuteState = self->m_HostMuteResult.get();
                self->m_HostMuteWriting = false;
                if (std::exchange(self->m_HostMuteToggleAfterRead, false) &&
                    self->m_Open && self->m_HostMuteState.has_value()) {
                    self->requestHostMute(!*self->m_HostMuteState);
                }
                if (self->m_Open) self->update();
            }
            return 0;
        }
        if (hwnd == self->m_Menu && msg == WM_TIMER && wp == HostMuteRefreshTimer) {
            if (self->m_Open && self->m_Page == Page::Sound) {
                self->requestHostMute(std::nullopt);
                self->update();
            }
            return 0;
        }
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps { };
            HDC dc = BeginPaint(hwnd, &ps);
            self->paint(hwnd, dc);
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
        if (hwnd == self->m_Menu && msg == WM_MOUSELEAVE) {
            KillTimer(hwnd, SubmenuTimer);
            self->m_PressedPanel = nullptr;
            self->m_Main.hover = self->m_Sub.hover = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
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
    std::function<std::future<QString>()> m_LockHost;
    std::function<std::future<std::optional<bool>>(std::optional<bool>)> m_RequestHostMute;
    const bool m_HostMuteAvailable;
    const QString m_HostUuid;
    const bool m_HostPrivacyAvailable;
    bool m_PrivacyScreen = false;
    const DisconnectLockRequest m_RequestDisconnectLock;
    bool m_LockOnDisconnect = false;
    static constexpr UINT_PTR LockStatusTimer = 1;
    static constexpr UINT_PTR HostMuteResultTimer = 2;
    static constexpr UINT_PTR HostMuteRefreshTimer = 3;
    static constexpr UINT_PTR DisconnectLockTimer = 4;
    static constexpr UINT_PTR SubmenuTimer = 5;
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
    double m_Scale = 1;
    bool m_Open = false, m_ButtonHover = false;
    Page m_Page = Page::Main;
    Panel m_Main, m_Sub;
    bool m_Inline = false;
    Panel* m_PressedPanel = nullptr;
    int m_PressedRow = -1;
    QSet<SDL_Scancode> m_MenuKeys;
    HWND m_Owner = nullptr, m_Button = nullptr, m_Menu = nullptr, m_Hud = nullptr;
    std::array<QString, FieldCount> m_Values;
};
