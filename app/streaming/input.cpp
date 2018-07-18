#include <Limelight.h>
#include <SDL.h>
#include "streaming/session.hpp"

#define VK_0 0x30
#define VK_A 0x41

// These are real Windows VK_* codes
#ifndef VK_F1
#define VK_F1 0x70
#define VK_F13 0x7C
#define VK_NUMPAD1 0x61
#endif

const int SdlInputHandler::k_ButtonMap[] = {
    A_FLAG, B_FLAG, X_FLAG, Y_FLAG,
    BACK_FLAG, SPECIAL_FLAG, PLAY_FLAG,
    LS_CLK_FLAG, RS_CLK_FLAG,
    LB_FLAG, RB_FLAG,
    UP_FLAG, DOWN_FLAG, LEFT_FLAG, RIGHT_FLAG
};

SdlInputHandler::SdlInputHandler(bool multiController)
    : m_MultiController(multiController)
{
    if (!m_MultiController) {
        // Player 1 is always present in non-MC mode
        m_GamepadMask = 0x1;
    }

    SDL_zero(m_GamepadState);
}

SdlInputHandler::~SdlInputHandler()
{
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (m_GamepadState[i].controller != nullptr) {
            SDL_GameControllerClose(m_GamepadState[i].controller);
        }
    }
}

