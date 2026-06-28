#include "streaming/session.h"

#include <Limelight.h>
#include "SDL_compat.h"
#include "settings/mappingmanager.h"

#include <QStringList>
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
#define ML_HAPTIC_SIMPLE_RUMBLE     (1U << 17)
#define ML_HAPTIC_GC_TRIGGER_RUMBLE (1U << 18)

#define SONY_VENDOR_ID 0x054c
#define DUALSENSE_EDGE_PRODUCT_ID 0x0df2

// SDL 2's DualSense Edge HIDAPI driver exposes the Edge-specific controls as
// raw joystick buttons 17-20. Its generated mapping is paddle1:b20,
// paddle2:b19, paddle3:b18, paddle4:b17, matching right rear, left rear,
// right Fn, and left Fn. Older controller DB entries identify the Edge as a
// generic PS5 controller and omit these bindings. Once mapped, SDL reports
// those raw buttons as controller buttons 16-19, matching the PADDLE1-4 order
// in k_ButtonMap.
#define DUALSENSE_EDGE_MIN_BUTTONS 21
#define DUALSENSE_EDGE_PADDLE_FLAGS (PADDLE1_FLAG | PADDLE2_FLAG | PADDLE3_FLAG | PADDLE4_FLAG)
#define DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE1 16
#define DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE2 17
#define DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE3 18
#define DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE4 19
#define DUALSENSE_EDGE_PADDLE_MAPPING_COUNT 4

static const char* const DUALSENSE_EDGE_PADDLE_MAPPING_KEYS[DUALSENSE_EDGE_PADDLE_MAPPING_COUNT] = {
    "paddle1",
    "paddle2",
    "paddle3",
    "paddle4",
};

static const char* const DUALSENSE_EDGE_PADDLE_MAPPING_VALUES[DUALSENSE_EDGE_PADDLE_MAPPING_COUNT] = {
    "b20",
    "b19",
    "b18",
    "b17",
};

const int SdlInputHandler::k_ButtonMap[] = {
    A_FLAG, B_FLAG, X_FLAG, Y_FLAG,
    BACK_FLAG, SPECIAL_FLAG, PLAY_FLAG,
    LS_CLK_FLAG, RS_CLK_FLAG,
    LB_FLAG, RB_FLAG,
    UP_FLAG, DOWN_FLAG, LEFT_FLAG, RIGHT_FLAG,
    MISC_FLAG,
    PADDLE1_FLAG, PADDLE2_FLAG, PADDLE3_FLAG, PADDLE4_FLAG,
    TOUCHPAD_FLAG,
};

static bool isDualSenseEdgeController(SDL_GameController* controller)
{
#if SDL_VERSION_ATLEAST(2, 0, 6)
    return SDL_GameControllerGetVendor(controller) == SONY_VENDOR_ID &&
           SDL_GameControllerGetProduct(controller) == DUALSENSE_EDGE_PRODUCT_ID;
#else
    return false;
#endif
}

static int dualSenseEdgeJoystickButtonCount(SDL_GameController* controller)
{
    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
    return joystick != nullptr ? SDL_JoystickNumButtons(joystick) : -1;
}

static bool dualSenseEdgeExposesPaddleButtons(SDL_GameController* controller)
{
    if (!isDualSenseEdgeController(controller)) {
        return false;
    }

    return dualSenseEdgeJoystickButtonCount(controller) >= DUALSENSE_EDGE_MIN_BUTTONS;
}

static const char* dualSenseEdgePaddleButtonName(Uint8 button)
{
    switch (button) {
    case DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE1:
        return "PADDLE1";
    case DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE2:
        return "PADDLE2";
    case DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE3:
        return "PADDLE3";
    case DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE4:
        return "PADDLE4";
    default:
        return nullptr;
    }
}

static bool dualSenseEdgeHasPaddleControllerButtons(SDL_GameController* controller)
{
#if SDL_VERSION_ATLEAST(2, 0, 14)
    // Raw joystick button count only proves SDL can see the physical controls.
    // The controller-button layer is what Moonlight can transmit as paddles.
    return SDL_GameControllerHasButton(controller, (SDL_GameControllerButton)DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE1) &&
           SDL_GameControllerHasButton(controller, (SDL_GameControllerButton)DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE2) &&
           SDL_GameControllerHasButton(controller, (SDL_GameControllerButton)DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE3) &&
           SDL_GameControllerHasButton(controller, (SDL_GameControllerButton)DUALSENSE_EDGE_CONTROLLER_BUTTON_PADDLE4);
#else
    return false;
#endif
}

