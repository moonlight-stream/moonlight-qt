#pragma once

#include "settings/streamingpreferences.h"
#include "backend/computermanager.h"
#include "cursorshapeclassifier.h"

#include "SDL_compat.h"

#include <QByteArray>
#include <QHash>
#include <QSet>

#include <atomic>

#ifdef HAVE_WINDOWS_RAW_TOUCHPAD
#include <memory>

class WindowsTouchpadInput;
#endif

struct GamepadState {
    SDL_GameController* controller;
    SDL_JoystickID jsId;
    short index;

#if !SDL_VERSION_ATLEAST(2, 0, 9)
    SDL_Haptic* haptic;
    int hapticMethod;
    int hapticEffectId;
#endif

    SDL_TimerID mouseEmulationTimer;
    uint32_t lastStartDownTime;

    bool clickpadButtonEmulationEnabled;
    bool emulatedClickpadButtonDown;

#if SDL_VERSION_ATLEAST(2, 0, 14)
    uint8_t gyroReportPeriodMs;
    float lastGyroEventData[SDL_arraysize(SDL_ControllerSensorEvent::data)];
    uint32_t lastGyroEventTime;

    uint8_t accelReportPeriodMs;
    float lastAccelEventData[SDL_arraysize(SDL_ControllerSensorEvent::data)];
    uint32_t lastAccelEventTime;
#endif

    int buttons;
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

#define MAX_TOUCHPAD_FRAME_CONTACTS 5

#define TOUCHPAD_SCROLL_SUPPRESSION_TIMEOUT_MS 500

// 主机（至少 Sunshine + DXGI 桌面复制）会把"光标可见"这一位翻来翻去：实测每秒五到
// 十次成对的 hidden → shown，形状和 shapeId 都没变。照做就是肉眼可见的闪烁。所以隐藏
// 要等它稳定这么久才生效，显示立即生效 —— 真正的隐藏（游戏自己藏光标）会一直保持，
// 只是晚这么点生效，看不出来；而成对的抖动会被整段吞掉。
#define REMOTE_CURSOR_HIDE_DEBOUNCE_MS 150

#ifdef HAVE_MACOS_NATIVE_TOUCHPAD
// macOS reports trackpad contacts and promoted mouse clicks through separate
// SDL event paths. Keep the click correlation window deliberately short to
// avoid consuming an unrelated click from a physical mouse.
#define MACOS_TOUCHPAD_TAP_MAX_DURATION_MS 500
#define MACOS_TOUCHPAD_CLICK_CORRELATION_TIMEOUT_MS 250
#define MACOS_TOUCHPAD_TAP_MAX_MOVEMENT 0.01f
#endif

#define GAMEPAD_HAPTIC_METHOD_NONE 0
#define GAMEPAD_HAPTIC_METHOD_LEFTRIGHT 1
#define GAMEPAD_HAPTIC_METHOD_SIMPLERUMBLE 2

#define GAMEPAD_HAPTIC_SIMPLE_HIFREQ_MOTOR_WEIGHT 0.33
#define GAMEPAD_HAPTIC_SIMPLE_LOWFREQ_MOTOR_WEIGHT 0.8

struct RemoteCursorUpdate {
    bool hasShape;
    bool visible;
    uint32_t shapeId;
    uint16_t width;
    uint16_t height;
    int16_t hotspotX;
    int16_t hotspotY;
    QByteArray bgra;
};

class SdlInputHandler
{
public:
    explicit SdlInputHandler(StreamingPreferences& prefs, int streamWidth, int streamHeight,
                             bool enablePhysicalDualSenseHaptics);

    ~SdlInputHandler();

    void setWindow(SDL_Window* window);

    void handleKeyEvent(SDL_KeyboardEvent* event);

    void handleMouseButtonEvent(SDL_MouseButtonEvent* event);

    void handleMouseMotionEvent(SDL_MouseMotionEvent* event);

    void handleMouseWheelEvent(SDL_MouseWheelEvent* event);

    void handleControllerAxisEvent(SDL_ControllerAxisEvent* event);

    void handleControllerButtonEvent(SDL_ControllerButtonEvent* event);

