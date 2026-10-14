#include "macoswindow.h"

#include <SDL_syswm.h>

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr char kFullSizeContentViewStateKey[] = "MoonlightFullSizeContentViewState";

struct FullSizeContentViewState
{
    MacOSWindow::IsInputCapturedCallback isInputCaptured;
    void* callbackContext;
    int titleBarHeight;
};

NSWindow* getNativeWindow(SDL_Window* window)
{
    SDL_SysWMinfo windowInfo;
    SDL_VERSION(&windowInfo.version);

    if (!SDL_GetWindowWMInfo(window, &windowInfo)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL_GetWindowWMInfo() failed while configuring the macOS title bar: %s",
                    SDL_GetError());
        return nil;
    }

    if (windowInfo.subsystem != SDL_SYSWM_COCOA) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Cannot configure a full-size content view for a non-Cocoa SDL window");
        return nil;
    }

    return windowInfo.info.cocoa.window;
}

bool applyFullSizeContentView(SDL_Window* window)
{
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
        return true;
    }

    NSWindow* nativeWindow = getNativeWindow(window);
    if (nativeWindow == nil) {
        return false;
    }

    const NSWindowStyleMask styleMask = nativeWindow.styleMask;
    if (!(styleMask & NSWindowStyleMaskTitled)) {
        return true;
    }

    if (!(styleMask & NSWindowStyleMaskFullSizeContentView)) {
        int width;
        int height;
        SDL_GetWindowSize(window, &width, &height);

        NSView* contentView = nativeWindow.contentView;
        NSResponder* nextResponder = contentView.nextResponder;

        // AppKit rewrites the responder chain when the style mask changes.
        contentView.nextResponder = nil;
        nativeWindow.styleMask = styleMask | NSWindowStyleMaskFullSizeContentView;
        contentView.nextResponder = nextResponder;

        // Keep SDL's logical client size unchanged now that it includes the title bar.
        SDL_SetWindowSize(window, width, height);
    }

    nativeWindow.titleVisibility = NSWindowTitleHidden;
    nativeWindow.titlebarAppearsTransparent = YES;
    nativeWindow.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;

    auto state = static_cast<FullSizeContentViewState*>(
                SDL_GetWindowData(window, kFullSizeContentViewStateKey));
    if (state != nullptr) {
        const CGFloat contentHeight = NSHeight(nativeWindow.contentView.bounds);
        const CGFloat layoutHeight = NSHeight(nativeWindow.contentLayoutRect);
        state->titleBarHeight = std::max(
                    0,
                    static_cast<int>(std::ceil(contentHeight - layoutHeight)));
    }

    return true;
}

SDL_HitTestResult SDLCALL titleBarHitTest(SDL_Window* window,
                                          const SDL_Point* point,
                                          void* userData)
{
    auto state = static_cast<FullSizeContentViewState*>(userData);
    if (state == nullptr || state->isInputCaptured(state->callbackContext) ||
            (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)) {
        return SDL_HITTEST_NORMAL;
    }

    return point->y < state->titleBarHeight ?
                SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}

}

bool MacOSWindow::enableFullSizeContentView(SDL_Window* window,
                                            IsInputCapturedCallback isInputCaptured,
                                            void* callbackContext)
{
    if (isInputCaptured == nullptr) {
        return false;
    }

    if (!applyFullSizeContentView(window)) {
        return false;
    }

    NSWindow* nativeWindow = getNativeWindow(window);
    if (nativeWindow == nil) {
        return false;
    }

    const CGFloat contentHeight = NSHeight(nativeWindow.contentView.bounds);
    const CGFloat layoutHeight = NSHeight(nativeWindow.contentLayoutRect);
    auto state = new FullSizeContentViewState {
        isInputCaptured,
        callbackContext,
        std::max(0,
                 static_cast<int>(std::ceil(contentHeight - layoutHeight))),
    };
    SDL_SetWindowData(window, kFullSizeContentViewStateKey, state);

    if (SDL_SetWindowHitTest(window, titleBarHitTest, state) < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL_SetWindowHitTest() failed for the macOS title bar: %s",
                    SDL_GetError());
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Enabled full-size content view for the macOS streaming window");
    return true;
}

void MacOSWindow::updateFullSizeContentView(SDL_Window* window)
{
    applyFullSizeContentView(window);
}

void MacOSWindow::disableFullSizeContentView(SDL_Window* window)
{
    SDL_SetWindowHitTest(window, nullptr, nullptr);

    auto state = static_cast<FullSizeContentViewState*>(
                SDL_SetWindowData(window, kFullSizeContentViewStateKey, nullptr));
    delete state;
}