static QString dualSenseEdgePaddleMappingEntry(int mappingIndex)
{
    return QString::fromLatin1(DUALSENSE_EDGE_PADDLE_MAPPING_KEYS[mappingIndex]) + ":" +
           QString::fromLatin1(DUALSENSE_EDGE_PADDLE_MAPPING_VALUES[mappingIndex]);
}

static int dualSenseEdgePaddleMappingIndex(const QString& mappingEntry)
{
    for (int i = 0; i < DUALSENSE_EDGE_PADDLE_MAPPING_COUNT; i++) {
        QString prefix = QString::fromLatin1(DUALSENSE_EDGE_PADDLE_MAPPING_KEYS[i]) + ":";
        if (mappingEntry.startsWith(prefix)) {
            return i;
        }
    }

    return -1;
}

static void addDualSenseEdgePaddleMappingChange(QString* changedMappings, int mappingIndex)
{
    if (!changedMappings->isEmpty()) {
        changedMappings->append(",");
    }
    changedMappings->append(dualSenseEdgePaddleMappingEntry(mappingIndex));
}

static QString normalizeDualSenseEdgePaddleMappings(const char* mapping, QString* changedMappings)
{
    QStringList entries = QString::fromUtf8(mapping).split(',');
    QStringList updatedEntries;
    bool foundMappings[DUALSENSE_EDGE_PADDLE_MAPPING_COUNT] = {};

    changedMappings->clear();

    // Normalize existing paddle entries too, because a stale mapping can expose
    // paddle buttons while assigning the wrong raw DualSense Edge buttons.
    for (int i = 0; i < entries.size(); i++) {
        const QString& entry = entries.at(i);
        int mappingIndex = dualSenseEdgePaddleMappingIndex(entry);

        if (mappingIndex < 0) {
            updatedEntries.append(entry);
            continue;
        }

        if (foundMappings[mappingIndex]) {
            addDualSenseEdgePaddleMappingChange(changedMappings, mappingIndex);
            continue;
        }

        QString expectedEntry = dualSenseEdgePaddleMappingEntry(mappingIndex);
        updatedEntries.append(expectedEntry);
        foundMappings[mappingIndex] = true;

        if (entry != expectedEntry) {
            addDualSenseEdgePaddleMappingChange(changedMappings, mappingIndex);
        }
    }

    int insertionIndex = -1;
    for (int i = 0; i < updatedEntries.size(); i++) {
        if (updatedEntries.at(i).startsWith("platform:")) {
            insertionIndex = i;
            break;
        }
    }
    if (insertionIndex < 0) {
        insertionIndex = updatedEntries.size();
        if (!updatedEntries.isEmpty() && updatedEntries.last().isEmpty()) {
            insertionIndex--;
        }
    }

    for (int i = 0; i < DUALSENSE_EDGE_PADDLE_MAPPING_COUNT; i++) {
        if (!foundMappings[i]) {
            updatedEntries.insert(insertionIndex++, dualSenseEdgePaddleMappingEntry(i));
            addDualSenseEdgePaddleMappingChange(changedMappings, i);
        }
    }

    return updatedEntries.join(",");
}

