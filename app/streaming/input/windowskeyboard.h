#pragma once

#include "SDL_compat.h"

#ifdef Q_OS_WIN32
#include <SDL_syswm.h>
#include <imm.h>
#endif

namespace WindowsKeyboardInput {

inline void disableLocalIme(SDL_Window* window)
{
#ifdef Q_OS_WIN32
    if (!window) {
        return;
    }

    SDL_SysWMinfo info = {};
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_WINDOWS) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Unable to get streaming HWND: %s", SDL_GetError());
        return;
    }

    // SDL3 initializes IMM against the first window (Moonlight's hidden
    // decoder test window) and IME_Disable() only disassociates that first
    // HWND. Subsequent or recreated streaming windows keep a local HIMC even
    // after SDL_StopTextInput(), allowing the client IME to intercept and
    // corrupt keystrokes meant for the host. Detach the local IME directly
    // from the streaming HWND.
    if (!ImmAssociateContextEx(info.info.win.window, nullptr, 0)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Unable to disable local streaming IME: %lu", GetLastError());
    }
#else
    (void)window;
#endif
}

}
