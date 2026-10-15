#include "streaming/session.h"

#include <Limelight.h>
#include "SDL_compat.h"
#include "settings/mappingmanager.h"
#include "utils.h"

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
#define ML_HAPTIC_GC_RUMBLE         (1U << 16)
#define ML_HAPTIC_GC_TRIGGER_RUMBLE (1U << 18)
#define ML_HAPTIC_GC_ADDRESSABLE    (1U << 19)

const uint32_t SdlInputHandler::k_ButtonMap[] = {
    A_FLAG, B_FLAG, X_FLAG, Y_FLAG,
    BACK_FLAG, SPECIAL_FLAG, PLAY_FLAG,
    LS_CLK_FLAG, RS_CLK_FLAG,
    LB_FLAG, RB_FLAG,
    UP_FLAG, DOWN_FLAG, LEFT_FLAG, RIGHT_FLAG,
    MISC_FLAG,
    PADDLE1_FLAG, PADDLE2_FLAG, PADDLE3_FLAG, PADDLE4_FLAG,
    TOUCHPAD_FLAG,
    0, 0, 0, 0, 0,
};

uint32_t SdlInputHandler::getButtonFlag(const GamepadState* state, SDL_GamepadButton button)
{
    if (state->isSteamController) {
        switch (button) {
        case SDL_GAMEPAD_BUTTON_MISC2:
            return STEAM_RIGHT_TOUCHPAD_FLAG;
        case SDL_GAMEPAD_BUTTON_MISC3:
            return STEAM_LEFT_TRIGGER_CLICK_FLAG;
        case SDL_GAMEPAD_BUTTON_MISC4:
            return STEAM_RIGHT_TRIGGER_CLICK_FLAG;
        default:
            break;
        }
    }

    return button >= 0 && static_cast<size_t>(button) < SDL_arraysize(k_ButtonMap) ? k_ButtonMap[button] : 0;
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

    // We can get a spurious removal event if the device is removed
    // before or during SDL_GameControllerOpen(). This is fine to ignore.
    return nullptr;
}

void SdlInputHandler::sendGamepadState(GamepadState* state)
{
    SDL_assert(m_GamepadMask == 0x1 || m_MultiController);

    // Handle Select+PS as the clickpad button on PS4/5 controllers without a clickpad mapping
    uint32_t buttons = state->buttons;
    if (state->clickpadButtonEmulationEnabled) {
        if (state->buttons == (BACK_FLAG | SPECIAL_FLAG)) {
            buttons = MISC_FLAG;
            state->emulatedClickpadButtonDown = true;
        }
        else if (state->emulatedClickpadButtonDown) {
            buttons &= ~MISC_FLAG;
            state->emulatedClickpadButtonDown = false;
        }
    }

    unsigned char lt = state->lt;
    unsigned char rt = state->rt;
    short lsX = state->lsX;
    short lsY = state->lsY;
    short rsX = state->rsX;
    short rsY = state->rsY;

    // When in single controller mode, merge all gamepad state together
    if (!m_MultiController) {
        for (int i = 0; i < MAX_GAMEPADS; i++) {
            if (m_GamepadState[i].index == state->index) {
                buttons |= m_GamepadState[i].buttons;
                if (lt < m_GamepadState[i].lt) {
                    lt = m_GamepadState[i].lt;
                }
                if (rt < m_GamepadState[i].rt) {
                    rt = m_GamepadState[i].rt;
                }

                // We use abs() here instead of qAbs() for get proper integer promotion to
                // correctly handle abs(-32768), which is not representable in a short.
                if (abs(lsX) < abs(m_GamepadState[i].lsX) || abs(lsY) < abs(m_GamepadState[i].lsY)) {
                    lsX = m_GamepadState[i].lsX;
                    lsY = m_GamepadState[i].lsY;
                }
                if (abs(rsX) < abs(m_GamepadState[i].rsX) || abs(rsY) < abs(m_GamepadState[i].rsY)) {
                    rsX = m_GamepadState[i].rsX;
                    rsY = m_GamepadState[i].rsY;
                }
            }
        }
    }

    LiSendMultiControllerEvent(state->index,
                               m_GamepadMask,
                               buttons,
                               lt,
                               rt,
                               lsX,
                               lsY,
                               rsX,
                               rsY);
}