static bool addDualSenseEdgePaddleMapping(SDL_GameController* controller)
{
#if !SDL_VERSION_ATLEAST(2, 0, 14)
    Q_UNUSED(controller);
    return false;
#else
    if (!isDualSenseEdgeController(controller)) {
        return false;
    }

    int buttonCount = dualSenseEdgeJoystickButtonCount(controller);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "DualSense Edge detected with %d SDL joystick buttons",
                buttonCount);

    if (!dualSenseEdgeExposesPaddleButtons(controller)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "DualSense Edge detected, but SDL exposes %d buttons; need at least %d for Edge-specific buttons",
                    buttonCount,
                    DUALSENSE_EDGE_MIN_BUTTONS);
        return false;
    }

    char* mapping = SDL_GameControllerMapping(controller);
    if (mapping == nullptr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Unable to query DualSense Edge controller mapping: %s",
                    SDL_GetError());
        return false;
    }

    QString changedMappings;
    QString updatedMapping = normalizeDualSenseEdgePaddleMappings(mapping, &changedMappings);
    if (changedMappings.isEmpty()) {
        if (dualSenseEdgeHasPaddleControllerButtons(controller)) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "DualSense Edge paddle mappings already present");
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "DualSense Edge paddle mappings are present, but SDL did not expose the paddle controller buttons");
        }
        SDL_free(mapping);
        return false;
    }
    SDL_free(mapping);

    int ret = SDL_GameControllerAddMapping(qPrintable(updatedMapping));
    if (ret < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to add DualSense Edge paddle mappings: %s",
                    SDL_GetError());
        return false;
    }
    else if (ret == 0 && !dualSenseEdgeHasPaddleControllerButtons(controller)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "DualSense Edge paddle mapping update did not expose the paddle controller buttons");
        return false;
    }
    else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Applied DualSense Edge paddle mappings (%s): %s",
                    ret > 0 ? "added" : "updated",
                    qPrintable(changedMappings));
        return ret > 0;
    }
#endif
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
    int buttons = state->buttons;
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

void SdlInputHandler::sendGamepadBatteryState(GamepadState* state, SDL_JoystickPowerLevel level)
{
    uint8_t batteryPercentage;
    uint8_t batteryState;

    // SDL's battery reporting capabilities are quite limited. Notably, we cannot
    // tell the battery level while charging (or even if a battery is present).
    // We also cannot tell the percentage of charge exactly in any case.
    switch (level)
    {
    case SDL_JOYSTICK_POWER_UNKNOWN:
        batteryState = LI_BATTERY_STATE_UNKNOWN;
        batteryPercentage = LI_BATTERY_PERCENTAGE_UNKNOWN;
        break;
    case SDL_JOYSTICK_POWER_WIRED:
        batteryState = LI_BATTERY_STATE_CHARGING;
        batteryPercentage = LI_BATTERY_PERCENTAGE_UNKNOWN;
        break;
    case SDL_JOYSTICK_POWER_EMPTY:
        batteryState = LI_BATTERY_STATE_DISCHARGING;
        batteryPercentage = 5;
        break;
    case SDL_JOYSTICK_POWER_LOW:
        batteryState = LI_BATTERY_STATE_DISCHARGING;
        batteryPercentage = 20;
        break;
    case SDL_JOYSTICK_POWER_MEDIUM:
        batteryState = LI_BATTERY_STATE_DISCHARGING;
        batteryPercentage = 50;
        break;
    case SDL_JOYSTICK_POWER_FULL:
        batteryState = LI_BATTERY_STATE_DISCHARGING;
        batteryPercentage = 90;
        break;
    default:
        return;
    }

    LiSendControllerBatteryEvent(state->index, batteryState, batteryPercentage);
}

Uint32 SdlInputHandler::mouseEmulationTimerCallback(Uint32 interval, void *param)
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
    if (event->button >= SDL_arraysize(k_ButtonMap)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "No mapping for gamepad button: %u",
                    event->button);
        return;
    }

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

    const char* edgeButtonName = dualSenseEdgePaddleButtonName(event->button);
    if (edgeButtonName != nullptr && isDualSenseEdgeController(state->controller)) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                     "DualSense Edge %s %s (paddle/Fn flags: 0x%08x)",
                     edgeButtonName,
                     event->state == SDL_PRESSED ? "pressed" : "released",
                     (unsigned int)(state->buttons & DUALSENSE_EDGE_PADDLE_FLAGS));
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

#if SDL_VERSION_ATLEAST(2, 0, 14)