void SdlInputHandler::handleKeyEvent(SDL_KeyboardEvent* event)
{
    short keyCode;
    char modifiers;

    // Check for our special key combos
    if ((event->state == SDL_PRESSED) &&
            (event->keysym.mod & KMOD_CTRL) &&
            (event->keysym.mod & KMOD_ALT) &&
            (event->keysym.mod & KMOD_SHIFT)) {

        // Force raise all keys in the combo to avoid
        // leaving them down after disconnecting
        LiSendKeyboardEvent(0xA0, KEY_ACTION_UP, 0);
        LiSendKeyboardEvent(0xA1, KEY_ACTION_UP, 0);
        LiSendKeyboardEvent(0xA2, KEY_ACTION_UP, 0);
        LiSendKeyboardEvent(0xA3, KEY_ACTION_UP, 0);
        LiSendKeyboardEvent(0xA4, KEY_ACTION_UP, 0);
        LiSendKeyboardEvent(0xA5, KEY_ACTION_UP, 0);

        // Check for quit combo (Ctrl+Alt+Shift+Q)
        if (event->keysym.sym == SDLK_q) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected quit key combo");

            SDL_Event event;

            // Drain the event queue of any additional input
            // that might be processed before our quit message.
            while (SDL_PollEvent(&event));

            // Push a quit event to the main loop
            event.type = SDL_QUIT;
            event.quit.timestamp = SDL_GetTicks();
            SDL_PushEvent(&event);
            return;
        }
        // Check for the unbind combo (Ctrl+Alt+Shift+Z)
        else if (event->keysym.sym == SDLK_z) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected mouse capture toggle combo");
            SDL_SetRelativeMouseMode((SDL_bool)!SDL_GetRelativeMouseMode());
            return;
        }
        // Check for the full-screen combo (Ctrl+Alt+Shift+X)
        else if (event->keysym.sym == SDLK_x) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected full-screen toggle combo");
            if (SDL_GetWindowFlags(Session::s_ActiveSession->m_Window) & SDL_WINDOW_FULLSCREEN) {
                SDL_SetWindowFullscreen(Session::s_ActiveSession->m_Window, 0);
            }
            else {
                SDL_SetWindowFullscreen(Session::s_ActiveSession->m_Window, SDL_WINDOW_FULLSCREEN);
            }
            return;
        }
    }

    if (!SDL_GetRelativeMouseMode()) {
        // Not capturing
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

    // Set keycode
    if (event->keysym.sym >= SDLK_0 && event->keysym.sym <= SDLK_9) {
        keyCode = (event->keysym.sym - SDLK_0) + VK_0;
    }
    else if (event->keysym.sym >= SDLK_a && event->keysym.sym <= SDLK_z) {
        keyCode = (event->keysym.sym - SDLK_a) + VK_A;
    }
    else if (event->keysym.sym >= SDLK_F1 && event->keysym.sym <= SDLK_F12) {
        keyCode = (event->keysym.sym - SDLK_F1) + VK_F1;
    }
    else if (event->keysym.sym >= SDLK_F13 && event->keysym.sym <= SDLK_F24) {
        keyCode = (event->keysym.sym - SDLK_F13) + VK_F13;
    }
    else if (event->keysym.sym >= SDLK_KP_1 && event->keysym.sym <= SDLK_KP_9) {
        // SDL defines SDLK_KP_0 > SDLK_KP_9, so we need to handle that manually
        keyCode = (event->keysym.sym - SDLK_KP_1) + VK_NUMPAD1;
    }
    else {
        switch (event->keysym.sym) {
            case SDLK_BACKSPACE:
                keyCode = 0x08;
                break;
            case SDLK_TAB:
                keyCode = 0x09;
                break;
            case SDLK_CLEAR:
                keyCode = 0x0C;
                break;
            case SDLK_RETURN:
                keyCode = 0x0D;
                break;
            case SDLK_PAUSE:
                keyCode = 0x13;
                break;
            case SDLK_CAPSLOCK:
                keyCode = 0x14;
                break;
            case SDLK_ESCAPE:
                keyCode = 0x1B;
                break;
            case SDLK_SPACE:
                keyCode = 0x20;
                break;
            case SDLK_PAGEUP:
                keyCode = 0x21;
                break;
            case SDLK_PAGEDOWN:
                keyCode = 0x22;
                break;
            case SDLK_END:
                keyCode = 0x23;
                break;
            case SDLK_HOME:
                keyCode = 0x24;
                break;
            case SDLK_LEFT:
                keyCode = 0x25;
                break;
            case SDLK_UP:
                keyCode = 0x26;
                break;
            case SDLK_RIGHT:
                keyCode = 0x27;
                break;
            case SDLK_DOWN:
                keyCode = 0x28;
                break;
            case SDLK_SELECT:
                keyCode = 0x29;
                break;
            case SDLK_EXECUTE:
                keyCode = 0x2B;
                break;
            case SDLK_PRINTSCREEN:
                keyCode = 0x2C;
                break;
            case SDLK_INSERT:
                keyCode = 0x2D;
                break;
            case SDLK_DELETE:
                keyCode = 0x2E;
                break;
            case SDLK_HELP:
                keyCode = 0x2F;
                break;
            case SDLK_KP_0:
                // See comment above about why we only handle KP_0 here
                keyCode = 0x60;
                break;
            case SDLK_KP_MULTIPLY:
                keyCode = 0x6A;
                break;
            case SDLK_KP_PLUS:
                keyCode = 0x6B;
                break;
            case SDLK_KP_COMMA:
                keyCode = 0x6C;
                break;
            case SDLK_KP_MINUS:
                keyCode = 0x6D;
                break;
            case SDLK_KP_DECIMAL:
                keyCode = 0x6E;
                break;
            case SDLK_KP_DIVIDE:
                keyCode = 0x6F;
                break;
            case SDLK_NUMLOCKCLEAR:
                keyCode = 0x90;
                break;
            case SDLK_SCROLLLOCK:
                keyCode = 0x91;
                break;
            case SDLK_LSHIFT:
                keyCode = 0xA0;
                break;
            case SDLK_RSHIFT:
                keyCode = 0xA1;
                break;
            case SDLK_LCTRL:
                keyCode = 0xA2;
                break;
            case SDLK_RCTRL:
                keyCode = 0xA3;
                break;
            case SDLK_LALT:
                keyCode = 0xA4;
                break;
            case SDLK_RALT:
                keyCode = 0xA5;
                break;
            case SDLK_AC_BACK:
                keyCode = 0xA6;
                break;
            case SDLK_AC_FORWARD:
                keyCode = 0xA7;
                break;
            case SDLK_AC_REFRESH:
                keyCode = 0xA8;
                break;
            case SDLK_AC_STOP:
                keyCode = 0xA9;
                break;
            case SDLK_AC_SEARCH:
                keyCode = 0xAA;
                break;
            case SDLK_AC_BOOKMARKS:
                keyCode = 0xAB;
                break;
            case SDLK_AC_HOME:
                keyCode = 0xAC;
                break;
            case SDLK_SEMICOLON:
                keyCode = 0xBA;
                break;
            case SDLK_PLUS:
                keyCode = 0xBB;
                break;
            case SDLK_COMMA:
                keyCode = 0xBC;
                break;
            case SDLK_MINUS:
                keyCode = 0xBD;
                break;
            case SDLK_PERIOD:
                keyCode = 0xBE;
                break;
            case SDLK_SLASH:
                keyCode = 0xBF;
                break;
            case SDLK_BACKQUOTE:
                keyCode = 0xC0;
                break;
            case SDLK_LEFTBRACKET:
                keyCode = 0xDB;
                break;
            case SDLK_BACKSLASH:
                keyCode = 0xDC;
                break;
            case SDLK_RIGHTBRACKET:
                keyCode = 0xDD;
                break;
            case SDLK_QUOTE:
                keyCode = 0xDE;
                break;
            default:
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Unhandled button event: %d",
                             event->keysym.sym);
                return;
        }
    }

    LiSendKeyboardEvent(keyCode,
                        event->state == SDL_PRESSED ?
                            KEY_ACTION_DOWN : KEY_ACTION_UP,
                        modifiers);
}

