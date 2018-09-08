#include <Limelight.h>
#include <SDL.h>
#include "streaming/session.hpp"

#include <QtGlobal>

#define VK_0 0x30
#define VK_A 0x41

// These are real Windows VK_* codes
#ifndef VK_F1
#define VK_F1 0x70
#define VK_F13 0x7C
#define VK_NUMPAD0 0x60
#endif

const int SdlInputHandler::k_ButtonMap[] = {
    A_FLAG, B_FLAG, X_FLAG, Y_FLAG,
    BACK_FLAG, SPECIAL_FLAG, PLAY_FLAG,
    LS_CLK_FLAG, RS_CLK_FLAG,
    LB_FLAG, RB_FLAG,
    UP_FLAG, DOWN_FLAG, LEFT_FLAG, RIGHT_FLAG
};

SdlInputHandler::SdlInputHandler(bool multiController)
    : m_LastMouseMotionTime(0),
      m_MultiController(multiController)
{
    // Allow gamepad input when the app doesn't have focus
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    // We need to reinit this each time, since you only get
    // an initial set of gamepad arrival events once per init.
    SDL_assert(!SDL_WasInit(SDL_INIT_GAMECONTROLLER));
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) failed: %s",
                     SDL_GetError());
    }

    if (!m_MultiController) {
        // Player 1 is always present in non-MC mode
        m_GamepadMask = 0x1;
    }
    else {
        // Otherwise, detect gamepads on the fly
        m_GamepadMask = 0;
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

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_assert(!SDL_WasInit(SDL_INIT_GAMECONTROLLER));
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

            // Uncapture the mouse to avoid processing any
            // further keyboard input
            SDL_SetRelativeMouseMode(SDL_FALSE);

            SDL_Event event;

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
            Session::s_ActiveSession->toggleFullscreen();
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
    short xdelta = (short)event->xrel;
    short ydelta = (short)event->yrel;

    // If we're sending more than one motion event per millisecond,
    // delay for 1 ms to allow batching of mouse move events.
    Uint32 currentTime = SDL_GetTicks();
    if (!SDL_TICKS_PASSED(currentTime, m_LastMouseMotionTime + 1)) {
        SDL_Delay(1);
        currentTime = SDL_GetTicks();
    }
    m_LastMouseMotionTime = currentTime;

    // Pump even if we didn't delay since we might get some extra events
    SDL_PumpEvents();

    // Batch all of the pending mouse motion events
    SDL_Event nextEvent;
    while (SDL_PeepEvents(&nextEvent,
                          1,
                          SDL_GETEVENT,
                          SDL_MOUSEMOTION,
                          SDL_MOUSEMOTION) == 1) {
        // In theory, these can overflow but in practice
        // it should be highly unlikely since it would require
        // moving 64K pixels in 1 ms.
        xdelta += nextEvent.motion.xrel;
        ydelta += nextEvent.motion.yrel;
    }

    if (xdelta != 0 || ydelta != 0) {
        LiSendMouseMoveEvent(xdelta, ydelta);
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
            // Signed values have one more negative value than
            // positive value, so inverting the sign on -32768
            // could actually cause the value to overflow and
            // wrap around to be negative again. Avoid that by
            // capping the value at 32767.
            state->lsY = -qMax(event->value, (short)-32767);
            break;
        case SDL_CONTROLLER_AXIS_RIGHTX:
            state->rsX = event->value;
            break;
        case SDL_CONTROLLER_AXIS_RIGHTY:
            state->rsY = -qMax(event->value, (short)-32767);
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