void SdlInputHandler::sendGamepadBatteryState(GamepadState* state, SDL_PowerState powerState, int percentage)
{
    uint8_t batteryPercentage = percentage >= 0 && percentage <= 100 ?
                                    static_cast<uint8_t>(percentage) :
                                    LI_BATTERY_PERCENTAGE_UNKNOWN;
    uint8_t batteryState;

    switch (powerState)
    {
    case SDL_POWERSTATE_ERROR:
    case SDL_POWERSTATE_UNKNOWN:
        batteryState = LI_BATTERY_STATE_UNKNOWN;
        break;
    case SDL_POWERSTATE_ON_BATTERY:
        batteryState = LI_BATTERY_STATE_DISCHARGING;
        break;
    case SDL_POWERSTATE_NO_BATTERY:
        batteryState = LI_BATTERY_STATE_NOT_PRESENT;
        batteryPercentage = LI_BATTERY_PERCENTAGE_UNKNOWN;
        break;
    case SDL_POWERSTATE_CHARGING:
        batteryState = LI_BATTERY_STATE_CHARGING;
        break;
    case SDL_POWERSTATE_CHARGED:
        batteryState = LI_BATTERY_STATE_FULL;
        break;
    default:
        return;
    }

    LiSendControllerBatteryEvent(state->index, batteryState, batteryPercentage);
}

