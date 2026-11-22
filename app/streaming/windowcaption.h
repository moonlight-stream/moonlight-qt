#pragma once

#include <windows.h>
#include <dwmapi.h>

inline void applyLightWindowCaption(HWND window)
{
    const BOOL dark = FALSE;
    const COLORREF caption = RGB(239, 244, 247), text = RGB(32, 54, 78);
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    DwmSetWindowAttribute(window, DWMWA_TEXT_COLOR, &text, sizeof(text));
}
