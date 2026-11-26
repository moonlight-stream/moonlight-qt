#include "streamrecoverywindow.h"
#include "session.h"
#include "windowcaption.h"

#include <SDL_syswm.h>
#include <QCoreApplication>
#include <algorithm>
#include <utility>

StreamRecoveryWindow::StreamRecoveryWindow(SDL_Window* window, HWND owner, Session* session) :
    m_Window(window), m_Owner(owner), m_Session(session)
{}

std::unique_ptr<StreamRecoveryWindow> StreamRecoveryWindow::hold(SDL_Window* window, Session* session, HWND groupedWindow)
{
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) || SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) return {};
    const HWND owner = info.info.win.window;
    auto view = std::unique_ptr<StreamRecoveryWindow>(new StreamRecoveryWindow(window, owner, session));
    const HWND front = !IsWindowVisible(owner) && IsWindowVisible(groupedWindow) ? groupedWindow : owner;
    view->capture(front);
    view->m_Cover = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_CLIPCHILDREN | SS_NOTIFY,
                                     0, 0, 0, 0, owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!view->m_Cover || !SetWindowSubclass(view->m_Cover, coverProc, 0, reinterpret_cast<DWORD_PTR>(view.get())) ||
            !SetWindowSubclass(owner, ownerProc, 0, reinterpret_cast<DWORD_PTR>(view.get()))) {
        view->releaseToStream();
        return {};
    }
    const auto cancel = QCoreApplication::translate("StreamSegue", "Cancel reconnection").toStdWString();
    view->m_Button = CreateWindowExW(0, L"BUTTON", cancel.c_str(), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                                     0, 0, 0, 0, view->m_Cover, reinterpret_cast<HMENU>(IDCANCEL), GetModuleHandleW(nullptr), nullptr);
    if (!view->m_Button) {
        view->releaseToStream();
        return {};
    }
    view->layout();
    ShowWindow(view->m_Cover, SW_SHOWNOACTIVATE);
    UpdateWindow(view->m_Cover);
    // A docked display can occupy the shared window while its primary is hidden.
    if (front != owner) {
        WINDOWPLACEMENT placement{sizeof(placement)};
        if (GetWindowPlacement(front, &placement)) {
            if (placement.showCmd == SW_SHOWNORMAL) placement.showCmd = SW_SHOWNOACTIVATE;
            SetWindowPlacement(owner, &placement);
        }
    }
    if (!IsWindowVisible(owner)) ShowWindow(owner, SW_SHOWNA);
    DwmFlush();
    return view;
}

void StreamRecoveryWindow::capture(HWND source)
{
    RECT area{};
    if (IsIconic(source) || !GetClientRect(source, &area) || IsRectEmpty(&area)) return;
    // Capture only the stream's own composed client area, including GPU-rendered video.
    HDC sourceDC = GetDC(source);
    if (!sourceDC) return;
    m_FrameDC = CreateCompatibleDC(sourceDC);
    m_Frame = CreateCompatibleBitmap(sourceDC, area.right, area.bottom);
    ReleaseDC(source, sourceDC);
    if (m_FrameDC && m_Frame) {
        m_OldBitmap = SelectObject(m_FrameDC, m_Frame);
        if (PrintWindow(source, m_FrameDC, PW_CLIENTONLY | PW_RENDERFULLCONTENT))
            m_FrameSize = {area.right, area.bottom};
    }
}

