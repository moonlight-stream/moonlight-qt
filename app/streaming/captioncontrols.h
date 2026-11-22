#pragma once

#include <SDL_syswm.h>
#include "windowcaption.h"
#include "input/input.h"
#include <commctrl.h>
#include <QCoreApplication>
#include <QPainter>
#include <QSettings>
#include <QStringList>
#include <array>

// The SDL loop owns these three non-activating windows. Menu input never
// starts a nested message loop or blocks the streaming thread.
class CaptionControls {
public:
    static constexpr int Fullscreen = 111;
    enum Field { Duration, Fps, Network, Decode, Upload, Download, Loss, Resolution, FieldCount };
    CaptionControls(SDL_Window* window, StreamingPreferences& preferences, SdlInputHandler& input)
        : m_Window(window)
        , m_Preferences(preferences)
        , m_Input(input)
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
        m_ButtonHover = false;
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
    void dismiss()
    {
        if (m_Open && m_Button)
            InvalidateRect(m_Button, nullptr, FALSE);
        m_Open = false;
        if (m_Menu)
            ShowWindow(m_Menu, SW_HIDE);
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
            m_Menu = create(0);
            m_Hud = create(WS_EX_LAYERED | WS_EX_TRANSPARENT);
            SetWindowSubclass(owner, ownerProc, reinterpret_cast<UINT_PTR>(this), reinterpret_cast<DWORD_PTR>(this));
        }
        layout(owner);
    }