void SdlInputHandler::handleControllerSensorEvent(SDL_ControllerSensorEvent* event)
{
    GamepadState* state = findStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    switch (event->sensor) {
    case SDL_SENSOR_ACCEL:
        if (state->accelReportPeriodMs &&
                SDL_TICKS_PASSED(event->timestamp, state->lastAccelEventTime + state->accelReportPeriodMs) &&
                memcmp(event->data, state->lastAccelEventData, sizeof(event->data)) != 0) {
            memcpy(state->lastAccelEventData, event->data, sizeof(event->data));
            state->lastAccelEventTime = event->timestamp;

            LiSendControllerMotionEvent((uint8_t)state->index, LI_MOTION_TYPE_ACCEL, event->data[0], event->data[1], event->data[2]);
        }
        break;
    case SDL_SENSOR_GYRO:
        if (state->gyroReportPeriodMs &&
                SDL_TICKS_PASSED(event->timestamp, state->lastGyroEventTime + state->gyroReportPeriodMs) &&
                memcmp(event->data, state->lastGyroEventData, sizeof(event->data)) != 0) {
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

void SdlInputHandler::handleControllerTouchpadEvent(SDL_ControllerTouchpadEvent* event)
{
    GamepadState* state = findStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    uint8_t eventType;
    switch (event->type) {
    case SDL_CONTROLLERTOUCHPADDOWN:
        eventType = LI_TOUCH_EVENT_DOWN;
        break;
    case SDL_CONTROLLERTOUCHPADUP:
        eventType = LI_TOUCH_EVENT_UP;
        break;
    case SDL_CONTROLLERTOUCHPADMOTION:
        eventType = LI_TOUCH_EVENT_MOVE;
        break;
    default:
        return;
    }

    LiSendControllerTouchEvent2((uint8_t)state->index, eventType,
                                (uint8_t)event->touchpad, event->finger,
                                event->x, event->y, event->pressure);
}

#endif

#if SDL_VERSION_ATLEAST(2, 24, 0)

void SdlInputHandler::handleJoystickBatteryEvent(SDL_JoyBatteryEvent* event)
{
    GamepadState* state = findStateForGamepad(event->which);
    if (state == NULL) {
        return;
    }

    sendGamepadBatteryState(state, event->level);
}

#endif

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

        controller = SDL_GameControllerOpen(event->which);
        if (controller == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to open gamepad: %s",
                         SDL_GetError());
            return;
        }

        // SDL_CONTROLLERDEVICEADDED can be reported multiple times for the same
        // gamepad in rare cases, because SDL doesn't fixup the device index in
        // the SDL_CONTROLLERDEVICEADDED event if an unopened gamepad disappears
        // before we've processed the add event.
        for (int i = 0; i < MAX_GAMEPADS; i++) {
            if (m_GamepadState[i].controller == controller) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Received duplicate add event for controller index: %d",
                            event->which);
                SDL_GameControllerClose(controller);
                return;
            }
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
            SDL_GameControllerClose(controller);
            return;
        }

        SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(controller)),
                                  guidStr, sizeof(guidStr));
        if (m_IgnoreDeviceGuids.contains(guidStr, Qt::CaseInsensitive))
        {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Skipping ignored device with GUID: %s",
                        guidStr);
            SDL_GameControllerClose(controller);
            return;
        }

        if (addDualSenseEdgePaddleMapping(controller)) {
            SDL_GameControllerClose(controller);
            controller = SDL_GameControllerOpen(event->which);
            if (controller == NULL) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to reopen DualSense Edge after applying paddle mappings: %s",
                             SDL_GetError());
                return;
            }

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Reopened DualSense Edge after adding paddle mappings");
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

        hapticCaps = 0;
#if SDL_VERSION_ATLEAST(2, 0, 18)
        hapticCaps |= SDL_GameControllerHasRumble(controller) ? ML_HAPTIC_GC_RUMBLE : 0;
        hapticCaps |= SDL_GameControllerHasRumbleTriggers(controller) ? ML_HAPTIC_GC_TRIGGER_RUMBLE : 0;
#elif SDL_VERSION_ATLEAST(2, 0, 9)
        // Perform a tiny rumbles to see if haptics are supported.
        // NB: We cannot use zeros for rumble intensity or SDL will not actually call the JS driver
        // and we'll get a (potentially false) success value returned.
        hapticCaps |= SDL_GameControllerRumble(controller, 1, 1, 1) == 0 ? ML_HAPTIC_GC_RUMBLE : 0;
#if SDL_VERSION_ATLEAST(2, 0, 14)
        hapticCaps |= SDL_GameControllerRumbleTriggers(controller, 1, 1, 1) == 0 ? ML_HAPTIC_GC_TRIGGER_RUMBLE : 0;
#endif
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

        mapping = SDL_GameControllerMapping(state->controller);
        name = SDL_GameControllerName(state->controller);

        uint16_t vendorId = 0;
        uint16_t productId = 0;
#if SDL_VERSION_ATLEAST(2, 0, 6)
        vendorId = SDL_GameControllerGetVendor(state->controller);
        productId = SDL_GameControllerGetProduct(state->controller);