    void handleControllerDeviceEvent(SDL_ControllerDeviceEvent* event);

#if SDL_VERSION_ATLEAST(2, 0, 14)
    void handleControllerSensorEvent(SDL_ControllerSensorEvent* event);

    void handleControllerTouchpadEvent(SDL_ControllerTouchpadEvent* event);
#endif

#if SDL_VERSION_ATLEAST(2, 24, 0)
    void handleJoystickBatteryEvent(SDL_JoyBatteryEvent* event);
#endif

    void handleJoystickArrivalEvent(SDL_JoyDeviceEvent* event);

    void sendText(QString& string);

    void rumble(uint16_t controllerNumber, uint16_t lowFreqMotor, uint16_t highFreqMotor);

    void rumbleTriggers(uint16_t controllerNumber, uint16_t leftTrigger, uint16_t rightTrigger);

    void setMotionEventState(uint16_t controllerNumber, uint8_t motionType, uint16_t reportRateHz);

    void setControllerLED(uint16_t controllerNumber, uint8_t r, uint8_t g, uint8_t b);

    void setAdaptiveTriggers(uint16_t controllerNumber, DualSenseOutputReport *report);

    void handleTouchFingerEvent(SDL_TouchFingerEvent* event);

#ifdef HAVE_WINDOWS_PEN_INPUT
    bool handleWindowsPenPointerMessage(unsigned int message, Uint64 wParam);
#endif

    void flushPendingTouchpadFrameEvent();

    // 去抖窗口到期，把主机要求的隐藏落实下去
    void flushPendingRemoteCursorHide();

    int getAttachedGamepadMask();

    void raiseAllKeys(bool clearKeys = true);
    bool hasKeysDown() const { return !m_KeysDown.isEmpty(); }

    void notifyMouseLeave();

    void notifyFocusLost();

    void notifyFocusGained();

    bool isCaptureActive();

    bool isSystemKeyCaptureActive();

    void setCaptureActive(bool active);

    bool isMouseInVideoRegion(int mouseX, int mouseY, int windowWidth = -1, int windowHeight = -1);

    void updateKeyboardGrabState();

    void updatePointerRegionLock();

    void updateRemoteCursor(const RemoteCursorUpdate& update);

    // 显示器变化后按新的 backing 比例重建远端光标（只有 macOS 需要）
    void refreshRemoteCursorScale();

    void synchronizeLocalCursorMode();

    int getLocalCursorMode() const;

    static
    QString getUnmappedGamepads();

    // KeyCombo 枚举（公开以便 OverlayMenu 等外部组件调用）
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

    // 公开 performSpecialKeyCombo 以便从悬浮菜单调用
    void performSpecialKeyCombo(KeyCombo combo);

    // Toggle gamepad mouse emulation for the first connected gamepad
    bool toggleGamepadMouseEmulation();

    // Check if any gamepad has mouse emulation currently active
    bool isMouseEmulationActive();

    // Update the gamepad mouse setting at runtime
    void setGamepadMouse(bool enabled) { m_GamepadMouse = enabled; }

private:
    qreal getRemoteCursorScale() const;

    GamepadState*
    findStateForGamepad(SDL_JoystickID id);

    void sendGamepadState(GamepadState* state);

    void sendGamepadBatteryState(GamepadState* state, SDL_JoystickPowerLevel level);

    void handleAbsoluteFingerEvent(SDL_TouchFingerEvent* event);

    void emulateAbsoluteFingerEvent(SDL_TouchFingerEvent* event);

    void disableTouchFeedback();
    static bool isPenTouchDevice(SDL_TouchID touchId);

#ifdef HAVE_WINDOWS_PEN_INPUT
    bool initializeWindowsPenInput();
    bool trySendWindowsPenCancel();
    void cancelWindowsPenInput(bool suppressPointer = false);
    void routeWindowsPenPointerToSdl(Uint32 pointerId);
    void shutdownWindowsPenInput(bool suppressPointer = false);
#endif

    void handleRelativeFingerEvent(SDL_TouchFingerEvent* event);

    void cancelRelativeTouchpadState();

    void handleNativeTouchpadEvent(SDL_TouchFingerEvent* event);

    void sendPendingTouchpadFrame();

    void cancelSdlTouchpadContacts();

    void cancelNativeTouchpadContacts();

