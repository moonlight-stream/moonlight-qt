#include "streaming/session.h"

#include <Limelight.h>
#include <SDL.h>
#include "settings/mappingmanager.h"

#include <QtMath>

// How long the Start button must be pressed to toggle mouse emulation
#define MOUSE_EMULATION_LONG_PRESS_TIME 750

// How long between polling the gamepad to send virtual mouse input
#define MOUSE_EMULATION_POLLING_INTERVAL 50

// Determines how fast the mouse will move each interval
#define MOUSE_EMULATION_MOTION_MULTIPLIER 4

// Determines the maximum motion amount before allowing movement
#define MOUSE_EMULATION_DEADZONE 2

// Haptic capabilities (in addition to those from SDL_HapticQuery())
#define ML_HAPTIC_GC_RUMBLE      (1U << 16)
#define ML_HAPTIC_SIMPLE_RUMBLE  (1U << 17)

const int SdlInputHandler::k_ButtonMap[] = {
    A_FLAG, B_FLAG, X_FLAG, Y_FLAG,
    BACK_FLAG, SPECIAL_FLAG, PLAY_FLAG,
    LS_CLK_FLAG, RS_CLK_FLAG,
    LB_FLAG, RB_FLAG,
    UP_FLAG, DOWN_FLAG, LEFT_FLAG, RIGHT_FLAG
};

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

Uint32 SdlInputHandler::mouseEmulationTimerCallback(Uint32 interval, void *param)
{
    auto gamepad = reinterpret_cast<GamepadState*>(param);

    short rawX;
    short rawY;

    // Determine which analog stick is currently receiving the strongest input
    if ((uint32_t)qAbs(gamepad->lsX) + qAbs(gamepad->lsY) > (uint32_t)qAbs(gamepad->rsX) + qAbs(gamepad->rsY)) {
        rawX = gamepad->lsX;
        rawY = -gamepad->lsY;
    }
    else {
        rawX = gamepad->rsX;
        rawY = -gamepad->rsY;
    }

    float deltaX;
    float deltaY;

    // Produce a base vector for mouse movement with increased speed as we deviate further from center
    deltaX = qPow(rawX / 32766.0f * MOUSE_EMULATION_MOTION_MULTIPLIER, 3);
    deltaY = qPow(rawY / 32766.0f * MOUSE_EMULATION_MOTION_MULTIPLIER, 3);

    // Enforce deadzones
    deltaX = qAbs(deltaX) > MOUSE_EMULATION_DEADZONE ? deltaX - MOUSE_EMULATION_DEADZONE : 0;
    deltaY = qAbs(deltaY) > MOUSE_EMULATION_DEADZONE ? deltaY - MOUSE_EMULATION_DEADZONE : 0;

    if (deltaX != 0 || deltaY != 0) {
        LiSendMouseMoveEvent((short)deltaX, (short)deltaY);
    }

    return interval;
}

void SdlInputHandler::handleControllerSensorEvent(SDL_ControllerSensorEvent* event)
{
    // Currently unhandled
    switch (event->sensor)
    {
        case SDL_SENSOR_ACCEL:
            SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION,
                        "CONTROLLER: Gyro motion %f %f %f", event->data[0], event->data[1], event->data[2]);
        break;
    }
}
void SdlInputHandler::handleControllerAxisEvent(SDL_ControllerAxisEvent* event)
{
    SDL_JoystickID gameControllerId = event->which;
    GamepadState* state = findStateForGamepad(gameControllerId);
    if (state == NULL) {
        return;
    }

    // Batch all pending axis motion events for this gamepad to save CPU time
    SDL_Event nextEvent;
    for (;;) {
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

        // Check for another event to batch with
        if (SDL_PeepEvents(&nextEvent, 1, SDL_PEEKEVENT, SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERAXISMOTION) <= 0) {
            break;
        }

        event = &nextEvent.caxis;
        if (event->which != gameControllerId) {
            // Stop batching if a different gamepad interrupts us
            break;
        }

        // Remove the next event to batch
        SDL_PeepEvents(&nextEvent, 1, SDL_GETEVENT, SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERAXISMOTION);
    }

    // Only send the gamepad state to the host if it's not in mouse emulation mode
    if (state->mouseEmulationTimer == 0) {
        sendGamepadState(state);
    }
}