#endif
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

        SDL_JoystickPowerLevel powerLevel = SDL_JoystickCurrentPowerLevel(SDL_GameControllerGetJoystick(state->controller));

#if SDL_VERSION_ATLEAST(2, 0, 14)
        // On SDL 2.0.14 and later, we can provide enhanced controller information to the host PC
        // for it to use as a hint for the type of controller to emulate.
        uint32_t supportedButtonFlags = 0;
        for (int i = 0; i < (int)SDL_arraysize(k_ButtonMap); i++) {
            if (SDL_GameControllerHasButton(state->controller, (SDL_GameControllerButton)i)) {
                supportedButtonFlags |= k_ButtonMap[i];
            }
        }
        bool isDualSenseEdge = isDualSenseEdgeController(state->controller);
        int dualSenseEdgeButtonCount = isDualSenseEdge ? dualSenseEdgeJoystickButtonCount(state->controller) : 0;
        bool hasDualSenseEdgePaddleButtons = isDualSenseEdge && dualSenseEdgeHasPaddleControllerButtons(state->controller);
        if (hasDualSenseEdgePaddleButtons) {
            supportedButtonFlags |= DUALSENSE_EDGE_PADDLE_FLAGS;
        }

        uint32_t capabilities = 0;
        if (SDL_GameControllerGetBindForAxis(state->controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT).bindType == SDL_CONTROLLER_BINDTYPE_AXIS ||
            SDL_GameControllerGetBindForAxis(state->controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT).bindType == SDL_CONTROLLER_BINDTYPE_AXIS) {
            // We assume these are analog triggers if the binding is to an axis rather than a button
            capabilities |= LI_CCAP_ANALOG_TRIGGERS;
        }
        if (hapticCaps & ML_HAPTIC_GC_RUMBLE) {
            capabilities |= LI_CCAP_RUMBLE;
        }
        if (hapticCaps & ML_HAPTIC_GC_TRIGGER_RUMBLE) {
            capabilities |= LI_CCAP_TRIGGER_RUMBLE;
        }
        if (SDL_GameControllerGetNumTouchpads(state->controller) > 0) {
            capabilities |= LI_CCAP_TOUCHPAD;
            if (SDL_GameControllerGetNumTouchpads(state->controller) > 1) {
                capabilities |= LI_CCAP_DUAL_TOUCHPAD;
            }
        }
        if (SDL_GameControllerHasSensor(state->controller, SDL_SENSOR_ACCEL)) {
            capabilities |= LI_CCAP_ACCEL;
        }
        if (SDL_GameControllerHasSensor(state->controller, SDL_SENSOR_GYRO)) {
            capabilities |= LI_CCAP_GYRO;
        }
        if (powerLevel != SDL_JOYSTICK_POWER_UNKNOWN || SDL_VERSION_ATLEAST(2, 24, 0)) {
            capabilities |= LI_CCAP_BATTERY_STATE;
        }
        if (SDL_GameControllerHasLED(state->controller)) {
            capabilities |= LI_CCAP_RGB_LED;
        }

        uint8_t type;
        switch (SDL_GameControllerGetType(state->controller)) {
        case SDL_CONTROLLER_TYPE_XBOX360:
        case SDL_CONTROLLER_TYPE_XBOXONE:
            type = LI_CTYPE_XBOX;
            break;
        case SDL_CONTROLLER_TYPE_PS3:
        case SDL_CONTROLLER_TYPE_PS4:
        case SDL_CONTROLLER_TYPE_PS5:
            type = LI_CTYPE_PS;
            break;
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
#if SDL_VERSION_ATLEAST(2, 24, 0)
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
#endif
            type = LI_CTYPE_NINTENDO;
            break;
        default:
            type = LI_CTYPE_UNKNOWN;
            break;
        }

        // If this is a PlayStation controller that doesn't have a touchpad button mapped,
        // we'll allow the Select+PS button combo to act as the touchpad.
        state->clickpadButtonEmulationEnabled =
#if SDL_VERSION_ATLEAST(2, 0, 14)
            SDL_GameControllerGetBindForButton(state->controller, SDL_CONTROLLER_BUTTON_TOUCHPAD).bindType == SDL_CONTROLLER_BINDTYPE_NONE &&
