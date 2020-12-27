#include "streaming/session.h"

#include <Limelight.h>
#include <SDL.h>

#include <QGuiApplication>

// Until we can fully capture these on all platforms (without conflicting with
// OS-provided shortcuts), we should avoid passing them through to the host.
//#define ENABLE_META

#define VK_0 0x30
#define VK_A 0x41

// These are real Windows VK_* codes
#ifndef VK_F1
#define VK_F1 0x70
#define VK_F13 0x7C
#define VK_NUMPAD0 0x60
#endif

void SdlInputHandler::handleKeyEvent(SDL_KeyboardEvent* event)
{
    short keyCode;
    char modifiers;

    // Check for our special key combos
    if ((event->state == SDL_PRESSED) &&
            (event->keysym.mod & KMOD_CTRL) &&
            (event->keysym.mod & KMOD_ALT) &&
            (event->keysym.mod & KMOD_SHIFT)) {
        // First we test the SDLK combos for matches,
        // that way we ensure that latin keyboard users
        // can match to the key they see on their keyboards.
        // If nothing matches that, we'll then go on to
        // checking scancodes so non-latin keyboard users
        // can have working hotkeys (though possibly in
        // odd positions). We must do all SDLK tests before
        // any scancode tests to avoid issues in cases
        // where the SDLK for one shortcut collides with
        // the scancode of another.

        // Check for quit combo (Ctrl+Alt+Shift+Q)
        if (event->keysym.sym == SDLK_q) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected quit key combo (SDLK)");

            // Push a quit event to the main loop
            SDL_Event event;
            event.type = SDL_QUIT;
            event.quit.timestamp = SDL_GetTicks();
            SDL_PushEvent(&event);
            return;
        }
        // Check for the unbind combo (Ctrl+Alt+Shift+Z) unless on EGLFS which has no window manager
        else if (event->keysym.sym == SDLK_z && QGuiApplication::platformName() != "eglfs") {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected mouse capture toggle combo (SDLK)");

            // Stop handling future input
            setCaptureActive(!isCaptureActive());

            // Force raise all keys to ensure they aren't stuck,
            // since we won't get their key up events.
            raiseAllKeys();
            return;
        }
        // Check for the mouse mode combo (Ctrl+Alt+Shift+M) unless on EGLFS which has no window manager
        else if (event->keysym.sym == SDLK_m && QGuiApplication::platformName() != "eglfs") {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected mouse mode toggle combo (SDLK)");

            // Uncapture input
            setCaptureActive(false);

            // Toggle mouse mode
            m_AbsoluteMouseMode = !m_AbsoluteMouseMode;

            // Recapture input
            setCaptureActive(true);
            return;
        }
        else if (event->keysym.sym == SDLK_x && QGuiApplication::platformName() != "eglfs") {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected full-screen toggle combo (SDLK)");
            Session::s_ActiveSession->toggleFullscreen();

            // Force raise all keys just be safe across this full-screen/windowed
            // transition just in case key events get lost.
            raiseAllKeys();
            return;
        }
        // Check for overlay combo (Ctrl+Alt+Shift+S)
        else if (event->keysym.sym == SDLK_s) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected stats toggle combo (SDLK)");

            // Toggle the stats overlay
            Session::get()->getOverlayManager().setOverlayState(Overlay::OverlayDebug,
                                                                !Session::get()->getOverlayManager().isOverlayEnabled(Overlay::OverlayDebug));

            // Force raise all keys just be safe across this full-screen/windowed
            // transition just in case key events get lost.
            raiseAllKeys();
            return;
        }
        // Check for mouse show combo (Ctrl+Alt+Shift+C)
        else if (event->keysym.sym == SDLK_c) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected show mouse combo (SDLK)");

            if (!SDL_GetRelativeMouseMode()) {
                SDL_ShowCursor(!SDL_ShowCursor(SDL_QUERY));
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Cursor can only be shown in remote desktop mouse mode");
            }
            return;
        }
        // Check for quit combo (Ctrl+Alt+Shift+Q)
        else if (event->keysym.scancode == SDL_SCANCODE_Q) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected quit key combo (scancode)");

            // Push a quit event to the main loop
            SDL_Event event;
            event.type = SDL_QUIT;
            event.quit.timestamp = SDL_GetTicks();
            SDL_PushEvent(&event);
            return;
        }
        // Check for the unbind combo (Ctrl+Alt+Shift+Z)
        else if (event->keysym.scancode == SDL_SCANCODE_Z && QGuiApplication::platformName() != "eglfs") {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected mouse capture toggle combo (scancode)");

            // Stop handling future input
            setCaptureActive(!isCaptureActive());

            // Force raise all keys to ensure they aren't stuck,
            // since we won't get their key up events.
            raiseAllKeys();
            return;
        }
        // Check for the full-screen combo (Ctrl+Alt+Shift+X) unless on EGLFS which has no window manager
        else if (event->keysym.scancode == SDL_SCANCODE_X && QGuiApplication::platformName() != "eglfs") {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected full-screen toggle combo (scancode)");
            Session::s_ActiveSession->toggleFullscreen();

            // Force raise all keys just be safe across this full-screen/windowed
            // transition just in case key events get lost.
            raiseAllKeys();
            return;
        }
        // Check for the mouse mode toggle combo (Ctrl+Alt+Shift+M) unless on EGLFS which has no window manager
        else if (event->keysym.scancode == SDL_SCANCODE_M && QGuiApplication::platformName() != "eglfs") {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected mouse mode toggle combo (scancode)");

            // Uncapture input
            setCaptureActive(false);

            // Toggle mouse mode
            m_AbsoluteMouseMode = !m_AbsoluteMouseMode;

            // Recapture input
            setCaptureActive(true);
            return;
        }
        else if (event->keysym.scancode == SDL_SCANCODE_S) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected stats toggle combo (scancode)");

            // Toggle the stats overlay
            Session::get()->getOverlayManager().setOverlayState(Overlay::OverlayDebug,
                                                                !Session::get()->getOverlayManager().isOverlayEnabled(Overlay::OverlayDebug));

            // Force raise all keys just be safe across this full-screen/windowed
            // transition just in case key events get lost.
            raiseAllKeys();
            return;
        }
        // Check for mouse show combo (Ctrl+Alt+Shift+C)
        else if (event->keysym.sym == SDL_SCANCODE_C) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected show mouse combo (scancode)");

            if (!SDL_GetRelativeMouseMode()) {
                SDL_ShowCursor(!SDL_ShowCursor(SDL_QUERY));
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Cursor can only be shown in remote desktop mouse mode");
            }
            return;
        }
    }

    if (event->repeat) {
        // Ignore repeat key down events
        SDL_assert(event->state == SDL_PRESSED);
        return;
    }

    // Set modifier flags
    modifiers = 0;
    if (event->keysym.mod & KMOD_CTRL) {
        modifiers |= MODIFIER_CTRL;
    }
    if (event->keysym.mod & KMOD_ALT) {
        modifiers |= MODIFIER_ALT;
    }
    if (event->keysym.mod & KMOD_SHIFT) {
        modifiers |= MODIFIER_SHIFT;
    }