void SdlInputHandler::handleMouseButtonEvent(SDL_MouseButtonEvent* event)
{
    int button;

    // Capture the mouse again if clicked when unbound
    if (event->button == SDL_BUTTON_LEFT &&
            event->state == SDL_PRESSED &&
            !SDL_GetRelativeMouseMode()) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        return;
    }
    else if (!SDL_GetRelativeMouseMode()) {
        // Not capturing
        return;
    }

    switch (event->button)
    {
        case SDL_BUTTON_LEFT:
            button = BUTTON_LEFT;
            break;
        case SDL_BUTTON_MIDDLE:
            button = BUTTON_MIDDLE;
            break;
        case SDL_BUTTON_RIGHT:
            button = BUTTON_RIGHT;
            break;
        case SDL_BUTTON_X1:
        case SDL_BUTTON_X2:
            // Unsupported by GameStream
            return;
        default:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Unhandled button event: %d",
                        event->button);
            return;
    }

    LiSendMouseButtonEvent(event->state == SDL_PRESSED ?
                               BUTTON_ACTION_PRESS :
                               BUTTON_ACTION_RELEASE,
                           button);
}

void SdlInputHandler::handleMouseMotionEvent(SDL_MouseMotionEvent* event)
{
    if (!SDL_GetRelativeMouseMode()) {
        // Not capturing
        return;
    }

    if (event->xrel != 0 || event->yrel != 0) {
        LiSendMouseMoveEvent((unsigned short)event->xrel,
                             (unsigned short)event->yrel);
    }
}

void SdlInputHandler::handleMouseWheelEvent(SDL_MouseWheelEvent* event)
{
    if (!SDL_GetRelativeMouseMode()) {
        // Not capturing
        return;
    }

    if (event->y != 0) {
        LiSendScrollEvent((signed char)event->y);
    }
}

GamepadState*
SdlInputHandler::findStateForGamepad(SDL_JoystickID id)
{
    int i;

    for (i = 0; i < MAX_GAMEPADS; i++) {
        if (m_GamepadState[i].jsId == id) {
            SDL_assert(!m_MultiController || m_GamepadState[i].index == i);
            return &m_GamepadState[i];
        }
    }

    // This should only happen with > 4 gamepads
    SDL_assert(SDL_NumJoysticks() > 4);
    return nullptr;
}