void SdlInputHandler::handleControllerButtonEvent(SDL_ControllerButtonEvent* event)
{
    GamepadState* state = findStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    if (m_SwapFaceButtons) {
        switch (event->button) {
        case SDL_CONTROLLER_BUTTON_A:
            event->button = SDL_CONTROLLER_BUTTON_B;
            break;
        case SDL_CONTROLLER_BUTTON_B:
            event->button = SDL_CONTROLLER_BUTTON_A;
            break;
        case SDL_CONTROLLER_BUTTON_X:
            event->button = SDL_CONTROLLER_BUTTON_Y;
            break;
        case SDL_CONTROLLER_BUTTON_Y:
            event->button = SDL_CONTROLLER_BUTTON_X;
            break;
        }
    }

    if (event->state == SDL_PRESSED) {
        state->buttons |= k_ButtonMap[event->button];

        if (event->button == SDL_CONTROLLER_BUTTON_START) {
            state->lastStartDownTime = SDL_GetTicks();
        }
        else if (state->mouseEmulationTimer != 0) {
            if (event->button == SDL_CONTROLLER_BUTTON_A) {
                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_B) {
                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_RIGHT);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_X) {
                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_MIDDLE);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_X1);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_X2);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                LiSendScrollEvent(1);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                LiSendScrollEvent(-1);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                LiSendHScrollEvent(1);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                LiSendHScrollEvent(-1);
            }
        }
    }
    else {
        state->buttons &= ~k_ButtonMap[event->button];

        if (event->button == SDL_CONTROLLER_BUTTON_START) {
            if (SDL_GetTicks() - state->lastStartDownTime > MOUSE_EMULATION_LONG_PRESS_TIME) {
                if (state->mouseEmulationTimer != 0) {
                    SDL_RemoveTimer(state->mouseEmulationTimer);
                    state->mouseEmulationTimer = 0;

                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Mouse emulation deactivated");
                    Session::get()->notifyMouseEmulationMode(false);
                }
                else if (m_GamepadMouse) {
                    // Send the start button up event to the host, since we won't do it below
                    sendGamepadState(state);

                    state->mouseEmulationTimer = SDL_AddTimer(MOUSE_EMULATION_POLLING_INTERVAL, SdlInputHandler::mouseEmulationTimerCallback, state);

                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Mouse emulation active");
                    Session::get()->notifyMouseEmulationMode(true);
                }
            }
        }
        else if (state->mouseEmulationTimer != 0) {
            if (event->button == SDL_CONTROLLER_BUTTON_A) {
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_B) {
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_RIGHT);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_X) {
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_MIDDLE);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_X1);
            }
            else if (event->button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_X2);
            }
        }
    }

    // Handle Start+Select+L1+R1 as a gamepad quit combo
    if (state->buttons == (PLAY_FLAG | BACK_FLAG | LB_FLAG | RB_FLAG) && qgetenv("NO_GAMEPAD_QUIT") != "1") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected quit gamepad button combo");

        // Push a quit event to the main loop
        SDL_Event event;
        event.type = SDL_QUIT;
        event.quit.timestamp = SDL_GetTicks();
        SDL_PushEvent(&event);

        // Clear buttons down on this gamepad
        LiSendMultiControllerEvent(state->index, m_GamepadMask,
                                   0, 0, 0, 0, 0, 0, 0);
        return;
    }

    // Handle Select+L1+R1+X as a gamepad overlay combo
    if (state->buttons == (BACK_FLAG | LB_FLAG | RB_FLAG | X_FLAG)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected stats toggle gamepad combo");

        // Toggle the stats overlay
        Session::get()->getOverlayManager().setOverlayState(Overlay::OverlayDebug,
                                                            !Session::get()->getOverlayManager().isOverlayEnabled(Overlay::OverlayDebug));

        // Clear buttons down on this gamepad
        LiSendMultiControllerEvent(state->index, m_GamepadMask,
                                   0, 0, 0, 0, 0, 0, 0);
        return;
    }

    // Only send the gamepad state to the host if it's not in mouse emulation mode
    if (state->mouseEmulationTimer == 0) {
        sendGamepadState(state);
    }
}