Uint32 SdlInputHandler::mouseEmulationTimerCallback(void* param, SDL_TimerID, Uint32 interval)
{
    auto gamepad = reinterpret_cast<GamepadState*>(param);

    int rawX;
    int rawY;

    // Determine which analog stick is currently receiving the strongest input
    if (abs(gamepad->lsX) + abs(gamepad->lsY) > abs(gamepad->rsX) + abs(gamepad->rsY)) {
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

static void updateAnalogStickAxis(short& value, short newValue, bool& dirty)
{
#ifdef STEAM_LINK
    // Use a deadzone on Steam Link to reduce CPU usage from idle joysticks
    newValue = abs(newValue) < 1500 ? 0 : newValue;
#endif
    dirty |= (newValue != value);
    value = newValue;
}

static void updateTriggerAxis(unsigned char& value, unsigned char newValue, bool& dirty)
{
#ifdef STEAM_LINK
    // Use a deadzone on Steam Link to reduce CPU usage from idle joysticks
    newValue = newValue < 10 ? 0 : newValue;
#endif
    dirty |= (newValue != value);
    value = newValue;
}

void SdlInputHandler::handleControllerAxisEvent(SDL_GamepadAxisEvent * event)
{
    SDL_JoystickID gameControllerId = event->which;
    GamepadState* state = findStateForGamepad(gameControllerId);
    if (state == NULL) {
        return;
    }

    // Batch all pending axis motion events for this gamepad to save CPU time
    SDL_Event nextEvent;
    bool dirty = false;
    for (;;) {
        switch (event->axis)
        {
            case SDL_GAMEPAD_AXIS_LEFTX:
                updateAnalogStickAxis(state->lsX, event->value, dirty);
                break;
            case SDL_GAMEPAD_AXIS_LEFTY :
                // Signed values have one more negative value than
                // positive value, so inverting the sign on -32768
                // could actually cause the value to overflow and
                // wrap around to be negative again. Avoid that by
                // capping the value at 32767.
                updateAnalogStickAxis(state->lsY, -qMax(event->value, (short)-32767), dirty);
                break;
            case SDL_GAMEPAD_AXIS_RIGHTX:
                updateAnalogStickAxis(state->rsX, event->value, dirty);
                break;
            case SDL_GAMEPAD_AXIS_RIGHTY:
                updateAnalogStickAxis(state->rsY, -qMax(event->value, (short)-32767), dirty);
                break;
            case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
                updateTriggerAxis(state->lt, (unsigned char)(event->value * 255UL / 32767), dirty);
                break;
            case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
                updateTriggerAxis(state->rt, (unsigned char)(event->value * 255UL / 32767), dirty);
                break;
            default:
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Unhandled controller axis: %d",
                            event->axis);
                return;
        }

        // Check for another event to batch with
        if (SDL_PeepEvents(&nextEvent, 1, SDL_PEEKEVENT, SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_EVENT_GAMEPAD_AXIS_MOTION) <= 0) {
            break;
        }

        event = &nextEvent.gaxis;
        if (event->which != gameControllerId) {
            // Stop batching if a different gamepad interrupts us
            break;
        }

        // Remove the next event to batch
        SDL_PeepEvents(&nextEvent, 1, SDL_GETEVENT, SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_EVENT_GAMEPAD_AXIS_MOTION);
    }

    // Only send the gamepad state to the host if it's not in mouse emulation mode
    if (state->mouseEmulationTimer == 0 && dirty) {
        sendGamepadState(state);
    }
}

void SdlInputHandler::handleControllerButtonEvent(SDL_GamepadButtonEvent * event)
{
    GamepadState* state = findStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    if (m_SwapFaceButtons) {
        switch (event->button) {
        case SDL_GAMEPAD_BUTTON_SOUTH :
            event->button = SDL_GAMEPAD_BUTTON_EAST;
            break;
        case SDL_GAMEPAD_BUTTON_EAST :
            event->button = SDL_GAMEPAD_BUTTON_SOUTH;
            break;
        case SDL_GAMEPAD_BUTTON_WEST :
            event->button = SDL_GAMEPAD_BUTTON_NORTH;
            break;
        case SDL_GAMEPAD_BUTTON_NORTH :
            event->button = SDL_GAMEPAD_BUTTON_WEST;
            break;
        }
    }

    uint32_t buttonFlag = getButtonFlag(state, static_cast<SDL_GamepadButton>(event->button));
    if (buttonFlag == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "No mapping for gamepad button: %u",
                    event->button);
        return;
    }

    if (event->down) {
        state->buttons |= buttonFlag;

        if (event->button == SDL_GAMEPAD_BUTTON_START) {
            state->lastStartDownTime = SDL_GetTicks();
        }
        else if (state->mouseEmulationTimer != 0) {
            if (event->button == SDL_GAMEPAD_BUTTON_SOUTH) {
                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_EAST) {
                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_RIGHT);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_WEST) {
                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_MIDDLE);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) {
                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_X1);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
                LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_X2);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_DPAD_UP) {
                LiSendScrollEvent(1);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                LiSendScrollEvent(-1);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
                LiSendHScrollEvent(1);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_DPAD_LEFT) {
                LiSendHScrollEvent(-1);
            }
        }
    }
    else {
        state->buttons &= ~buttonFlag;

        if (event->button == SDL_GAMEPAD_BUTTON_START) {
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
            if (event->button == SDL_GAMEPAD_BUTTON_SOUTH) {
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_EAST) {
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_RIGHT);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_WEST) {
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_MIDDLE);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) {
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_X1);
            }
            else if (event->button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_X2);
            }
        }
    }

    // Capacitive stick and grip contacts can remain active while holding the
    // Steam Controller. They are not buttons in the quit combo.
    const uint32_t incidentalTouchFlags = state->isSteamController ?
        (STEAM_LEFT_STICK_TOUCH_FLAG | STEAM_RIGHT_STICK_TOUCH_FLAG |
         STEAM_LEFT_GRIP_TOUCH_FLAG | STEAM_RIGHT_GRIP_TOUCH_FLAG) : 0;
    const uint32_t comboButtons = state->buttons & ~incidentalTouchFlags;

    // Handle Start+Select+L1+R1 as a gamepad quit combo
    if (comboButtons == (PLAY_FLAG | BACK_FLAG | LB_FLAG | RB_FLAG) &&
        qgetenv("NO_GAMEPAD_QUIT") != "1") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected quit gamepad button combo");

        // Push a quit event to the main loop
        SDL_Event event;
        event.type = SDL_EVENT_QUIT;
        event.quit.timestamp = SDL_GetTicks();
        SDL_PushEvent(&event);

        // Clear buttons down on this gamepad
        LiSendMultiControllerEvent(state->index, m_GamepadMask,
                                   0, 0, 0, 0, 0, 0, 0);
        return;
    }

    // Handle Select+L1+R1+X as a gamepad overlay combo
    if (comboButtons == (BACK_FLAG | LB_FLAG | RB_FLAG | X_FLAG)) {
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

void SdlInputHandler::handleControllerSensorEvent(SDL_GamepadSensorEvent * event)
{
    GamepadState* state = findStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    switch (event->sensor) {
    case SDL_SENSOR_ACCEL:
        if (state->accelReportPeriodMs &&
                event->timestamp >= state->lastAccelEventTime + SDL_MS_TO_NS(state->accelReportPeriodMs) &&
                memcmp(event->data, state->lastAccelEventData, sizeof(event->data)) != 0) {
            memcpy(state->lastAccelEventData, event->data, sizeof(event->data));
            state->lastAccelEventTime = event->timestamp;

            LiSendControllerMotionEvent((uint8_t)state->index, LI_MOTION_TYPE_ACCEL, event->data[0], event->data[1], event->data[2]);
        }
        break;
    case SDL_SENSOR_GYRO: {
        const bool shouldForward = state->gyroReportPeriodMs &&
            event->timestamp >= state->lastGyroEventTime + SDL_MS_TO_NS(state->gyroReportPeriodMs) &&
            memcmp(event->data, state->lastGyroEventData, sizeof(event->data)) != 0;

        // Enable with SDL_LOGGING=input=debug to compare the physical SDL
        // reading with what Moonlight forwards, without flooding normal logs.
        if (state->isSteamController &&
                SDL_GetLogPriority(SDL_LOG_CATEGORY_INPUT) <= SDL_LOG_PRIORITY_DEBUG &&
                event->timestamp >= state->lastGyroDebugTime + SDL_MS_TO_NS(200)) {
            SDL_LogDebug(SDL_LOG_CATEGORY_INPUT,
                         "Steam Controller gyro SDL rad/s: %.3f, %.3f, %.3f; forward: %d",
                         event->data[0], event->data[1], event->data[2], shouldForward);
            state->lastGyroDebugTime = event->timestamp;
        }

        if (shouldForward) {
            memcpy(state->lastGyroEventData, event->data, sizeof(event->data));
            state->lastGyroEventTime = event->timestamp;

            // Convert rad/s to deg/s
            LiSendControllerMotionEvent((uint8_t)state->index, LI_MOTION_TYPE_GYRO,
                                        event->data[0] * 57.2957795f,
                                        event->data[1] * 57.2957795f,
                                        event->data[2] * 57.2957795f);
        }
        break;
    }
    }
}

void SdlInputHandler::handleControllerTouchpadEvent(SDL_GamepadTouchpadEvent * event)
{
    GamepadState* state = findStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    uint8_t eventType;
    switch (event->type) {
    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN :
        eventType = LI_TOUCH_EVENT_DOWN;
        break;
    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP :
        eventType = LI_TOUCH_EVENT_UP;
        break;
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION :
        eventType = LI_TOUCH_EVENT_MOVE;
        break;
    default:
        return;
    }

    if (state->isSteamController &&
            SDL_GetLogPriority(SDL_LOG_CATEGORY_INPUT) <= SDL_LOG_PRIORITY_DEBUG &&
            (eventType != LI_TOUCH_EVENT_MOVE ||
             event->timestamp >= state->lastTouchpadDebugTime + SDL_MS_TO_NS(200))) {
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT,
                     "Steam Controller touch forwarded: ms=%llu pad=%d finger=%d event=%s x=%.3f y=%.3f pressure=%.3f",
                     (unsigned long long)SDL_NS_TO_MS(event->timestamp), event->touchpad, event->finger,
                     eventType == LI_TOUCH_EVENT_DOWN ? "down" :
                     eventType == LI_TOUCH_EVENT_UP ? "up" : "move",
                     event->x, event->y, event->pressure);
        state->lastTouchpadDebugTime = event->timestamp;
    }

    LiSendControllerTouchEvent2((uint8_t)state->index, eventType,
                                (uint8_t)event->touchpad, event->finger,
                                event->x, event->y, event->pressure);
}