    void selectNativeTouchpadTransport();

    void transitionNativeTouchpadToSoftwarePointer();

    int getCapturedCursorVisibilityState() const;

    void applyCapturedCursorState();

    void resetRemoteCursor();

    // 换上新的远端光标并接手它的所有权，顺带放掉旧的那只
    void installRemoteCursor(SDL_Cursor* cursor);

    // 认位图里的标准形状，认出来就换成本机的系统光标。返回 false 表示没认出来，
    // 调用方该回退去画主机位图。
    bool tryUseNativeRemoteCursor(const RemoteCursorUpdate& update);

    // 应用主机推来的显隐状态：显示立即生效，隐藏要等去抖窗口坐实。
    void updateRemoteCursorVisibility(bool visible);

    void cancelPendingRemoteCursorHide();

    struct NativeTouchpadContact {
        uint8_t eventType;
        uint32_t pointerId;
        float x;
        float y;
        float pressure;
    };

    void sendNativeTouchpadContacts(const NativeTouchpadContact* contacts, int contactCount,
                                    bool transitionToSoftwarePointer = true,
                                    uint8_t buttonState = 0,
                                    uint16_t deviceWidthMm = 0,
                                    uint16_t deviceHeightMm = 0);

#ifdef HAVE_MACOS_NATIVE_TOUCHPAD
    void updateMacTouchpadGesture(const SDL_TouchFingerEvent* event);

    bool sendMacTouchpadButtonState(bool down);

    bool shouldSuppressMacTouchpadMouseButtonEvent(const SDL_MouseButtonEvent* event);

    void resetMacTouchpadState();
#endif

#ifdef HAVE_WINDOWS_RAW_TOUCHPAD
    void handleWindowsTouchpadFrame(uint64_t deviceId,
                                    const uint32_t* pointerIds,
                                    const float* x, const float* y, const float* pressure,
                                    const uint8_t* touching,
                                    int contactCount, bool hasContactFrame,
                                    bool buttonDown, uint16_t deviceWidthMm,
                                    uint16_t deviceHeightMm);

    void cancelWindowsTouchpadContacts(uint64_t deviceId = 0);

    void sendWindowsTouchpadMouseButton(bool down);

    bool shouldSuppressWindowsTouchpadMouseEvent(Uint32 mouseId);
    bool shouldSuppressWindowsTouchpadMouseButtonEvent(const SDL_MouseButtonEvent* event);

    friend class WindowsTouchpadInput;
#endif

    static
    Uint32 longPressTimerCallback(Uint32 interval, void* param);

    static
    Uint32 mouseEmulationTimerCallback(Uint32 interval, void* param);

    static
    Uint32 releaseLeftButtonTimerCallback(Uint32 interval, void* param);

    static
    Uint32 releaseRightButtonTimerCallback(Uint32 interval, void* param);

    static
    Uint32 dragTimerCallback(Uint32 interval, void* param);

    static
    Uint32 remoteCursorHideTimerCallback(Uint32 interval, void* param);

    SDL_Window* m_Window;
    bool m_MultiController;
    bool m_GamepadMouse;
    bool m_EnableDualSenseHaptics;
    bool m_SwapMouseButtons;
    bool m_SwapWinAltKeys;
    bool m_ReverseScrollDirection;
    bool m_SwapFaceButtons;
    StreamingPreferences::GamepadQuitCombo m_GamepadQuitCombo;

    bool m_NeedsManualCaptureOnLeave;
    bool m_MouseWasInVideoRegion;
    bool m_PendingMouseButtonsAllUpOnVideoRegionLeave;
    bool m_PointerRegionLockActive;
    bool m_PointerRegionLockToggledByUser;