#endif
            type == LI_CTYPE_PS;

        if (isDualSenseEdge) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "DualSense Edge arrival support: buttons=%d supportedButtonFlags=0x%08x paddle/Fn=0x%08x capabilities=0x%08x",
                        dualSenseEdgeButtonCount,
                        (unsigned int)supportedButtonFlags,
                        (unsigned int)(supportedButtonFlags & DUALSENSE_EDGE_PADDLE_FLAGS),
                        (unsigned int)capabilities);
        }

        LiSendControllerArrivalEvent(state->index, m_GamepadMask, type, supportedButtonFlags, capabilities);
#else

        // Send an empty event to tell the PC we've arrived
        sendGamepadState(state);
#endif

        // Send a power level if it's known at this time
        if (powerLevel != SDL_JOYSTICK_POWER_UNKNOWN) {
            sendGamepadBatteryState(state, powerLevel);
        }
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

void SdlInputHandler::rumbleTriggers(uint16_t controllerNumber, uint16_t leftTrigger, uint16_t rightTrigger)
{
    // Make sure the controller number is within our supported count
    if (controllerNumber >= MAX_GAMEPADS) {
        return;
    }

#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (m_GamepadState[controllerNumber].controller != nullptr) {
        SDL_GameControllerRumbleTriggers(m_GamepadState[controllerNumber].controller, leftTrigger, rightTrigger, 30000);
    }
#endif
}

void SdlInputHandler::setMotionEventState(uint16_t controllerNumber, uint8_t motionType, uint16_t reportRateHz)
{
    // Make sure the controller number is within our supported count
    if (controllerNumber >= MAX_GAMEPADS) {
        return;
    }

#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (m_GamepadState[controllerNumber].controller != nullptr) {
        uint8_t reportPeriodMs = reportRateHz ? (1000 / reportRateHz) : 0;

        switch (motionType) {
        case LI_MOTION_TYPE_ACCEL:
            m_GamepadState[controllerNumber].accelReportPeriodMs = reportPeriodMs;
            SDL_GameControllerSetSensorEnabled(m_GamepadState[controllerNumber].controller, SDL_SENSOR_ACCEL, reportRateHz ? SDL_TRUE : SDL_FALSE);
            break;

        case LI_MOTION_TYPE_GYRO:
            m_GamepadState[controllerNumber].gyroReportPeriodMs = reportPeriodMs;
            SDL_GameControllerSetSensorEnabled(m_GamepadState[controllerNumber].controller, SDL_SENSOR_GYRO, reportRateHz ? SDL_TRUE : SDL_FALSE);
            break;
        }
    }
#endif
}

void SdlInputHandler::setControllerLED(uint16_t controllerNumber, uint8_t r, uint8_t g, uint8_t b)
{
    // Make sure the controller number is within our supported count
    if (controllerNumber >= MAX_GAMEPADS) {
        return;
    }

#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (m_GamepadState[controllerNumber].controller != nullptr) {
        SDL_GameControllerSetLED(m_GamepadState[controllerNumber].controller, r, g, b);
    }
#endif
}

void SdlInputHandler::setAdaptiveTriggers(uint16_t controllerNumber, DualSenseOutputReport *report){

#if SDL_VERSION_ATLEAST(2, 0, 16)
        // Make sure the controller number is within our supported count
    if (controllerNumber <= MAX_GAMEPADS &&
        // and we have a valid controller
        m_GamepadState[controllerNumber].controller != nullptr &&
        // and it's a PS5 controller
        SDL_GameControllerGetType(m_GamepadState[controllerNumber].controller) == SDL_CONTROLLER_TYPE_PS5) {
        SDL_GameControllerSendEffect(m_GamepadState[controllerNumber].controller, report, sizeof(*report));
    }
#endif

    SDL_free(report);
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

    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; i++) {
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

    // Flush stale events so they aren't processed by the main session event loop
    SDL_FlushEvents(SDL_JOYDEVICEADDED, SDL_JOYDEVICEREMOVED);
    SDL_FlushEvents(SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMAPPED);

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
    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; i++) {
        if (SDL_IsGameController(i)) {
            char guidStr[33];
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i),
                                      guidStr, sizeof(guidStr));

            if (!m_IgnoreDeviceGuids.contains(guidStr, Qt::CaseInsensitive))
            {
                mask |= (1 << count++);
            }
        }
    }

    return mask;
}