#if SDL_VERSION_ATLEAST(3, 5, 0)
void SdlInputHandler::handleControllerCapSenseEvent(SDL_GamepadCapSenseEvent* event)
{
    GamepadState* state = findStateForGamepad(event->which);
    if (state == nullptr || !state->isSteamController) {
        return;
    }

    uint32_t buttonFlag;
    switch (event->capsense) {
    case SDL_GAMEPAD_CAPSENSE_LEFT_STICK:
        buttonFlag = STEAM_LEFT_STICK_TOUCH_FLAG;
        break;
    case SDL_GAMEPAD_CAPSENSE_RIGHT_STICK:
        buttonFlag = STEAM_RIGHT_STICK_TOUCH_FLAG;
        break;
    case SDL_GAMEPAD_CAPSENSE_LEFT_GRIP:
        buttonFlag = STEAM_LEFT_GRIP_TOUCH_FLAG;
        break;
    case SDL_GAMEPAD_CAPSENSE_RIGHT_GRIP:
        buttonFlag = STEAM_RIGHT_GRIP_TOUCH_FLAG;
        break;
    default:
        return;
    }

    if (event->down) {
        state->buttons |= buttonFlag;
    }
    else {
        state->buttons &= ~buttonFlag;
    }
    sendGamepadState(state);
}
#endif