void SdlInputHandler::handleControllerDeviceEvent(SDL_ControllerDeviceEvent* event)
{
    GamepadState* state;

    if (event->type == SDL_CONTROLLERDEVICEADDED) {
        int i;
        const char* name;
        SDL_GameController* controller;
        const char* mapping;
        char guidStr[33];
        uint32_t hapticCaps;
        float motionDataRate = 0;

        controller = SDL_GameControllerOpen(event->which);
        if (controller == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to open gamepad: %s",
                         SDL_GetError());
            return;
        }

        // Gyro support only started on SDL 2.0.14+
        // https://github.com/libsdl-org/SDL/blob/4cd981609b50ed273d80c635c1ca4c1e5518fb21/WhatsNew.txt
#if SDL_VERSION_ATLEAST(2, 0, 14)
        if (SDL_GameControllerHasSensor(controller, SDL_SensorType::SDL_SENSOR_ACCEL) == SDL_bool::SDL_TRUE) {

            if (SDL_GameControllerIsSensorEnabled(controller, SDL_SensorType::SDL_SENSOR_ACCEL) != SDL_bool:: SDL_TRUE) {
                if (SDL_GameControllerSetSensorEnabled(controller, SDL_SensorType::SDL_SENSOR_ACCEL, SDL_bool::SDL_TRUE) != 0) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to enable gamepad gyro: %s",
                         SDL_GetError());
                    return;
                }
            }
            motionDataRate = SDL_GameControllerGetSensorDataRate(controller, SDL_SensorType::SDL_SENSOR_ACCEL);

        }
#endif

        // We used to use SDL_GameControllerGetPlayerIndex() here but that
        // can lead to strange issues due to bugs in Windows where an Xbox
        // controller will join as player 2, even though no player 1 controller
        // is connected at all. This pretty much screws any attempt to use
        // the gamepad in single player games, so just assign them in order from 0.
        i = 0;

        for (; i < MAX_GAMEPADS; i++) {
            SDL_assert(m_GamepadState[i].controller != controller);
            if (m_GamepadState[i].controller == NULL) {
                // Found an empty slot
                break;
            }
        }

        if (i == MAX_GAMEPADS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "No open gamepad slots found!");
            SDL_GameControllerClose(controller);
            return;
        }

        state = &m_GamepadState[i];
        if (m_MultiController) {
            state->index = i;

#if SDL_VERSION_ATLEAST(2, 0, 12)
            // This will change indicators on the controller to show the assigned
            // player index. For Xbox 360 controllers, that means updating the LED
            // ring to light up the corresponding quadrant for this player.
            SDL_GameControllerSetPlayerIndex(controller, state->index);
#endif
        }
        else {
            // Always player 1 in single controller mode
            state->index = 0;
        }

        state->controller = controller;
        state->jsId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(state->controller));

#if SDL_VERSION_ATLEAST(2, 0, 18)
        hapticCaps = SDL_GameControllerHasRumble(controller) ? ML_HAPTIC_GC_RUMBLE : 0;
#elif SDL_VERSION_ATLEAST(2, 0, 9)
        // Perform a tiny rumble to see if haptics are supported.
        // NB: We cannot use zeros for rumble intensity or SDL will not actually call the JS driver
        // and we'll get a (potentially false) success value returned.
        hapticCaps = SDL_GameControllerRumble(controller, 1, 1, 1) == 0 ?
                        ML_HAPTIC_GC_RUMBLE : 0;