    int m_GamepadMask;
    GamepadState m_GamepadState[MAX_GAMEPADS];
    QSet<short> m_KeysDown;
    bool m_FakeMouseCaptureActive;
    bool m_KeyboardCaptureActive;
    QString m_OldIgnoreDevices;
    QString m_OldIgnoreDevicesExcept;
    QStringList m_IgnoreDeviceGuids;
    StreamingPreferences::CaptureSysKeysMode m_CaptureSystemKeysMode;
    int m_MouseCursorCapturedVisibilityState;
    std::atomic<int> m_LocalCursorMode;
    bool m_RemoteCursorVisible;
    SDL_Cursor* m_RemoteCursor;
    // 上一次的识别结果。用来挡掉"主机重复推同一形状"时的无谓重建，也用来只在结果
    // 变化时打一次未识别的度量日志。
    NativeCursorShape m_LastCursorClass;
    // 最后一份成功建出光标的位图形状，以及当时用的 backing 比例。换成系统光标时会
    // 清掉 m_HasLastCursorShape —— 系统光标不受 backing 比例影响，不需要重建。
    RemoteCursorUpdate m_LastCursorShape;
    bool m_HasLastCursorShape;
    qreal m_RemoteCursorScale;
    // 隐藏的去抖定时器。见 updateRemoteCursorVisibility()。
    SDL_TimerID m_RemoteCursorHideTimer;

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

#ifdef HAVE_WINDOWS_PEN_INPUT
    void* m_WindowsPenWindow;
    void* m_WindowsPenSubclassContext;
    Uint32 m_WindowsPenPointerId;
    Uint32 m_WindowsPenFallbackPointerId;
    Uint32 m_WindowsPenSuppressedPointerId;
    bool m_WindowsPenSubclassInstalled;
    bool m_WindowsPenPointerTracked;
    bool m_WindowsPenCancelPending;
#endif

    enum NativeTouchpadTransport {
        NTT_UNKNOWN,
        NTT_FRAME,
        NTT_INDIVIDUAL,
        NTT_SOFTWARE_POINTER,
    };

    bool m_NativeTouchpadEnabled;
    bool m_TouchpadFlushEventQueued;
    NativeTouchpadTransport m_NativeTouchpadTransport;
    SDL_TouchID m_PendingTouchpadId;
    Uint32 m_PendingTouchpadTimestamp;
    int m_PendingTouchpadContactCount;
    NativeTouchpadContact m_PendingTouchpadContacts[MAX_TOUCHPAD_FRAME_CONTACTS];
    SDL_TouchID m_ActiveTouchpadId;
    QHash<SDL_FingerID, NativeTouchpadContact> m_ActiveTouchpadContacts;
    QSet<SDL_FingerID> m_IgnoredTouchpadContacts;
    Uint32 m_LastTouchpadScrollTimestamp;

#ifdef HAVE_MACOS_NATIVE_TOUCHPAD
    Uint32 m_MacTouchpadSuppressedMouseButtons;
    Uint32 m_MacTouchpadPendingTapButtons;
    Uint32 m_MacTouchpadLastTapTimestamp;
    Uint32 m_MacTouchpadGestureStartTimestamp;
    SDL_FingerID m_MacTouchpadGesturePrimaryFinger;
    QSet<SDL_FingerID> m_MacTouchpadGestureContacts;
    float m_MacTouchpadGestureStartX;
    float m_MacTouchpadGestureStartY;
    int m_MacTouchpadGestureMaxContacts;
    bool m_MacTouchpadGestureMoved;
    bool m_MacTouchpadGestureHadPhysicalButton;
    bool m_MacTouchpadButtonDown;
#endif

#ifdef HAVE_WINDOWS_RAW_TOUCHPAD
    QHash<uint32_t, NativeTouchpadContact> m_ActiveWindowsTouchpadContacts;
    uint64_t m_ActiveWindowsTouchpadDevice;
    Uint32 m_LastWindowsTouchpadFrameTicks;
    Uint32 m_SuppressedWindowsTouchpadMouseButtons;
    bool m_WindowsTouchpadButtonDown;
    bool m_WindowsTouchpadButtonUsesMouseFallback;
    uint16_t m_WindowsTouchpadWidthMm;
    uint16_t m_WindowsTouchpadHeightMm;
    std::unique_ptr<WindowsTouchpadInput> m_WindowsTouchpadInput;
#endif

    SDL_TouchFingerEvent m_TouchDownEvent[MAX_FINGERS];
    SDL_TimerID m_LeftButtonReleaseTimer;
    SDL_TimerID m_RightButtonReleaseTimer;
    SDL_TimerID m_DragTimer;
    char m_DragButton;
    int m_NumFingersDown;

    static const int k_ButtonMap[];
};