void SdlInputHandler::handleJoystickBatteryEvent(SDL_JoyBatteryEvent* event)
{
    GamepadState* state = findStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    sendGamepadBatteryState(state, event->state, event->percent);
}

void SdlInputHandler::handleControllerDeviceEvent(SDL_GamepadDeviceEvent * event)
{
    GamepadState* state;

    if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        int i;
        const char* name;
        SDL_Gamepad * controller;
        const char* mapping;
        char guidStr[33];
        uint32_t hapticCaps;

        controller = SDL_OpenGamepad(event->which);
        if (controller == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to open gamepad: %s",
                         SDL_GetError());
            return;
        }

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
            SDL_CloseGamepad(controller);
            return;
        }

        SDL_GUIDToString(SDL_GetJoystickGUID(SDL_GetGamepadJoystick(controller)),
                         guidStr, sizeof(guidStr));
        if (m_IgnoreDeviceGuids.contains(guidStr, Qt::CaseInsensitive))
        {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Skipping ignored device with GUID: %s",
                        guidStr);
            SDL_CloseGamepad(controller);
            return;
        }

        state = &m_GamepadState[i];
        if (m_MultiController) {
            state->index = i;

            // This will change indicators on the controller to show the assigned
            // player index. For Xbox 360 controllers, that means updating the LED
            // ring to light up the corresponding quadrant for this player.
            SDL_SetGamepadPlayerIndex(controller, state->index);
        }
        else {
            // Always player 1 in single controller mode
            state->index = 0;
        }

        state->controller = controller;
        state->jsId = SDL_GetJoystickID(SDL_GetGamepadJoystick(state->controller));

        SDL_PropertiesID gamepadProperties = SDL_GetGamepadProperties(controller);
        hapticCaps = 0;
        hapticCaps |= SDL_GetBooleanProperty(gamepadProperties, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false) ?
                          ML_HAPTIC_GC_RUMBLE : 0;
        hapticCaps |= SDL_GetBooleanProperty(gamepadProperties, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false) ?
                          ML_HAPTIC_GC_TRIGGER_RUMBLE : 0;

        mapping = SDL_GetGamepadMapping(state->controller);
        name = SDL_GetGamepadName(state->controller);

        uint16_t vendorId = SDL_GetGamepadVendor(state->controller);
        uint16_t productId = SDL_GetGamepadProduct(state->controller);
        state->isSteamController = SDL_GetGamepadType(state->controller) == SDL_GAMEPAD_TYPE_STEAM &&
                                   vendorId == 0x28de && productId >= 0x1302 && productId <= 0x1305;
        hapticCaps |= state->isSteamController ? ML_HAPTIC_GC_ADDRESSABLE : 0;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Gamepad %d (player %d) is: %s (VID/PID: 0x%.4x/0x%.4x) (haptic capabilities: 0x%x) (mapping: %s -> %s)",
                    i,
                    state->index,
                    name != nullptr ? name : "<null>",
                    vendorId,
                    productId,
                    hapticCaps,
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

        int batteryPercentage = -1;
        SDL_PowerState powerState = SDL_GetJoystickPowerInfo(SDL_GetGamepadJoystick(state->controller),
                                                             &batteryPercentage);

        uint32_t supportedButtonFlags = 0;
        for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; button++) {
            SDL_GamepadButton gamepadButton = static_cast<SDL_GamepadButton>(button);
            if (SDL_GamepadHasButton(state->controller, gamepadButton)) {
                supportedButtonFlags |= getButtonFlag(state, gamepadButton);
            }
        }
#if SDL_VERSION_ATLEAST(3, 5, 0)
        if (state->isSteamController) {
            supportedButtonFlags |= SDL_GamepadHasCapSense(state->controller, SDL_GAMEPAD_CAPSENSE_LEFT_STICK) ?
                                        STEAM_LEFT_STICK_TOUCH_FLAG : 0;
            supportedButtonFlags |= SDL_GamepadHasCapSense(state->controller, SDL_GAMEPAD_CAPSENSE_RIGHT_STICK) ?
                                        STEAM_RIGHT_STICK_TOUCH_FLAG : 0;
            supportedButtonFlags |= SDL_GamepadHasCapSense(state->controller, SDL_GAMEPAD_CAPSENSE_LEFT_GRIP) ?
                                        STEAM_LEFT_GRIP_TOUCH_FLAG : 0;
            supportedButtonFlags |= SDL_GamepadHasCapSense(state->controller, SDL_GAMEPAD_CAPSENSE_RIGHT_GRIP) ?
                                        STEAM_RIGHT_GRIP_TOUCH_FLAG : 0;
        }