#else
        state->haptic = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(state->controller));
        state->hapticEffectId = -1;
        state->hapticMethod = GAMEPAD_HAPTIC_METHOD_NONE;
        if (state->haptic != nullptr) {
            // Query for supported haptic effects
            hapticCaps = SDL_HapticQuery(state->haptic);
            hapticCaps |= SDL_HapticRumbleSupported(state->haptic) ?
                            ML_HAPTIC_SIMPLE_RUMBLE : 0;

            if ((SDL_HapticQuery(state->haptic) & SDL_HAPTIC_LEFTRIGHT) == 0) {
                if (SDL_HapticRumbleSupported(state->haptic)) {
                    if (SDL_HapticRumbleInit(state->haptic) == 0) {
                        state->hapticMethod = GAMEPAD_HAPTIC_METHOD_SIMPLERUMBLE;
                    }
                }
                if (state->hapticMethod == GAMEPAD_HAPTIC_METHOD_NONE) {
                    SDL_HapticClose(state->haptic);
                    state->haptic = nullptr;
                }
            } else {
                state->hapticMethod = GAMEPAD_HAPTIC_METHOD_LEFTRIGHT;
            }
        }
        else {
            hapticCaps = 0;
        }
#endif

        SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(state->controller)),
                                  guidStr, sizeof(guidStr));
        mapping = SDL_GameControllerMapping(state->controller);
        name = SDL_GameControllerName(state->controller);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Gamepad %d (player %d) is: %s (haptic capabilities: 0x%x) (motion rate: %f) (mapping: %s -> %s)",
                    i,
                    state->index,
                    name != nullptr ? name : "<null>",
                    hapticCaps,
                    motionDataRate,
                    guidStr,
                    mapping != nullptr ? mapping : "<null>");
        if (mapping != nullptr) {
            SDL_free((void*)mapping);
        }

        // Add this gamepad to the gamepad mask
        if (m_MultiController) {
            // NB: Don't assert that it's unset here because we will already
            // have the mask set for initially attached gamepads to avoid confusing
            // apps running on the host.
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
            if (state->mouseEmulationTimer != 0) {
                Session::get()->notifyMouseEmulationMode(false);
                SDL_RemoveTimer(state->mouseEmulationTimer);
            }

            SDL_GameControllerClose(state->controller);

#if !SDL_VERSION_ATLEAST(2, 0, 9)
            if (state->haptic != nullptr) {
                SDL_HapticClose(state->haptic);
            }
#endif

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

void SdlInputHandler::handleJoystickArrivalEvent(SDL_JoyDeviceEvent* event)
{
    SDL_assert(event->type == SDL_JOYDEVICEADDED);

    if (!SDL_IsGameController(event->which)) {
        char guidStr[33];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(event->which),
                                  guidStr, sizeof(guidStr));
        const char* name = SDL_JoystickNameForIndex(event->which);
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Joystick discovered with no mapping: %s %s",
                    name ? name : "<UNKNOWN>",
                    guidStr);
        SDL_Joystick* joy = SDL_JoystickOpen(event->which);
        if (joy != nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Number of axes: %d | Number of buttons: %d | Number of hats: %d",
                        SDL_JoystickNumAxes(joy), SDL_JoystickNumButtons(joy),
                        SDL_JoystickNumHats(joy));
            SDL_JoystickClose(joy);
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Unable to open joystick for query: %s",
                        SDL_GetError());
        }
    }
}

void SdlInputHandler::rumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
    // Make sure the controller number is within our supported count
    if (controllerNumber >= MAX_GAMEPADS) {
        return;
    }

#if SDL_VERSION_ATLEAST(2, 0, 9)
    if (m_GamepadState[controllerNumber].controller != nullptr) {
        SDL_GameControllerRumble(m_GamepadState[controllerNumber].controller, lowFreqMotor, highFreqMotor, 30000);
    }
