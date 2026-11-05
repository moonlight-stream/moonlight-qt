#pragma once

#include "settings/streamingpreferences.h"
#include "backend/computermanager.h"

#include "SDL_compat.h"

struct GamepadState {
    SDL_Gamepad * controller;
    SDL_JoystickID jsId;
    short index;
    bool isSteamController;

    SDL_TimerID mouseEmulationTimer;
    uint32_t lastStartDownTime;

    bool clickpadButtonEmulationEnabled;
    bool emulatedClickpadButtonDown;

    uint8_t gyroReportPeriodMs;
    float lastGyroEventData[SDL_arraysize(SDL_GamepadSensorEvent::data)];
    Uint64 lastGyroEventTime;
    Uint64 lastGyroDebugTime;
    Uint64 lastTouchpadDebugTime;

    uint8_t accelReportPeriodMs;
    float lastAccelEventData[SDL_arraysize(SDL_GamepadSensorEvent::data)];
    Uint64 lastAccelEventTime;

    uint32_t buttons;
    short lsX, lsY;
    short rsX, rsY;
    unsigned char lt, rt;
};


struct DualSenseOutputReport{
    uint8_t validFlag0;
    uint8_t validFlag1;

    /* For DualShock 4 compatibility mode. */
    uint8_t motorRight;
    uint8_t motorLeft;

    /* Audio controls */
    uint8_t reserved[4];
    uint8_t muteButtonLed;

    uint8_t powerSaveControl;
    uint8_t rightTriggerEffectType;
    uint8_t rightTriggerEffect[DS_EFFECT_PAYLOAD_SIZE];
    uint8_t leftTriggerEffectType;
    uint8_t leftTriggerEffect[DS_EFFECT_PAYLOAD_SIZE];
    uint8_t reserved2[6];

    /* LEDs and lightbar */
    uint8_t validFlag2;
    uint8_t reserved3[2];
    uint8_t lightbarSetup;
    uint8_t ledBrightness;
    uint8_t playerLeds;
    uint8_t lightbarRed;
    uint8_t lightbarGreen;
    uint8_t lightbarBlue;
};

// activeGamepadMask is a short, so we're bounded by the number of mask bits
#define MAX_GAMEPADS 16

#define MAX_FINGERS 2

class SdlInputHandler
{
public:
    explicit SdlInputHandler(StreamingPreferences& prefs, int streamWidth, int streamHeight);

    ~SdlInputHandler();

    void setWindow(SDL_Window* window);

    void handleKeyEvent(SDL_KeyboardEvent* event);

    void handleMouseButtonEvent(SDL_MouseButtonEvent* event);

    void handleMouseMotionEvent(SDL_MouseMotionEvent* event);

    void handleMouseWheelEvent(SDL_MouseWheelEvent* event);

    void handleControllerAxisEvent(SDL_GamepadAxisEvent* event);

    void handleControllerButtonEvent(SDL_GamepadButtonEvent* event);

    void handleControllerDeviceEvent(SDL_GamepadDeviceEvent* event);

    void handleControllerSensorEvent(SDL_GamepadSensorEvent* event);

    void handleControllerTouchpadEvent(SDL_GamepadTouchpadEvent* event);

#if SDL_VERSION_ATLEAST(3, 5, 0)
    void handleControllerCapSenseEvent(SDL_GamepadCapSenseEvent* event);
#endif

    void handleJoystickBatteryEvent(SDL_JoyBatteryEvent* event);

    void handleJoystickArrivalEvent(SDL_JoyDeviceEvent* event);

    void sendText(QString& string);

    void rumble(uint16_t controllerNumber, uint16_t lowFreqMotor, uint16_t highFreqMotor);

    void rumbleTriggers(uint16_t controllerNumber, uint16_t leftTrigger, uint16_t rightTrigger);

    void setMotionEventState(uint16_t controllerNumber, uint8_t motionType, uint16_t reportRateHz);

    void setControllerLED(uint16_t controllerNumber, uint8_t r, uint8_t g, uint8_t b);

    void setAdaptiveTriggers(uint16_t controllerNumber, DualSenseOutputReport *report);

    void setControllerHaptics(uint16_t controllerNumber, LI_CONTROLLER_HAPTIC_EFFECT* effect);

    void handleTouchFingerEvent(SDL_TouchFingerEvent* event);