#endif

        uint32_t capabilities = 0;
        if (SDL_GamepadHasAxis(state->controller, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) ||
            SDL_GamepadHasAxis(state->controller, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
            capabilities |= LI_CCAP_ANALOG_TRIGGERS;
        }
        if (hapticCaps & ML_HAPTIC_GC_RUMBLE) {
            capabilities |= LI_CCAP_RUMBLE;
        }
        if (hapticCaps & ML_HAPTIC_GC_TRIGGER_RUMBLE) {
            capabilities |= LI_CCAP_TRIGGER_RUMBLE;
        }
        if (hapticCaps & ML_HAPTIC_GC_ADDRESSABLE) {
            capabilities |= LI_CCAP_HAPTICS;
        }
        if (SDL_GetNumGamepadTouchpads(state->controller) > 0) {
            capabilities |= LI_CCAP_TOUCHPAD;
            if (SDL_GetNumGamepadTouchpads(state->controller) > 1) {
                capabilities |= LI_CCAP_DUAL_TOUCHPAD;
            }
        }
        if (SDL_GamepadHasSensor(state->controller, SDL_SENSOR_ACCEL)) {
            capabilities |= LI_CCAP_ACCEL;
        }
        if (SDL_GamepadHasSensor(state->controller, SDL_SENSOR_GYRO)) {
            capabilities |= LI_CCAP_GYRO;
        }
        capabilities |= LI_CCAP_BATTERY_STATE;
        if (SDL_GetBooleanProperty(gamepadProperties, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false)) {
            capabilities |= LI_CCAP_RGB_LED;
        }

        uint8_t type;
        switch (SDL_GetGamepadType(state->controller)) {
        case SDL_GAMEPAD_TYPE_XBOX360 :
        case SDL_GAMEPAD_TYPE_XBOXONE :
            type = LI_CTYPE_XBOX;
            break;
        case SDL_GAMEPAD_TYPE_PS3 :
        case SDL_GAMEPAD_TYPE_PS4 :
        case SDL_GAMEPAD_TYPE_PS5 :
            type = LI_CTYPE_PS;
            break;
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO :
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT :
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT :
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR :
            type = LI_CTYPE_NINTENDO;
            break;
        case SDL_GAMEPAD_TYPE_STEAM:
            type = state->isSteamController ? LI_CTYPE_STEAM : LI_CTYPE_UNKNOWN;
            break;
        default:
            type = LI_CTYPE_UNKNOWN;
            break;
        }

        // If this is a PlayStation controller that doesn't have a touchpad button mapped,
        // we'll allow the Select+PS button combo to act as the touchpad.
        state->clickpadButtonEmulationEnabled =
            !SDL_GamepadHasButton(state->controller, SDL_GAMEPAD_BUTTON_TOUCHPAD) &&
            type == LI_CTYPE_PS;

        LiSendControllerArrivalEvent(state->index, m_GamepadMask, type, supportedButtonFlags, capabilities);

        // Send a power level if it's known at this time
        if (powerState != SDL_POWERSTATE_ERROR && powerState != SDL_POWERSTATE_UNKNOWN) {
            sendGamepadBatteryState(state, powerState, batteryPercentage);
        }
    }
    else if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
        state = findStateForGamepad(event->which);
        if (state != NULL) {
            if (state->mouseEmulationTimer != 0) {
                Session::get()->notifyMouseEmulationMode(false);
                SDL_RemoveTimer(state->mouseEmulationTimer);
            }

            SDL_CloseGamepad(state->controller);

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
    SDL_assert(event->type == SDL_EVENT_JOYSTICK_ADDED);

    if (!SDL_IsGamepad(event->which)) {
        SDL_Joystick* joy = SDL_OpenJoystick(event->which);
        if (joy != nullptr) {
            char guidStr[33];
            SDL_GUIDToString(SDL_GetJoystickGUID(joy), guidStr, sizeof(guidStr));
            const char* name = SDL_GetJoystickName(joy);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Unmapped joystick: %s %s",
                        name ? name : "<UNKNOWN>",
                        guidStr);
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Number of axes: %d | Number of buttons: %d | Number of hats: %d",
                        SDL_GetNumJoystickAxes(joy), SDL_GetNumJoystickButtons(joy),
                        SDL_GetNumJoystickHats(joy));
            SDL_CloseJoystick(joy);
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

    if (m_GamepadState[controllerNumber].controller != nullptr) {
        SDL_RumbleGamepad(m_GamepadState[controllerNumber].controller, lowFreqMotor, highFreqMotor, 30000);
    }
}

