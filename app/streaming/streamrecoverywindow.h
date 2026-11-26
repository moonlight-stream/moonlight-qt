#pragma once

#include <QString>
#include <qt_windows.h>
#include <commctrl.h>
#include <memory>

struct SDL_Window;
class Session;

// Owns the existing SDL window between connection attempts, on the UI thread.
class StreamRecoveryWindow {
public:
    static std::unique_ptr<StreamRecoveryWindow> hold(SDL_Window* window, Session* session, HWND groupedWindow);
    ~StreamRecoveryWindow();
    SDL_Window* window() const { return m_Window; }
    void showAttempt(Session* session, const QString& status);
    void releaseToStream();

private:
    StreamRecoveryWindow(SDL_Window* window, HWND owner, Session* session);
    void capture(HWND source);
    void layout();
    void paint(HDC dc);
    void cancel();
    static LRESULT CALLBACK ownerProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    static LRESULT CALLBACK coverProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

    SDL_Window* m_Window;
    HWND m_Owner, m_Cover = nullptr, m_Button = nullptr;
    Session* m_Session;
    HDC m_FrameDC = nullptr;
    HBITMAP m_Frame = nullptr;
    HGDIOBJ m_OldBitmap = nullptr;
    HFONT m_Font = nullptr;
    UINT m_Dpi = 0;
    SIZE m_FrameSize{};
    RECT m_Panel{}, m_Label{};
    QString m_Status;
};
