#pragma once

#include <SDL.h>

struct GamepadState {
    SDL_GameController* controller;
    SDL_JoystickID jsId;
    short index;

    short buttons;
    short lsX, lsY;
    short rsX, rsY;
    unsigned char lt, rt;
};

#define MAX_GAMEPADS 4

class SdlInputHandler
{
public:
    explicit SdlInputHandler(bool multiController);

    void handleKeyEvent(SDL_KeyboardEvent* event);

    void handleMouseButtonEvent(SDL_MouseButtonEvent* event);

    void handleMouseMotionEvent(SDL_MouseMotionEvent* event);

    void handleMouseWheelEvent(SDL_MouseWheelEvent* event);

    void handleControllerAxisEvent(SDL_ControllerAxisEvent* event);

    void handleControllerButtonEvent(SDL_ControllerButtonEvent* event);

    void handleControllerDeviceEvent(SDL_ControllerDeviceEvent* event);

    int getAttachedGamepadMask();

private:
    GamepadState*
    findStateForGamepad(SDL_JoystickID id);

    void sendGamepadState(GamepadState* state);

    bool m_MultiController;
    int m_GamepadMask;
    GamepadState m_GamepadState[MAX_GAMEPADS];

    static const int k_ButtonMap[];
};