void SdlInputHandler::rumbleTriggers(uint16_t controllerNumber, uint16_t leftTrigger, uint16_t rightTrigger)
{
    // Make sure the controller number is within our supported count
    if (controllerNumber >= MAX_GAMEPADS) {
        return;
    }

    if (m_GamepadState[controllerNumber].controller != nullptr) {
        SDL_RumbleGamepadTriggers(m_GamepadState[controllerNumber].controller, leftTrigger, rightTrigger, 30000);
    }
}

void SdlInputHandler::setMotionEventState(uint16_t controllerNumber, uint8_t motionType, uint16_t reportRateHz)
{
    // Make sure the controller number is within our supported count
    if (controllerNumber >= MAX_GAMEPADS) {
        return;
    }

    uint16_t reportRateHzLimit;
    if (Utils::getEnvironmentVariableOverride("SENSOR_REPORT_RATE_LIMIT_HZ", &reportRateHzLimit) &&
        reportRateHz > reportRateHzLimit) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Sensor report rate limited to %u Hz by environment variable",
                    reportRateHzLimit);
        reportRateHz = reportRateHzLimit;
    }

    if (m_GamepadState[controllerNumber].controller != nullptr) {
        uint8_t reportPeriodMs = reportRateHz ? (1000 / reportRateHz) : 0;

        switch (motionType) {
        case LI_MOTION_TYPE_ACCEL:
            m_GamepadState[controllerNumber].accelReportPeriodMs = reportPeriodMs;
            if (!SDL_SetGamepadSensorEnabled(m_GamepadState[controllerNumber].controller,
                                             SDL_SENSOR_ACCEL, reportRateHz != 0)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Failed to %s controller accelerometer: %s",
                            reportRateHz ? "enable" : "disable", SDL_GetError());
            }
            break;

        case LI_MOTION_TYPE_GYRO:
            m_GamepadState[controllerNumber].gyroReportPeriodMs = reportPeriodMs;
            if (!SDL_SetGamepadSensorEnabled(m_GamepadState[controllerNumber].controller,
                                             SDL_SENSOR_GYRO, reportRateHz != 0)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Failed to %s controller gyroscope: %s",
                            reportRateHz ? "enable" : "disable", SDL_GetError());
            }
            break;
        }
    }
}

void SdlInputHandler::setControllerLED(uint16_t controllerNumber, uint8_t r, uint8_t g, uint8_t b)
{
    // Make sure the controller number is within our supported count
    if (controllerNumber >= MAX_GAMEPADS) {
        return;
    }

    if (m_GamepadState[controllerNumber].controller != nullptr) {
        SDL_SetGamepadLED(m_GamepadState[controllerNumber].controller, r, g, b);
    }
}

void SdlInputHandler::setAdaptiveTriggers(uint16_t controllerNumber, DualSenseOutputReport *report){
    if (controllerNumber < MAX_GAMEPADS &&
        // and we have a valid controller
        m_GamepadState[controllerNumber].controller != nullptr &&
        // and it's a PS5 controller
        SDL_GetGamepadType(m_GamepadState[controllerNumber].controller) == SDL_GAMEPAD_TYPE_PS5) {
        SDL_SendGamepadEffect(m_GamepadState[controllerNumber].controller, report, sizeof(*report));
    }

    SDL_free(report);
}

static uint16_t clampUint16(uint64_t value)
{
    return static_cast<uint16_t>(qMin<uint64_t>(value, UINT16_MAX));
}

static void writeUint16LE(uint8_t* destination, uint16_t value)
{
    destination[0] = static_cast<uint8_t>(value);
    destination[1] = static_cast<uint8_t>(value >> 8);
}

