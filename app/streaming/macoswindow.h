#pragma once

#include "SDL_compat.h"

namespace MacOSWindow {
    using IsInputCapturedCallback = bool (*)(void* context);

    bool enableFullSizeContentView(SDL_Window* window,
                                   IsInputCapturedCallback isInputCaptured,
                                   void* callbackContext);
    void updateFullSizeContentView(SDL_Window* window);
    void disableFullSizeContentView(SDL_Window* window);
    void notifyInputCaptureChanged(SDL_Window* window);
}
