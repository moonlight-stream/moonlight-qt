#pragma once

#include "video/clipboarduploadoverlay.h"
#include <windows.h>
#include <windowsx.h>
#include <cstring>

// The clipboard STA owns this non-activating window. The stream overlay also
// paints the desktop card, while Explorer continues to own keyboard focus.
class DesktopTransferProgress
{
public:
    ~DesktopTransferProgress() { if (window) DestroyWindow(window); }

    void show(HWND folder)
    {
        if (!window) {
            WNDCLASSW cls{};
            cls.hInstance = GetModuleHandleW(nullptr);
            cls.lpfnWndProc = procedure;
            cls.lpszClassName = L"MoonlightDesktopTransfer";
            cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
            RegisterClassW(&cls);
            window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                                     cls.lpszClassName, L"Moonlight", WS_POPUP, 0, 0, 1, 1,
                                     nullptr, nullptr, cls.hInstance, this);
            if (!window) return;
        }

        MONITORINFO monitor{sizeof(monitor)};
        if (!GetMonitorInfoW(MonitorFromWindow(folder, MONITOR_DEFAULTTONEAREST), &monitor)) return;
        work = monitor.rcWork;
        const UINT dpi = GetDpiForWindow(folder);
        scale = dpi ? dpi / 96.0 : 1.0;
        KillTimer(window, Dismiss);
        progress = {};
        progress.taskId = ++sequence;
        progress.downloading = true;
        cancelled = false;
        pressed = Header::None;
        userPosition.reset();
        card.setHeaderHovered(Header::None);
        draw(true);
        ShowWindow(window, SW_SHOWNOACTIVATE);
        SetTimer(window, Refresh, 80, nullptr);
    }

    void update(const ClipboardUploadProgress& next)
    {
        progress = next;
        progress.taskId = sequence;
    }

    void finish(bool ok)
    {
        if (!window) return;
        KillTimer(window, Refresh);
        progress.state = ok ? State::Saved : cancelled ? State::Cancelled : State::Failed;
        // The terminal card shows the outcome. Queued names no longer describe active work.
        progress.names.clear();
        progress.index = 0;
        draw(true);
        SetTimer(window, Dismiss, ok ? 1800 : 4000, nullptr);
    }

    bool visible() const { return window && IsWindowVisible(window); }
    bool cancelled = false;

private:
    using State = ClipboardUploadProgress::State;
    using Header = ClipboardUploadOverlay::Header;
    enum Timer { Refresh = 1, Dismiss };

    void draw(bool force)
    {
        const QSize area(work.right - work.left, work.bottom - work.top);
        if (!card.updateTasks({progress}, area, scale) && !force) return;
        const QImage image = card.desktopImage();
        if (image.isNull()) return;

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = image.width();
        info.bmiHeader.biHeight = -image.height();
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc) return;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (bitmap) {
            std::memcpy(pixels, image.constBits(), image.sizeInBytes());
            HGDIOBJ previous = SelectObject(dc, bitmap);
            const int margin = qRound(12 * scale);
            POINT position{work.left + margin, work.bottom - margin - image.height()};
            if (userPosition) {
                position.x = qBound(work.left, userPosition->x, qMax(work.left, work.right - image.width()));
                position.y = qBound(work.top, userPosition->y, qMax(work.top, work.bottom - image.height()));
            }
            POINT origin{};
            SIZE size{image.width(), image.height()};
            BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
            UpdateLayeredWindow(window, nullptr, &position, &size, dc, &origin, 0, &blend, ULW_ALPHA);
            SelectObject(dc, previous);
            DeleteObject(bitmap);
        }
        DeleteDC(dc);
    }

    Header headerAt(LPARAM point) const
    {
        return card.desktopHeader(QPoint(qFloor(GET_X_LPARAM(point) / scale), qFloor(GET_Y_LPARAM(point) / scale)));
    }

    void moveToPointer(LPARAM point)
    {
        POINT cursor{GET_X_LPARAM(point), GET_Y_LPARAM(point)};
        ClientToScreen(window, &cursor);
        MONITORINFO monitor{sizeof(monitor)};
        if (GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitor)) work = monitor.rcWork;
        userPosition = POINT{cursor.x - grab.x, cursor.y - grab.y};
        draw(true);
    }

    static LRESULT CALLBACK procedure(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
    {
        if (msg == WM_NCCREATE) {
            const auto create = reinterpret_cast<CREATESTRUCTW*>(l);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        const auto self = reinterpret_cast<DesktopTransferProgress*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!self) return DefWindowProcW(hwnd, msg, w, l);
        switch (msg) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_MOUSEMOVE: {
            if (self->pressed == Header::Drag && (w & MK_LBUTTON)) {
                self->moveToPointer(l);
                return 0;
            }
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&track);
            if (self->card.setHeaderHovered(self->headerAt(l))) self->draw(true);
            return 0;
        }
        case WM_MOUSELEAVE:
            if (self->card.setHeaderHovered(Header::None)) self->draw(true);
            return 0;
        case WM_LBUTTONDOWN:
            self->pressed = self->headerAt(l);
            self->grab = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            if (self->pressed != Header::None) SetCapture(hwnd);
            return 0;
        case WM_LBUTTONUP: {
            const Header clicked = self->pressed == self->headerAt(l) ? self->pressed : Header::None;
            if (GetCapture() == hwnd) ReleaseCapture();
            self->pressed = Header::None;
            if (clicked == Header::Toggle) {
                self->card.toggleCollapsed();
                self->draw(true);
            } else if (clicked == Header::Cancel) {
                if (self->progress.state == State::Uploading) self->cancelled = true;
                else ShowWindow(hwnd, SW_HIDE);
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            self->pressed = Header::None;
            return 0;
        case WM_DPICHANGED:
            self->scale = LOWORD(w) / 96.0;
            // Repaint after the move that triggered this message has completed.
            if (self->window) PostMessageW(hwnd, WM_APP, 0, 0);
            return 0;
        case WM_APP:
            self->draw(true);
            return 0;
        case WM_TIMER:
            if (w == Refresh) self->draw(false);
            if (w == Dismiss) {
                KillTimer(hwnd, Dismiss);
                ShowWindow(hwnd, SW_HIDE);
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, w, l);
    }

    HWND window = nullptr;
    RECT work{};
    qreal scale = 1;
    Header pressed = Header::None;
    POINT grab{};
    std::optional<POINT> userPosition;
    quint64 sequence = 0;
    ClipboardUploadProgress progress;
    ClipboardUploadOverlay card;
};