    int getAttachedGamepadMask();

    void raiseAllKeys();

    void notifyMouseLeave();

    void notifyFocusLost();

    void notifyFocusGained();

    bool isCaptureActive();

    bool isSystemKeyCaptureActive();

    void setCaptureActive(bool active);

    bool isMouseInVideoRegion(int mouseX, int mouseY, int windowWidth = -1, int windowHeight = -1);

    void updateKeyboardGrabState();

    void updatePointerRegionLock();

    static
    QString getUnmappedGamepads();

private:
    enum KeyCombo {
        KeyComboQuit,
        KeyComboUngrabInput,
        KeyComboToggleFullScreen,
        KeyComboToggleStatsOverlay,
        KeyComboToggleMouseMode,
        KeyComboToggleCursorHide,
        KeyComboToggleMinimize,
        KeyComboPasteText,
        KeyComboTogglePointerRegionLock,
        KeyComboQuitAndExit,
        KeyComboToggleKeyboardGrab,
        KeyComboMax
    };

    GamepadState*
    findStateForGamepad(SDL_JoystickID id);

    static
    uint32_t getButtonFlag(const GamepadState* state, SDL_GamepadButton button);

    void sendGamepadState(GamepadState* state);

    void sendGamepadBatteryState(GamepadState* state, SDL_PowerState powerState, int percentage);

    void handleAbsoluteFingerEvent(SDL_TouchFingerEvent* event);

    void emulateAbsoluteFingerEvent(SDL_TouchFingerEvent* event);

    void disableTouchFeedback();

    void handleRelativeFingerEvent(SDL_TouchFingerEvent* event);

    void performSpecialKeyCombo(KeyCombo combo);

    static
    Uint32 longPressTimerCallback(void* param, SDL_TimerID timerId, Uint32 interval);

    static
    Uint32 mouseEmulationTimerCallback(void* param, SDL_TimerID timerId, Uint32 interval);

    static
    Uint32 releaseLeftButtonTimerCallback(void* param, SDL_TimerID timerId, Uint32 interval);

    static
    Uint32 releaseRightButtonTimerCallback(void* param, SDL_TimerID timerId, Uint32 interval);

    static
    Uint32 dragTimerCallback(void* param, SDL_TimerID timerId, Uint32 interval);

    SDL_Window* m_Window;
    bool m_MultiController;
    bool m_GamepadMouse;
    bool m_SwapMouseButtons;
    bool m_ReverseScrollDirection;
    bool m_SwapFaceButtons;

    bool m_NeedsManualCaptureOnLeave;
    bool m_MouseWasInVideoRegion;
    bool m_PendingMouseButtonsAllUpOnVideoRegionLeave;
    bool m_PointerRegionLockActive;
    bool m_PointerRegionLockToggledByUser;

    int m_GamepadMask;
    GamepadState m_GamepadState[MAX_GAMEPADS];
    QSet<uint32_t> m_KeysDown;
    bool m_FakeMouseCaptureActive;
    bool m_KeyboardCaptureActive;
    QString m_OldIgnoreDevices;
    QString m_OldIgnoreDevicesExcept;
    QStringList m_IgnoreDeviceGuids;
    StreamingPreferences::CaptureSysKeysMode m_CaptureSystemKeysMode;
    bool m_MouseCursorCapturedVisibilityState;

    struct {
        KeyCombo keyCombo;
        SDL_Keycode keyCode;
        SDL_Scancode scanCode;
        bool enabled;
    } m_SpecialKeyCombos[KeyComboMax];

    SDL_TouchFingerEvent m_LastTouchDownEvent;
    SDL_TouchFingerEvent m_LastTouchUpEvent;
    SDL_TimerID m_LongPressTimer;
    int m_StreamWidth;
    int m_StreamHeight;
    bool m_AbsoluteMouseMode;
    bool m_AbsoluteTouchMode;
    bool m_DisabledTouchFeedback;

    SDL_TouchFingerEvent m_TouchDownEvent[MAX_FINGERS];
    SDL_TimerID m_LeftButtonReleaseTimer;
    SDL_TimerID m_RightButtonReleaseTimer;
    SDL_TimerID m_DragTimer;
    char m_DragButton;
    int m_NumFingersDown;

    static const uint32_t k_ButtonMap[];
};