StreamRecoveryWindow::~StreamRecoveryWindow()
{
    if (m_Owner) RemoveWindowSubclass(m_Owner, ownerProc, 0);
    if (m_Cover) DestroyWindow(m_Cover);
    if (m_OldBitmap) SelectObject(m_FrameDC, m_OldBitmap);
    if (m_Frame) DeleteObject(m_Frame);
    if (m_FrameDC) DeleteDC(m_FrameDC);
    if (m_Font) DeleteObject(m_Font);
    if (m_Window) SDL_DestroyWindow(m_Window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void StreamRecoveryWindow::releaseToStream()
{
    m_Window = nullptr;
}

void StreamRecoveryWindow::showAttempt(Session* session, const QString& status)
{
    m_Session = session;
    m_Status = status;
    SDL_SetWindowTitle(m_Window, status.toUtf8().constData());
    layout();
    ShowWindow(m_Button, SW_SHOWNOACTIVATE);
    InvalidateRect(m_Cover, &m_Panel, FALSE);
}

void StreamRecoveryWindow::layout()
{
    RECT area{};
    GetClientRect(m_Owner, &area);
    SetWindowPos(m_Cover, HWND_TOP, 0, 0, area.right, area.bottom, SWP_NOACTIVATE);
    const auto dpi = windowDpi(m_Owner);
    const auto px = [dpi](int value) { return MulDiv(value, dpi, 96); };
    if (dpi != m_Dpi) {
        auto font = CreateFontW(-px(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SendMessageW(m_Button, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        if (m_Font) DeleteObject(m_Font);
        m_Font = font;
        m_Dpi = dpi;
    }
    const int width = std::max(0, std::min(int(area.right) - px(12), px(378)));
    const int x = (area.right - width) / 2;
    const int y = std::max(0, int(area.bottom) - px(68));
    m_Panel = {x, y, x + width, y + px(44)};
    const int button = std::min(px(108), width / 3);
    m_Label = {x + px(12), y, m_Panel.right - button - px(16), m_Panel.bottom};
    SetWindowPos(m_Button, nullptr, m_Panel.right - button - px(8), y + px(7), button, px(30), SWP_NOACTIVATE | SWP_NOZORDER);
    InvalidateRect(m_Cover, nullptr, FALSE);
}

void StreamRecoveryWindow::paint(HDC dc)
{
    RECT area{};
    GetClientRect(m_Cover, &area);
    if (m_FrameSize.cx && m_FrameSize.cy) {
        const auto size = QSize(m_FrameSize.cx, m_FrameSize.cy).scaled(area.right, area.bottom, Qt::KeepAspectRatio);
        const int x = (area.right - size.width()) / 2, y = (area.bottom - size.height()) / 2;
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchBlt(dc, x, y, size.width(), size.height(), m_FrameDC, 0, 0, m_FrameSize.cx, m_FrameSize.cy, SRCCOPY);
        if (size.width() != area.right || size.height() != area.bottom) {
            const int saved = SaveDC(dc);
            ExcludeClipRect(dc, x, y, x + size.width(), y + size.height());
            FillRect(dc, &area, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            RestoreDC(dc, saved);
        }
    } else {
        FillRect(dc, &area, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    }
    if (!m_Status.isEmpty()) {
        SetDCBrushColor(dc, RGB(34, 43, 55));
        FillRect(dc, &m_Panel, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        const auto previousFont = SelectObject(dc, m_Font);
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, reinterpret_cast<LPCWSTR>(m_Status.utf16()), m_Status.size(), &m_Label,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc, previousFont);
    }
}

void StreamRecoveryWindow::cancel()
{
    if (m_Session) {
        auto* session = std::exchange(m_Session, nullptr);
        EnableWindow(m_Button, FALSE);
        session->interrupt();
    }
}

LRESULT CALLBACK StreamRecoveryWindow::ownerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data)
{
    auto* self = reinterpret_cast<StreamRecoveryWindow*>(data);
    if (msg == WM_CLOSE || (msg == WM_KEYDOWN && wp == VK_ESCAPE)) {
        self->cancel();
        return 0;
    }
    if (msg == WM_SIZE) self->layout();
    if (msg == WM_NCDESTROY) self->m_Owner = nullptr;
    return DefSubclassProc(hwnd, msg, wp, lp);
}

LRESULT CALLBACK StreamRecoveryWindow::coverProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data)
{
    auto* self = reinterpret_cast<StreamRecoveryWindow*>(data);
    if (msg == WM_COMMAND && reinterpret_cast<HWND>(lp) == self->m_Button && HIWORD(wp) == BN_CLICKED) {
        self->cancel();
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{};
        self->paint(BeginPaint(hwnd, &ps));
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_PRINTCLIENT) {
        self->paint(reinterpret_cast<HDC>(wp));
        return 0;
    }
    if (msg == WM_NCDESTROY) self->m_Cover = self->m_Button = nullptr;
    return DefSubclassProc(hwnd, msg, wp, lp);
}