void SdlInputHandler::setControllerHaptics(uint16_t controllerNumber, LI_CONTROLLER_HAPTIC_EFFECT* effect)
{
    if (controllerNumber >= MAX_GAMEPADS ||
        m_GamepadState[controllerNumber].controller == nullptr ||
        !m_GamepadState[controllerNumber].isSteamController) {
        SDL_free(effect);
        return;
    }

    uint8_t report[10] = {};
    size_t reportSize;

    switch (effect->kind) {
    case LI_HAPTIC_EFFECT_PULSE:
        report[0] = 0x81;
        report[1] = effect->target;
        writeUint16LE(&report[2], clampUint16(effect->durationUs > 0 ? effect->durationUs : 0));
        writeUint16LE(&report[4], clampUint16(effect->intervalUs));
        writeUint16LE(&report[6], effect->repeatCount);
        reportSize = 8;
        break;
    case LI_HAPTIC_EFFECT_OFF:
    case LI_HAPTIC_EFFECT_TICK:
    case LI_HAPTIC_EFFECT_CLICK:
    case LI_HAPTIC_EFFECT_RUMBLE:
    case LI_HAPTIC_EFFECT_NOISE:
        report[0] = 0x82;
        report[1] = effect->target;
        report[2] = effect->kind;
        report[3] = static_cast<uint8_t>(effect->gainDb);
        reportSize = 4;
        break;
    case LI_HAPTIC_EFFECT_TONE:
        report[0] = 0x83;
        report[1] = effect->target;
        report[2] = static_cast<uint8_t>(effect->gainDb);
        writeUint16LE(&report[3], effect->frequencyHz);
        writeUint16LE(&report[5], clampUint16(effect->durationUs > 0 ? effect->durationUs / 1000 : 0));
        writeUint16LE(&report[7], effect->lfoFrequencyHz);
        report[9] = effect->lfoDepthPercent;
        reportSize = 10;
        break;
    case LI_HAPTIC_EFFECT_LOGARITHMIC_SWEEP:
        report[0] = 0x84;
        report[1] = effect->target;
        report[2] = static_cast<uint8_t>(effect->gainDb);
        writeUint16LE(&report[3], clampUint16(effect->durationUs > 0 ? effect->durationUs / 1000 : 0));
        writeUint16LE(&report[5], effect->startFrequencyHz);
        writeUint16LE(&report[7], effect->endFrequencyHz);
        reportSize = 9;
        break;
    case LI_HAPTIC_EFFECT_SCRIPT:
        report[0] = 0x85;
        report[1] = effect->target;
        report[2] = effect->scriptId;
        report[3] = static_cast<uint8_t>(effect->gainDb);
        reportSize = 4;
        break;
    default:
        SDL_free(effect);
        return;
    }

    if (!SDL_SendGamepadEffect(m_GamepadState[controllerNumber].controller, report, reportSize)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Unable to send Steam Controller haptic effect: %s",
                    SDL_GetError());
    }
    SDL_free(effect);
}

QString SdlInputHandler::getUnmappedGamepads()
{
    QString ret;

    if (SDLC_FAILURE(SDL_InitSubSystem(SDL_INIT_GAMEPAD))) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_GAMEPAD) failed: %s",
                     SDL_GetError());
    }

    MappingManager mappingManager;
    mappingManager.applyMappings();

    int numJoysticks = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks);
    for (int i = 0; i < numJoysticks; i++) {
        if (!SDL_IsGamepad(joysticks[i])) {
            SDL_Joystick* joy = SDL_OpenJoystick(joysticks[i]);
            if (joy != nullptr) {
                char guidStr[33];
                SDL_GUIDToString(SDL_GetJoystickGUID(joy), guidStr, sizeof(guidStr));
                const char* name = SDL_GetJoystickName(joy);
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Unmapped joystick: %s %s",
                            name ? name : "<UNKNOWN>",
                            guidStr);

                int numButtons = SDL_GetNumJoystickButtons(joy);
                int numHats = SDL_GetNumJoystickHats(joy);
                int numAxes = SDL_GetNumJoystickAxes(joy);

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

                SDL_CloseJoystick(joy);
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Unable to open joystick for query: %s",
                            SDL_GetError());
            }
        }
    }
    SDL_free(joysticks);

    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);

    // Flush stale events so they aren't processed by the main session event loop
    SDL_FlushEvents(SDL_EVENT_JOYSTICK_ADDED, SDL_EVENT_JOYSTICK_REMOVED);
    SDL_FlushEvents(SDL_EVENT_GAMEPAD_ADDED, SDL_EVENT_GAMEPAD_REMAPPED);

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
    int numGamepads = 0;
    SDL_JoystickID *gamepads = SDL_GetGamepads(&numGamepads);
    for (int i = 0; i < numGamepads; i++) {
        char guidStr[33];
        SDL_GUIDToString(SDL_GetJoystickGUIDForID(gamepads[i]), guidStr, sizeof(guidStr));

        if (!m_IgnoreDeviceGuids.contains(guidStr, Qt::CaseInsensitive)) {
            mask |= (1 << count++);
        }
    }
    SDL_free(gamepads);

    return mask;
}