#ifdef ENABLE_META
    if (event->keysym.mod & KMOD_GUI) {
        modifiers |= MODIFIER_META;
    }
#endif

    // Set keycode. We explicitly use scancode here because GFE will try to correct
    // for AZERTY layouts on the host but it depends on receiving VK_ values matching
    // a QWERTY layout to work.
    if (event->keysym.scancode >= SDL_SCANCODE_1 && event->keysym.scancode <= SDL_SCANCODE_9) {
        // SDL defines SDL_SCANCODE_0 > SDL_SCANCODE_9, so we need to handle that manually
        keyCode = (event->keysym.scancode - SDL_SCANCODE_1) + VK_0 + 1;
    }
    else if (event->keysym.scancode >= SDL_SCANCODE_A && event->keysym.scancode <= SDL_SCANCODE_Z) {
        keyCode = (event->keysym.scancode - SDL_SCANCODE_A) + VK_A;
    }
    else if (event->keysym.scancode >= SDL_SCANCODE_F1 && event->keysym.scancode <= SDL_SCANCODE_F12) {
        keyCode = (event->keysym.scancode - SDL_SCANCODE_F1) + VK_F1;
    }
    else if (event->keysym.scancode >= SDL_SCANCODE_F13 && event->keysym.scancode <= SDL_SCANCODE_F24) {
        keyCode = (event->keysym.scancode - SDL_SCANCODE_F13) + VK_F13;
    }
    else if (event->keysym.scancode >= SDL_SCANCODE_KP_1 && event->keysym.scancode <= SDL_SCANCODE_KP_9) {
        // SDL defines SDL_SCANCODE_KP_0 > SDL_SCANCODE_KP_9, so we need to handle that manually
        keyCode = (event->keysym.scancode - SDL_SCANCODE_KP_1) + VK_NUMPAD0 + 1;
    }
    else {
        switch (event->keysym.scancode) {
            case SDL_SCANCODE_BACKSPACE:
                keyCode = 0x08;
                break;
            case SDL_SCANCODE_TAB:
                keyCode = 0x09;
                break;
            case SDL_SCANCODE_CLEAR:
                keyCode = 0x0C;
                break;
            case SDL_SCANCODE_KP_ENTER: // FIXME: Is this correct?
            case SDL_SCANCODE_RETURN:
                keyCode = 0x0D;
                break;
            case SDL_SCANCODE_PAUSE:
                keyCode = 0x13;
                break;
            case SDL_SCANCODE_CAPSLOCK:
                keyCode = 0x14;
                break;
            case SDL_SCANCODE_ESCAPE:
                keyCode = 0x1B;
                break;
            case SDL_SCANCODE_SPACE:
                keyCode = 0x20;
                break;
            case SDL_SCANCODE_PAGEUP:
                keyCode = 0x21;
                break;
            case SDL_SCANCODE_PAGEDOWN:
                keyCode = 0x22;
                break;
            case SDL_SCANCODE_END:
                keyCode = 0x23;
                break;
            case SDL_SCANCODE_HOME:
                keyCode = 0x24;
                break;
            case SDL_SCANCODE_LEFT:
                keyCode = 0x25;
                break;
            case SDL_SCANCODE_UP:
                keyCode = 0x26;
                break;
            case SDL_SCANCODE_RIGHT:
                keyCode = 0x27;
                break;
            case SDL_SCANCODE_DOWN:
                keyCode = 0x28;
                break;
            case SDL_SCANCODE_SELECT:
                keyCode = 0x29;
                break;
            case SDL_SCANCODE_EXECUTE:
                keyCode = 0x2B;
                break;
            case SDL_SCANCODE_PRINTSCREEN:
                keyCode = 0x2C;
                break;
            case SDL_SCANCODE_INSERT:
                keyCode = 0x2D;
                break;
            case SDL_SCANCODE_DELETE:
                keyCode = 0x2E;
                break;
            case SDL_SCANCODE_HELP:
                keyCode = 0x2F;
                break;
            case SDL_SCANCODE_KP_0:
                // See comment above about why we only handle SDL_SCANCODE_KP_0 here
                keyCode = VK_NUMPAD0;
                break;
            case SDL_SCANCODE_0:
                // See comment above about why we only handle SDL_SCANCODE_0 here
                keyCode = VK_0;
                break;
            case SDL_SCANCODE_KP_MULTIPLY:
                keyCode = 0x6A;
                break;
            case SDL_SCANCODE_KP_PLUS:
                keyCode = 0x6B;
                break;
            case SDL_SCANCODE_KP_COMMA:
                keyCode = 0x6C;
                break;
            case SDL_SCANCODE_KP_MINUS:
                keyCode = 0x6D;
                break;
            case SDL_SCANCODE_KP_PERIOD:
                keyCode = 0x6E;
                break;
            case SDL_SCANCODE_KP_DIVIDE:
                keyCode = 0x6F;
                break;
            case SDL_SCANCODE_NUMLOCKCLEAR:
                keyCode = 0x90;
                break;
            case SDL_SCANCODE_SCROLLLOCK:
                keyCode = 0x91;
                break;
            case SDL_SCANCODE_LSHIFT:
                keyCode = 0xA0;
                break;
            case SDL_SCANCODE_RSHIFT:
                keyCode = 0xA1;
                break;
            case SDL_SCANCODE_LCTRL:
                keyCode = 0xA2;
                break;
            case SDL_SCANCODE_RCTRL:
                keyCode = 0xA3;
                break;
            case SDL_SCANCODE_LALT:
                keyCode = 0xA4;
                break;
            case SDL_SCANCODE_RALT:
                keyCode = 0xA5;
                break;
            #ifdef ENABLE_META
            case SDL_SCANCODE_LGUI:
                keyCode = 0x5B;
                break;
            case SDL_SCANCODE_RGUI:
                keyCode = 0x5C;
                break;
            #endif
            case SDL_SCANCODE_AC_BACK:
                keyCode = 0xA6;
                break;
            case SDL_SCANCODE_AC_FORWARD:
                keyCode = 0xA7;
                break;
            case SDL_SCANCODE_AC_REFRESH:
                keyCode = 0xA8;
                break;
            case SDL_SCANCODE_AC_STOP:
                keyCode = 0xA9;
                break;
            case SDL_SCANCODE_AC_SEARCH:
                keyCode = 0xAA;
                break;
            case SDL_SCANCODE_AC_BOOKMARKS:
                keyCode = 0xAB;
                break;
            case SDL_SCANCODE_AC_HOME:
                keyCode = 0xAC;
                break;
            case SDL_SCANCODE_SEMICOLON:
                keyCode = 0xBA;
                break;
            case SDL_SCANCODE_EQUALS:
                keyCode = 0xBB;
                break;
            case SDL_SCANCODE_COMMA:
                keyCode = 0xBC;
                break;
            case SDL_SCANCODE_MINUS:
                keyCode = 0xBD;
                break;
            case SDL_SCANCODE_PERIOD:
                keyCode = 0xBE;
                break;
            case SDL_SCANCODE_SLASH:
                keyCode = 0xBF;
                break;
            case SDL_SCANCODE_GRAVE:
                keyCode = 0xC0;
                break;
            case SDL_SCANCODE_LEFTBRACKET:
                keyCode = 0xDB;
                break;
            case SDL_SCANCODE_BACKSLASH:
                keyCode = 0xDC;
                break;
            case SDL_SCANCODE_RIGHTBRACKET:
                keyCode = 0xDD;
                break;
            case SDL_SCANCODE_APOSTROPHE:
                keyCode = 0xDE;
                break;
            case SDL_SCANCODE_NONUSBACKSLASH:
                keyCode = 0xE2;
                break;
            default:
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Unhandled button event: %d",
                             event->keysym.scancode);
                return;
        }
    }

    // Track the key state so we always know which keys are down
    if (event->state == SDL_PRESSED) {
        m_KeysDown.insert(keyCode);
    }
    else {
        m_KeysDown.remove(keyCode);
    }

    LiSendKeyboardEvent(keyCode,
                        event->state == SDL_PRESSED ?
                            KEY_ACTION_DOWN : KEY_ACTION_UP,
                        modifiers);
}
