#pragma once

#include <windows.h>
#include <dwmapi.h>

inline constexpr COLORREF LightWindowCaptionColor = RGB(239, 244, 247);

inline UINT windowDpi(HWND window)
{
    using DpiForWindow = UINT(WINAPI*)(HWND);
    static const auto get = reinterpret_cast<DpiForWindow>(GetProcAddress(GetModuleHandleW(L"user32"), "GetDpiForWindow"));
    const UINT dpi = get ? get(window) : 0;
    return dpi ? dpi : 96;
}

// Most of the native sizing border is outside the visible DWM frame.
// Accept a narrow strip inside the video as well when the pointer is free.
inline LRESULT streamClientEdge(HWND window, LPARAM position)
{
    POINT point{short(LOWORD(position)), short(HIWORD(position))};
    RECT client{};
    if (!GetClientRect(window, &client) || !ScreenToClient(window, &point) || !PtInRect(&client, point))
        return HTCLIENT;

    const int grip = MulDiv(4, windowDpi(window), 96);
    const bool left = point.x < grip;
    const bool right = point.x >= client.right - grip;
    if (point.y >= client.bottom - grip) {
        if (left) return HTBOTTOMLEFT;
        if (right) return HTBOTTOMRIGHT;
        return HTBOTTOM;
    }
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    return HTCLIENT;
}

inline bool applyLightWindowCaption(HWND window)
{
    const BOOL dark = FALSE;
    const COLORREF text = RGB(32, 54, 78);
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    const auto result = DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR,
                                             &LightWindowCaptionColor, sizeof(LightWindowCaptionColor));
    DwmSetWindowAttribute(window, DWMWA_TEXT_COLOR, &text, sizeof(text));
    return SUCCEEDED(result);
}
