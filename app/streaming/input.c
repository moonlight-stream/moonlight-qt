#include <Limelight.h>
#include <SDL.h>
#include <stdbool.h>

#define VK_0 0x30
#define VK_A 0x41

// These are real Windows VK_* codes
#ifndef VK_F1
#define VK_F1 0x70
#define VK_F13 0x7C
#define VK_NUMPAD1 0x61
#endif

typedef struct _GAMEPAD_STATE {
    SDL_GameController* controller;
    SDL_JoystickID jsId;
    short index;

    short buttons;
    short lsX, lsY;
    short rsX, rsY;
    unsigned char lt, rt;
} GAMEPAD_STATE, *PGAMEPAD_STATE;

#define MAX_GAMEPADS 4
GAMEPAD_STATE g_GamepadState[MAX_GAMEPADS];
unsigned short g_GamepadMask;
bool g_MultiController;

const short k_ButtonMap[] = {
    A_FLAG, B_FLAG, X_FLAG, Y_FLAG,
    BACK_FLAG, SPECIAL_FLAG, PLAY_FLAG,
    LS_CLK_FLAG, RS_CLK_FLAG,
    LB_FLAG, RB_FLAG,
    UP_FLAG, DOWN_FLAG, LEFT_FLAG, RIGHT_FLAG
};

void SdlHandleKeyEvent(SDL_KeyboardEvent* event)
{
    short keyCode;
    char modifiers;

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

    // Ignore repeats
    if (event->repeat != 0) {
        return;
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

void SdlHandleMouseButtonEvent(SDL_MouseButtonEvent* event)
{
    int button;

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

void SdlHandleMouseMotionEvent(SDL_MouseMotionEvent* event)
{
    if (event->xrel != 0 || event->yrel != 0) {
        LiSendMouseMoveEvent((unsigned short)event->xrel,
                             (unsigned short)event->yrel);
    }
}

void SdlHandleMouseWheelEvent(SDL_MouseWheelEvent* event)
{
    if (event->y != 0) {
        LiSendScrollEvent((signed char)event->y);
    }
}

static PGAMEPAD_STATE FindStateForGamepad(SDL_JoystickID id)
{
    int i;

    for (i = 0; i < MAX_GAMEPADS; i++) {
        if (g_GamepadState[i].jsId == id) {
            SDL_assert(!g_MultiController || g_GamepadState[i].index == i);
            return &g_GamepadState[i];
        }
    }

    // This should only happen with > 4 gamepads
    SDL_assert(SDL_NumJoysticks() > 4);
    return NULL;
}

void SendGamepadState(PGAMEPAD_STATE state)
{
    SDL_assert(g_GamepadMask == 0x1 || g_MultiController);
    LiSendMultiControllerEvent(state->index,
                               g_GamepadMask,
                               state->buttons,
                               state->lt,
                               state->rt,
                               state->lsX,
                               state->lsY,
                               state->rsX,
                               state->rsY);
}

void SdlHandleControllerAxisEvent(SDL_ControllerAxisEvent* event)
{
    PGAMEPAD_STATE state = FindStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    switch (event->axis)
    {
        case SDL_CONTROLLER_AXIS_LEFTX:
            state->lsX = event->value;
            break;
        case SDL_CONTROLLER_AXIS_LEFTY:
            state->lsY = event->value;
            break;
        case SDL_CONTROLLER_AXIS_RIGHTX:
            state->rsX = event->value;
            break;
        case SDL_CONTROLLER_AXIS_RIGHTY:
            state->rsY = event->value;
            break;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
            state->lt = (unsigned char)((event->value + 32768UL) * 255 / 65535);
            break;
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
            state->rt = (unsigned char)((event->value + 32768UL) * 255 / 65535);
            break;
        default:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Unhandled controller axis: %d",
                        event->axis);
            return;
    }

    SendGamepadState(state);
}

void SdlHandleControllerButtonEvent(SDL_ControllerButtonEvent* event)
{
    PGAMEPAD_STATE state = FindStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    if (event->state == SDL_PRESSED) {
        state->buttons |= k_ButtonMap[event->button];
    }
    else {
        state->buttons &= ~k_ButtonMap[event->button];
    }

    SendGamepadState(state);
}

void SdlHandleControllerDeviceEvent(SDL_ControllerDeviceEvent* event)
{
    PGAMEPAD_STATE state;

    if (event->type == SDL_CONTROLLERDEVICEADDED) {
        int i;
        const char* name;

        for (i = 0; i < MAX_GAMEPADS; i++) {
            if (g_GamepadState[i].controller == NULL) {
                // Found an empty slot
                break;
            }
        }

        if (i == MAX_GAMEPADS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "No open gamepad slots found!");
            return;
        }

        state = &g_GamepadState[i];
        if (g_MultiController) {
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
        if (g_MultiController) {
            SDL_assert(!(g_GamepadMask & (1 << state->index)));
            g_GamepadMask |= (1 << state->index);
        }
        else {
            SDL_assert(g_GamepadMask == 0x1);
        }

        // Send an empty event to tell the PC we've arrived
        SendGamepadState(state);
    }
    else if (event->type == SDL_CONTROLLERDEVICEREMOVED) {
        state = FindStateForGamepad(event->which);
        if (state != NULL) {
            SDL_GameControllerClose(state->controller);

            // Remove this from the gamepad mask in MC-mode
            if (g_MultiController) {
                SDL_assert(g_GamepadMask & (1 << state->index));
                g_GamepadMask &= ~(1 << state->index);
            }
            else {
                SDL_assert(g_GamepadMask == 0x1);
            }

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Gamepad %d is gone",
                        state->index);

            // Send a final event to let the PC know this gamepad is gone
            LiSendMultiControllerEvent(state->index, g_GamepadMask,
                                       0, 0, 0, 0, 0, 0, 0);

            // Clear all remaining state from this slot
            SDL_memset(state, 0, sizeof(*state));
        }
    }
}

void SdlInitializeGamepad(bool multiController)
{
    g_MultiController = multiController;
    if (!g_MultiController) {
        // Player 1 is always present in non-MC mode
        g_GamepadMask = 0x01;
    }
}
