#pragma once

#include "settings/streamingpreferences.h"
#include "backend/computermanager.h"

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
#define MAX_FINGERS 2

class SdlInputHandler
{
public:
    explicit SdlInputHandler(StreamingPreferences& prefs, NvComputer* computer,
                             int streamWidth, int streamHeight);

    ~SdlInputHandler();

    void handleKeyEvent(SDL_KeyboardEvent* event);

    void handleMouseButtonEvent(SDL_MouseButtonEvent* event);

    void handleMouseMotionEvent(SDL_MouseMotionEvent* event);

    void handleMouseWheelEvent(SDL_MouseWheelEvent* event);

    void handleControllerAxisEvent(SDL_ControllerAxisEvent* event);

    void handleControllerButtonEvent(SDL_ControllerButtonEvent* event);

    void handleControllerDeviceEvent(SDL_ControllerDeviceEvent* event);

    void handleJoystickArrivalEvent(SDL_JoyDeviceEvent* event);

    void handleTouchFingerEvent(SDL_TouchFingerEvent* event);

    int getAttachedGamepadMask();

    static
    QString getUnmappedGamepads();

private:
    GamepadState*
    findStateForGamepad(SDL_JoystickID id);

    void sendGamepadState(GamepadState* state);

    static
    Uint32 releaseLeftButtonTimerCallback(Uint32 interval, void* param);

    static
    Uint32 releaseRightButtonTimerCallback(Uint32 interval, void* param);

    static
    Uint32 dragTimerCallback(Uint32 interval, void* param);

    Uint32 m_LastMouseMotionTime;
    bool m_MultiController;
    bool m_NeedsInputDelay;
    int m_GamepadMask;
    GamepadState m_GamepadState[MAX_GAMEPADS];

    SDL_TouchFingerEvent m_TouchDownEvent[MAX_FINGERS];
    float m_CumulativeDelta[MAX_FINGERS];
    SDL_TimerID m_LeftButtonReleaseTimer;
    SDL_TimerID m_RightButtonReleaseTimer;
    SDL_TimerID m_DragTimer;
    char m_DragButton;
    int m_NumFingersDown;
    int m_StreamWidth;
    int m_StreamHeight;

    static const int k_ButtonMap[];
};