#else
    // Check if the controller supports haptics (and if the controller exists at all)
    SDL_Haptic* haptic = m_GamepadState[controllerNumber].haptic;
    if (haptic == nullptr) {
        return;
    }

    // Stop the last effect we played
    if (m_GamepadState[controllerNumber].hapticMethod == GAMEPAD_HAPTIC_METHOD_LEFTRIGHT) {
        if (m_GamepadState[controllerNumber].hapticEffectId >= 0) {
            SDL_HapticDestroyEffect(haptic, m_GamepadState[controllerNumber].hapticEffectId);
        }
    } else if (m_GamepadState[controllerNumber].hapticMethod == GAMEPAD_HAPTIC_METHOD_SIMPLERUMBLE) {
        SDL_HapticRumbleStop(haptic);
    }

    // If this callback is telling us to stop both motors, don't bother queuing a new effect
    if (lowFreqMotor == 0 && highFreqMotor == 0) {
        return;
    }

    if (m_GamepadState[controllerNumber].hapticMethod == GAMEPAD_HAPTIC_METHOD_LEFTRIGHT) {
        SDL_HapticEffect effect;
        SDL_memset(&effect, 0, sizeof(effect));
        effect.type = SDL_HAPTIC_LEFTRIGHT;

        // The effect should last until we are instructed to stop or change it
        effect.leftright.length = SDL_HAPTIC_INFINITY;

        // SDL haptics range from 0-32767 but XInput uses 0-65535, so divide by 2 to correct for SDL's scaling
        effect.leftright.large_magnitude = lowFreqMotor / 2;
        effect.leftright.small_magnitude = highFreqMotor / 2;

        // Play the new effect
        m_GamepadState[controllerNumber].hapticEffectId = SDL_HapticNewEffect(haptic, &effect);
        if (m_GamepadState[controllerNumber].hapticEffectId >= 0) {
            SDL_HapticRunEffect(haptic, m_GamepadState[controllerNumber].hapticEffectId, 1);
        }
    } else if (m_GamepadState[controllerNumber].hapticMethod == GAMEPAD_HAPTIC_METHOD_SIMPLERUMBLE) {
        SDL_HapticRumblePlay(haptic,
                             std::min(1.0, (GAMEPAD_HAPTIC_SIMPLE_HIFREQ_MOTOR_WEIGHT*highFreqMotor +
                                            GAMEPAD_HAPTIC_SIMPLE_LOWFREQ_MOTOR_WEIGHT*lowFreqMotor) / 65535.0),
                             SDL_HAPTIC_INFINITY);
    }
#endif
}

QString SdlInputHandler::getUnmappedGamepads()
{
    QString ret;

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) failed: %s",
                     SDL_GetError());
    }

    MappingManager mappingManager;
    mappingManager.applyMappings();

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!SDL_IsGameController(i)) {
            char guidStr[33];
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i),
                                      guidStr, sizeof(guidStr));
            const char* name = SDL_JoystickNameForIndex(i);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Unmapped joystick: %s %s",
                        name ? name : "<UNKNOWN>",
                        guidStr);
            SDL_Joystick* joy = SDL_JoystickOpen(i);
            if (joy != nullptr) {
                int numButtons = SDL_JoystickNumButtons(joy);
                int numHats = SDL_JoystickNumHats(joy);
                int numAxes = SDL_JoystickNumAxes(joy);

                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Number of axes: %d | Number of buttons: %d | Number of hats: %d",
                            numAxes, numButtons, numHats);

                if ((numAxes >= 4 && numAxes <= 8) && numButtons >= 8 && numHats <= 1) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Joystick likely to be an unmapped game controller");
                    if (!ret.isEmpty()) {
                        ret += ", ";
                    }

                    ret += name;
                }

                SDL_JoystickClose(joy);
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Unable to open joystick for query: %s",
                            SDL_GetError());
            }
        }
    }

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);

    return ret;
}

int SdlInputHandler::getAttachedGamepadMask()
{
    int count;
    int mask;

    if (!m_MultiController) {
        // Player 1 is always present in non-MC mode
        return 0x1;
    }

    count = mask = 0;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            mask |= (1 << count++);
        }
    }

    return mask;
}