void SdlInputHandler::sendGamepadState(GamepadState* state)
{
    SDL_assert(m_GamepadMask == 0x1 || m_MultiController);
    LiSendMultiControllerEvent(state->index,
                               m_GamepadMask,
                               state->buttons,
                               state->lt,
                               state->rt,
                               state->lsX,
                               state->lsY,
                               state->rsX,
                               state->rsY);
}

void SdlInputHandler::handleControllerAxisEvent(SDL_ControllerAxisEvent* event)
{
    GamepadState* state = findStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    switch (event->axis)
    {
        case SDL_CONTROLLER_AXIS_LEFTX:
            state->lsX = event->value;
            break;
        case SDL_CONTROLLER_AXIS_LEFTY:
            state->lsY = -event->value;
            break;
        case SDL_CONTROLLER_AXIS_RIGHTX:
            state->rsX = event->value;
            break;
        case SDL_CONTROLLER_AXIS_RIGHTY:
            state->rsY = -event->value;
            break;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
            state->lt = (unsigned char)(event->value * 255UL / 32767);
            break;
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
            state->rt = (unsigned char)(event->value * 255UL / 32767);
            break;
        default:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Unhandled controller axis: %d",
                        event->axis);
            return;
    }

    sendGamepadState(state);
}

void SdlInputHandler::handleControllerButtonEvent(SDL_ControllerButtonEvent* event)
{
    GamepadState* state = findStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    if (event->state == SDL_PRESSED) {
        state->buttons |= k_ButtonMap[event->button];
    }
    else {
        state->buttons &= ~k_ButtonMap[event->button];
    }

    sendGamepadState(state);
}

void SdlInputHandler::handleControllerDeviceEvent(SDL_ControllerDeviceEvent* event)
{
    GamepadState* state;

    if (event->type == SDL_CONTROLLERDEVICEADDED) {
        int i;
        const char* name;

        for (i = 0; i < MAX_GAMEPADS; i++) {
            if (m_GamepadState[i].controller == NULL) {
                // Found an empty slot
                break;
            }
        }

        if (i == MAX_GAMEPADS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "No open gamepad slots found!");
            return;
        }

        state = &m_GamepadState[i];
        if (m_MultiController) {
            state->index = i;
        }
        else {
            // Always player 1 in single controller mode
            state->index = 0;
        }
        state->controller = SDL_GameControllerOpen(event->which);
        if (state->controller == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to open gamepad: %s",
                         SDL_GetError());
            return;
        }

        state->jsId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(state->controller));

        name = SDL_GameControllerName(state->controller);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Gamepad %d (player %d) is: %s",
                    i,
                    state->index,
                    name != NULL ? name : "<null>");
        
        // Add this gamepad to the gamepad mask
        if (m_MultiController) {
            SDL_assert(!(m_GamepadMask & (1 << state->index)));
            m_GamepadMask |= (1 << state->index);
        }
        else {
            SDL_assert(m_GamepadMask == 0x1);
        }

        // Send an empty event to tell the PC we've arrived
        sendGamepadState(state);
    }
    else if (event->type == SDL_CONTROLLERDEVICEREMOVED) {
        state = findStateForGamepad(event->which);
        if (state != NULL) {
            SDL_GameControllerClose(state->controller);

            // Remove this from the gamepad mask in MC-mode
            if (m_MultiController) {
                SDL_assert(m_GamepadMask & (1 << state->index));
                m_GamepadMask &= ~(1 << state->index);
            }
            else {
                SDL_assert(m_GamepadMask == 0x1);
            }

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Gamepad %d is gone",
                        state->index);

            // Send a final event to let the PC know this gamepad is gone
            LiSendMultiControllerEvent(state->index, m_GamepadMask,
                                       0, 0, 0, 0, 0, 0, 0);

            // Clear all remaining state from this slot
            SDL_memset(state, 0, sizeof(*state));
        }
    }
}

int SdlInputHandler::getAttachedGamepadMask()
{
    int i;
    int count;
    int mask;

    if (!m_MultiController) {
        // Player 1 is always present in non-MC mode
        return 0x1;
    }

    count = mask = 0;
    for (i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            mask |= (1 << count++);
        }
    }

    return mask;
}