private:
    // Native move/size loops dispatch window messages while the SDL event loop is blocked.
    static LRESULT CALLBACK ownerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
    {
        if (msg == WM_NCDESTROY)
            RemoveWindowSubclass(hwnd, ownerProc, id);
        LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
        if (msg == WM_WINDOWPOSCHANGED) {
            const auto* pos = reinterpret_cast<const WINDOWPOS*>(lp);
            if ((pos->flags & (SWP_NOMOVE | SWP_NOSIZE)) != (SWP_NOMOVE | SWP_NOSIZE))
                reinterpret_cast<CaptionControls*>(data)->layout(hwnd);
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
            const int inset = px(1);
            place(m_Button, right - px(108), top + inset, px(108), bottom - top - 2 * inset);
            if (m_Open) {
                if (GetForegroundWindow() != owner)
                    dismiss();
                else {
                    const int width = px(m_Page == Security ? 340 : (m_Page == Main || m_Page == SystemKeys ? 300 : 216));
                    const int height = px(12 + rows().size() * 32 + (m_Page == Security ? 102 : 0));
                    RECT work { };
                    MONITORINFO monitor { sizeof(monitor) };
                    GetMonitorInfo(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor);
                    work = monitor.rcWork;
                    place(m_Menu,
                        qBound(int(work.left), right - width, qMax(int(work.left), int(work.right) - width)),
                        qBound(int(work.top), int(origin.y) + 3, qMax(int(work.top), int(work.bottom) - height)),
                        width, height);
                }
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

    enum Page { Main, HudSettings, Fields, Corner, Opacity, Security, SystemKeys };
    static QString tr(const char* s) { return QCoreApplication::translate("StreamControlCenter", s); }
    static QString settingsText(const char* s) { return QCoreApplication::translate("SettingsView", s); }
    int px(int v) const { return qRound(v * m_Scale); }
    QStringList labels() const
    {
        return { tr("Duration"), tr("Frame rate"), tr("Network RTT"), tr("Frame latency"), tr("Adapter upload"),
            tr("Adapter download"), tr("Network frame loss"), tr("Resolution") };
    }
    QStringList rows() const
    {
        switch (m_Page) {
        case Main:
            return { tr("Statistics HUD"), tr("HUD settings"), tr("Security"),
                settingsText("Capture system keyboard shortcuts"), tr("Fullscreen") };
        case SystemKeys:
            return { settingsText("Capture system keyboard shortcuts"), settingsText("Off"),
                settingsText("Fullscreen only"), settingsText("Always") };
        case HudSettings:
            return { tr("HUD settings"), tr("Displayed metrics"), tr("Position"), tr("Opacity") };
        case Security:
            return { tr("Security"), tr("Request screen lock on disconnect"), tr("Use installed host lock shortcut"),
                tr("Request screen lock now") };
        case Fields:
            return QStringList { tr("Displayed metrics") } + labels();
        case Corner:
            return { tr("Position"), tr("Top left"), tr("Top right"), tr("Bottom left"), tr("Bottom right") };
        case Opacity:
            return { tr("Opacity"), "25%", "50%", "75%", "100%" };
        }
        return { };
    }
    void select(int row)
    {
        if (row < 0 || row >= rows().size())
            return;
        if (m_Page == Main) {
            switch (row) {
            case 0:
                toggle();
                break;
            case 1:
                m_Page = HudSettings;
                break;
            case 2:
                m_LockOnDisconnect = m_Preferences.lockHostOnDisconnect();
                m_LockShortcutStatus.clear();
                m_Page = Security;
                break;
            case 3:
                m_Page = SystemKeys;
                break;
            case 4: {
                dismiss();
                SDL_Event e { };
                e.type = SDL_USEREVENT;
                e.user.code = Fullscreen;
                SDL_PushEvent(&e);
                break;
            }
            }
        } else if (row == 0) {
            m_Page = (m_Page == Security || m_Page == HudSettings || m_Page == SystemKeys) ? Main : HudSettings;
        } else if (m_Page == SystemKeys) {
            const auto mode = static_cast<StreamingPreferences::CaptureSysKeysMode>(row - 1);
            m_Input.setCaptureSystemKeysMode(mode);
            m_Preferences.setCaptureSysKeysMode(mode);
        } else if (m_Page == Security && row == 1) {
            m_LockShortcutStatus.clear();
            m_Preferences.setLockHostOnDisconnect(!m_LockOnDisconnect);
            m_LockOnDisconnect = m_Preferences.lockHostOnDisconnect();
        } else if (m_Page == Security && row == 2) {
            m_LockShortcutStatus.clear();
            m_Input.setUsesHostLockShortcut(!m_Input.usesHostLockShortcut());
        } else if (m_Page == Security) {
            m_LockRequestQueued = m_Input.requestHostScreenLock();
            if (m_LockRequestQueued) {
                m_LockShortcutStatus = tr("Lock shortcut queued");
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Queued host screen lock shortcut from control center");
                SetTimer(m_Menu, LockStatusTimer, 3000, nullptr);
            } else {
                m_LockShortcutStatus = tr("Couldn't queue lock shortcut");
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to queue host screen lock shortcut from control center");
                KillTimer(m_Menu, LockStatusTimer);
            }
        } else if (m_Page == HudSettings) {
            constexpr Page pages[] = { Fields, Corner, Opacity };
            m_Page = pages[row - 1];
        } else if (m_Page == Fields) {
            int next = m_Fields ^ (1 << (row - 1));
            if (next) {
                m_Fields = next;
                QSettings().setValue("streamHud/fields", next);
            }
        } else if (m_Page == Corner) {
            m_Corner = row - 1;
            QSettings().setValue("streamHud/corner", m_Corner);
        } else {
            m_Opacity = row;
            QSettings().setValue("streamHud/opacity", row);
        }
        m_Hover = -1;
        update();
    }
    HWND create(DWORD extra)
    {
        WNDCLASSW wc { };
        wc.lpfnWndProc = proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"MoonlightControlCenter";
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
        if (w != rect.right - rect.left || h != rect.bottom - rect.top) {
            HRGN region = CreateRoundRectRgn(0, 0, w + 1, h + 1, px(12), px(12));
            if (!SetWindowRgn(hwnd, region, FALSE))
                DeleteObject(region);
        }
        if (rect.left != x || rect.top != y || rect.right - rect.left != w || rect.bottom - rect.top != h ||
                !IsWindowVisible(hwnd))
            SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    void paint(HWND hwnd, HDC dc)
    {
        RECT r { };
        GetClientRect(hwnd, &r);
        QImage image(r.right, r.bottom, QImage::Format_RGB32);
        if (image.isNull())
            return;
        image.fill(hwnd == m_Button ? QColor(m_Open || m_ButtonHover ? "#dce8f5" : "#eff4f7") : QColor("#f8fafc"));
        QPainter p(&image);
        p.scale(m_Scale, m_Scale);
        QFont font(QStringLiteral("Microsoft YaHei UI"));
        font.setPixelSize(13);
        p.setFont(font);
        int w = qRound(r.right / m_Scale), h = qRound(r.bottom / m_Scale);
        p.setPen(QColor("#20364e"));
        if (hwnd == m_Button) {
            const QString label = tr("Control center");
            int left = qMax(3, (w - p.fontMetrics().boundingRect(label).width() - 22) / 2);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(QColor("#20364e"), 1.3));
            for (int i = 0; i < 2; ++i) {
                QRectF track(left, h * 0.5 - 7 + 8 * i, 14, 6);
                p.drawRoundedRect(track, 3, 3);
                p.fillRect(QRectF(left + (i ? 9 : 3), track.top() + 2, 2, 2), QColor("#20364e"));
            }
            p.drawText(QRect(left + 21, 0, w - left - 23, h), Qt::AlignLeft | Qt::AlignVCenter, label);
        } else if (hwnd == m_Hud) {
            auto names = labels();
            int y = 6;
            for (int i = 0; i < FieldCount; ++i)
                if (m_Fields & (1 << i)) {
                    QRect line(12, y, w - 24, 23);
                    p.setPen(QColor("#52647a"));
                    p.drawText(line, Qt::AlignLeft | Qt::AlignVCenter, names[i]);
                    p.setPen(QColor("#172b45"));
                    p.drawText(line, Qt::AlignRight | Qt::AlignVCenter, m_Values[i]);
                    y += 23;
                }
        } else {
            auto items = rows();
            for (int i = 0; i < items.size(); ++i) {
                QRect line(6, 6 + 32 * i, w - 12, 32);
                if (i == m_Hover)
                    p.fillRect(line, QColor("#e3edfc"));
                bool back = m_Page != Main && i == 0;
                QString mark;
                if ((m_Page == Main && i >= 1 && i <= 3) || (m_Page == HudSettings && i > 0))
                    mark = QStringLiteral("›");
                if ((m_Page == Corner && i == m_Corner + 1) || (m_Page == Opacity && i == m_Opacity)
                    || (m_Page == SystemKeys && i == m_Input.captureSystemKeysMode() + 1)
                    || (m_Page == Fields && i > 0 && (m_Fields & (1 << (i - 1)))))
                    mark = QStringLiteral("✓");
                line.adjust(9, 0, -9, 0);
                if ((m_Page == Main && i == 0) || (m_Page == Security && (i == 1 || i == 2))) {
                    const bool checked = m_Page == Main ? enabled() : (i == 1 ? m_LockOnDisconnect : m_Input.usesHostLockShortcut());
                    const QRect track(line.right() - 29, line.center().y() - 8, 30, 16);
                    p.setRenderHint(QPainter::Antialiasing);
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(checked ? "#2468d5" : "#788c97"));
                    p.drawRoundedRect(track, 8, 8);
                    p.setBrush(Qt::white);
                    p.drawEllipse(track.left() + (checked ? 16 : 2), track.top() + 2, 12, 12);
                    line.setRight(track.left() - 8);
                }
                p.setPen(QColor("#20364e"));
                p.drawText(line, Qt::AlignLeft | Qt::AlignVCenter,
                    (back ? QStringLiteral("‹   ") : QString()) + items[i]);
                p.setPen(QColor("#2468d5"));
                p.drawText(line, Qt::AlignRight | Qt::AlignVCenter, mark);
            }
            if (m_Page == Security) {
                const int top = 6 + items.size() * 32;
                p.setPen(QColor("#dce4ec"));
                p.drawLine(15, top, w - 15, top);
                font.setPixelSize(12);
                p.setFont(font);
                p.setPen(QColor("#52647a"));
                p.drawText(QRect(15, top + 6, w - 30, h - top - 32), Qt::AlignLeft | Qt::TextWordWrap,
                    tr("If Win+L doesn't lock this PC, install the Ctrl+Alt+F10 lock shortcut on the remote PC and enable it above. Press Ctrl+Alt+Shift+End during a stream to request a lock."));
                if (!m_LockShortcutStatus.isEmpty()) {
                    p.setPen(QColor(m_LockRequestQueued ? "#2468d5" : "#a13726"));
                    p.drawText(QRect(15, h - 27, w - 30, 21), Qt::AlignLeft | Qt::AlignVCenter, m_LockShortcutStatus);
                }
            }
        }
        p.end();
        BITMAPINFO bi { };
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = image.width();
        bi.bmiHeader.biHeight = -image.height();
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        SetDIBitsToDevice(dc, 0, 0, image.width(), image.height(), 0, 0, 0, image.height(), image.constBits(), &bi,
            DIB_RGB_COLORS);
    }
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        if (msg == WM_NCCREATE)
            SetWindowLongPtr(hwnd, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams));
        auto* self = reinterpret_cast<CaptionControls*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (!self)
            return DefWindowProcW(hwnd, msg, wp, lp);
        if (msg == WM_MOUSEACTIVATE)
            return MA_NOACTIVATE;
        if (msg == WM_NCHITTEST && hwnd == self->m_Hud)
            return HTTRANSPARENT;
        if (hwnd == self->m_Menu && msg == WM_TIMER && wp == LockStatusTimer) {
            KillTimer(hwnd, LockStatusTimer);
            if (self->m_LockRequestQueued) {
                self->m_LockShortcutStatus.clear();
                InvalidateRect(hwnd, nullptr, FALSE);
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
        if (hwnd == self->m_Menu && (msg == WM_MOUSEMOVE || msg == WM_LBUTTONUP)) {
            int y = static_cast<int>(static_cast<short>(HIWORD(lp)) / self->m_Scale) - 6;
            int row = y < 0 ? -1 : y / 32;
            if (msg == WM_LBUTTONUP)
                self->select(row);
            else if (row != self->m_Hover) {
                self->m_Hover = row;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
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
                self->m_Open = true;
                self->m_Page = Main;
                self->m_Hover = -1;
                self->update();
                SetWindowPos(self->m_Menu, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    SDL_Window* m_Window;
    StreamingPreferences& m_Preferences;
    SdlInputHandler& m_Input;
    bool m_LockOnDisconnect = false;
    static constexpr UINT_PTR LockStatusTimer = 1;
    bool m_LockRequestQueued = false;
    QString m_LockShortcutStatus;
    int m_Fields, m_Corner, m_Opacity, m_Hover = -1;
    double m_Scale = 1;
    bool m_Open = false, m_ButtonHover = false;
    Page m_Page = Main;
    HWND m_Owner = nullptr, m_Button = nullptr, m_Menu = nullptr, m_Hud = nullptr;
    std::array<QString, FieldCount> m_Values;
};
