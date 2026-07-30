#include <Limelight.h>
#include "SDL_compat.h"
#include "streaming/session.h"
#include "settings/mappingmanager.h"
#include "path.h"
#include "utils.h"

#ifdef HAVE_WINDOWS_RAW_TOUCHPAD
#include "wintouchpad.h"
#endif

#include <QtGlobal>
#include <QDir>
#include <QGuiApplication>

// Include SDL_syswm.h after Qt headers to avoid X11 macro conflicts on Linux
#include <SDL_syswm.h>

#ifdef Q_OS_WIN32
#include <Windows.h>
#include <Imm.h>
#pragma comment(lib, "imm32.lib")
#endif

SdlInputHandler::SdlInputHandler(StreamingPreferences& prefs, int streamWidth, int streamHeight)
    : m_MultiController(prefs.multiController),
      m_GamepadMouse(prefs.gamepadMouse),
      m_SwapMouseButtons(prefs.swapMouseButtons),
      m_SwapWinAltKeys(prefs.swapWinAltKeys),
      m_ReverseScrollDirection(prefs.reverseScrollDirection),
      m_SwapFaceButtons(prefs.swapFaceButtons),
      m_GamepadQuitCombo(prefs.gamepadQuitCombo),
      m_MouseWasInVideoRegion(false),
      m_PendingMouseButtonsAllUpOnVideoRegionLeave(false),
      m_PointerRegionLockActive(false),
      m_PointerRegionLockToggledByUser(false),
      m_FakeMouseCaptureActive(false),
      m_KeyboardCaptureActive(false),
      m_CaptureSystemKeysMode(prefs.captureSysKeysMode),
      m_MouseCursorCapturedVisibilityState(prefs.showLocalCursor ? SDL_ENABLE : SDL_DISABLE),
      m_LocalCursorMode(prefs.absoluteMouseMode && prefs.showLocalCursor ?
                            LI_CURSOR_MODE_LOCAL : LI_CURSOR_MODE_VIDEO),
      m_RemoteCursorVisible(true),
      m_RemoteCursor(nullptr),
      m_LongPressTimer(0),
      m_StreamWidth(streamWidth),
      m_StreamHeight(streamHeight),
      m_AbsoluteMouseMode(prefs.absoluteMouseMode),
      m_AbsoluteTouchMode(prefs.absoluteTouchMode),
      m_DisabledTouchFeedback(false),
      m_NativeTouchpadEnabled(SDL_GetHintBoolean(SDL_HINT_TRACKPAD_IS_TOUCH_ONLY, SDL_FALSE) == SDL_TRUE),
      m_TouchpadFlushEventQueued(false),
      m_NativeTouchpadTransport(NTT_UNKNOWN),
      m_PendingTouchpadId(0),
      m_PendingTouchpadTimestamp(0),
      m_PendingTouchpadContactCount(0),
      m_ActiveTouchpadId(0),
      m_LastTouchpadScrollTimestamp(0),
#ifdef HAVE_WINDOWS_RAW_TOUCHPAD
      m_ActiveWindowsTouchpadDevice(0),
      m_LastWindowsTouchpadFrameTicks(0),
      m_SuppressedWindowsTouchpadMouseButtons(0),
      m_WindowsTouchpadButtonDown(false),
      m_WindowsTouchpadButtonUsesMouseFallback(false),
      m_WindowsTouchpadWidthMm(0),
      m_WindowsTouchpadHeightMm(0),
#endif
      m_LeftButtonReleaseTimer(0),
      m_RightButtonReleaseTimer(0),
      m_DragTimer(0),
      m_DragButton(0),
      m_NumFingersDown(0)
{
    // System keys are always captured when running without a DE
    if (!WMUtils::isRunningDesktopEnvironment()) {
        m_CaptureSystemKeysMode = StreamingPreferences::CSK_ALWAYS;
    }

    // SDL3 breaks our auto-capture-on-leave logic because the mouse focus has already
    // been lost by the time we attempt to call SDL_CaptureMouse(). Fortunately, SDL3's
    // own auto-capture logic seems to be stable now (unlike SDL2), so we can rely on
    // that instead of our own hack when running on sdl2-compat.
    // https://github.com/libsdl-org/SDL/commit/e54001b02809dcebbb822bd0297919c8c76976a1
    SDL_version ver;
    SDL_GetVersion(&ver);
    m_NeedsManualCaptureOnLeave = !(ver.major == 2 && ver.minor >= 30 && ver.patch >= 50) && !SDL_GetHint("SDL3_VERSION");
    if (m_NeedsManualCaptureOnLeave) {
        // Disable the buggy auto-capture on earlier SDL2 builds
        SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
    }

    // Allow gamepad input when the app doesn't have focus if requested
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, prefs.backgroundGamepad ? "1" : "0");

#if !SDL_VERSION_ATLEAST(2, 0, 15)
    // For older versions of SDL (2.0.14 and earlier), use SDL_HINT_GRAB_KEYBOARD
    SDL_SetHintWithPriority(SDL_HINT_GRAB_KEYBOARD,
                            m_CaptureSystemKeysMode != StreamingPreferences::CSK_OFF ? "1" : "0",
                            SDL_HINT_OVERRIDE);
#endif

    // Opt-out of SDL's built-in Alt+Tab handling while keyboard grab is enabled
    SDL_SetHint(SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, "0");

    // Allow clicks to pass through to us when focusing the window. If we're in
    // absolute mouse mode, this will avoid the user having to click twice to
    // trigger a click on the host if the Moonlight window is not focused. In
    // relative mode, the click event will trigger the mouse to be recaptured.
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    // Enabling extended input reports allows rumble to function on Bluetooth PS4/PS5
    // controllers, but breaks DirectInput applications. We will enable it because
    // it's likely that working rumble is what the user is expecting. If they don't
    // want this behavior, they can override it with the environment variable.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");

    // Populate special key combo configuration
    m_SpecialKeyCombos[KeyComboQuit].keyCombo = KeyComboQuit;
    m_SpecialKeyCombos[KeyComboQuit].keyCode = SDLK_q;
    m_SpecialKeyCombos[KeyComboQuit].scanCode = SDL_SCANCODE_Q;
    m_SpecialKeyCombos[KeyComboQuit].enabled = true;

    m_SpecialKeyCombos[KeyComboUngrabInput].keyCombo = KeyComboUngrabInput;
    m_SpecialKeyCombos[KeyComboUngrabInput].keyCode = SDLK_z;
    m_SpecialKeyCombos[KeyComboUngrabInput].scanCode = SDL_SCANCODE_Z;
    m_SpecialKeyCombos[KeyComboUngrabInput].enabled = WMUtils::isRunningDesktopEnvironment();

    m_SpecialKeyCombos[KeyComboToggleFullScreen].keyCombo = KeyComboToggleFullScreen;
    m_SpecialKeyCombos[KeyComboToggleFullScreen].keyCode = SDLK_x;
    m_SpecialKeyCombos[KeyComboToggleFullScreen].scanCode = SDL_SCANCODE_X;
    m_SpecialKeyCombos[KeyComboToggleFullScreen].enabled = WMUtils::isRunningDesktopEnvironment();

    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].keyCombo = KeyComboToggleStatsOverlay;
    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].keyCode = SDLK_s;
    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].scanCode = SDL_SCANCODE_S;
    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].enabled = true;

    m_SpecialKeyCombos[KeyComboToggleMouseMode].keyCombo = KeyComboToggleMouseMode;
    m_SpecialKeyCombos[KeyComboToggleMouseMode].keyCode = SDLK_m;
    m_SpecialKeyCombos[KeyComboToggleMouseMode].scanCode = SDL_SCANCODE_M;
    m_SpecialKeyCombos[KeyComboToggleMouseMode].enabled = true;

    m_SpecialKeyCombos[KeyComboToggleCursorHide].keyCombo = KeyComboToggleCursorHide;
    m_SpecialKeyCombos[KeyComboToggleCursorHide].keyCode = SDLK_c;
    m_SpecialKeyCombos[KeyComboToggleCursorHide].scanCode = SDL_SCANCODE_C;
    m_SpecialKeyCombos[KeyComboToggleCursorHide].enabled = true;

    m_SpecialKeyCombos[KeyComboToggleMinimize].keyCombo = KeyComboToggleMinimize;
    m_SpecialKeyCombos[KeyComboToggleMinimize].keyCode = SDLK_d;
    m_SpecialKeyCombos[KeyComboToggleMinimize].scanCode = SDL_SCANCODE_D;
    m_SpecialKeyCombos[KeyComboToggleMinimize].enabled = WMUtils::isRunningDesktopEnvironment();

    m_SpecialKeyCombos[KeyComboPasteText].keyCombo = KeyComboPasteText;
    m_SpecialKeyCombos[KeyComboPasteText].keyCode = SDLK_v;
    m_SpecialKeyCombos[KeyComboPasteText].scanCode = SDL_SCANCODE_V;
    m_SpecialKeyCombos[KeyComboPasteText].enabled = true;

    m_SpecialKeyCombos[KeyComboTogglePointerRegionLock].keyCombo = KeyComboTogglePointerRegionLock;
    m_SpecialKeyCombos[KeyComboTogglePointerRegionLock].keyCode = SDLK_l;
    m_SpecialKeyCombos[KeyComboTogglePointerRegionLock].scanCode = SDL_SCANCODE_L;
    m_SpecialKeyCombos[KeyComboTogglePointerRegionLock].enabled = true;

    m_SpecialKeyCombos[KeyComboQuitAndExit].keyCombo = KeyComboQuitAndExit;
    m_SpecialKeyCombos[KeyComboQuitAndExit].keyCode = SDLK_e;
    m_SpecialKeyCombos[KeyComboQuitAndExit].scanCode = SDL_SCANCODE_E;
    m_SpecialKeyCombos[KeyComboQuitAndExit].enabled = true;

    m_SpecialKeyCombos[KeyComboToggleKeyboardGrab].keyCombo = KeyComboToggleKeyboardGrab;
    m_SpecialKeyCombos[KeyComboToggleKeyboardGrab].keyCode = SDLK_k;
    m_SpecialKeyCombos[KeyComboToggleKeyboardGrab].scanCode = SDL_SCANCODE_K;
    m_SpecialKeyCombos[KeyComboToggleKeyboardGrab].enabled = WMUtils::isRunningDesktopEnvironment();

    m_OldIgnoreDevices = SDL_GetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES);
    m_OldIgnoreDevicesExcept = SDL_GetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT);

    QString streamIgnoreDevices = qgetenv("STREAM_GAMECONTROLLER_IGNORE_DEVICES");
    QString streamIgnoreDevicesExcept = qgetenv("STREAM_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT");

    if (!streamIgnoreDevices.isEmpty() && !streamIgnoreDevices.endsWith(',')) {
        streamIgnoreDevices += ',';
    }
    streamIgnoreDevices += m_OldIgnoreDevices;

    // STREAM_IGNORE_DEVICE_GUIDS allows to specify additional devices to be ignored when starting
    // the stream in case the scope of STREAM_GAMECONTROLLER_IGNORE_DEVICES is too broad. One such
    // case is "Steam Virtual Gamepad" where everything is under the same VID/PID, but different GUIDs.
    // Multiple GUIDs can be provided, but need to be separated by commas:
    //
    //     <GUID>,<GUID>,<GUID>,...
    //
    QString streamIgnoreDeviceGuids = qgetenv("STREAM_IGNORE_DEVICE_GUIDS");
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    m_IgnoreDeviceGuids = streamIgnoreDeviceGuids.split(',', Qt::SkipEmptyParts);
#else
    m_IgnoreDeviceGuids = streamIgnoreDeviceGuids.split(',', QString::SkipEmptyParts);
#endif

    // For SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES, we use the union of SDL_GAMECONTROLLER_IGNORE_DEVICES
    // and STREAM_GAMECONTROLLER_IGNORE_DEVICES while streaming. STREAM_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT
    // overrides SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT while streaming.
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES, streamIgnoreDevices.toUtf8());
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, streamIgnoreDevicesExcept.toUtf8());

    // We must initialize joystick explicitly before gamecontroller in order
    // to ensure we receive gamecontroller attach events for gamepads where
    // SDL doesn't have a built-in mapping. By starting joystick first, we
    // can allow mapping manager to update the mappings before GC attach
    // events are generated.
    SDL_assert(!SDL_WasInit(SDL_INIT_JOYSTICK));
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_JOYSTICK) failed: %s",
                     SDL_GetError());
    }

    MappingManager mappingManager;
    mappingManager.applyMappings();

    // Flush gamepad arrival and departure events which may be queued before
    // starting the gamecontroller subsystem again. This prevents us from
    // receiving duplicate arrival and departure events for the same gamepad.
    SDL_FlushEvent(SDL_CONTROLLERDEVICEADDED);
    SDL_FlushEvent(SDL_CONTROLLERDEVICEREMOVED);

    // We need to reinit this each time, since you only get
    // an initial set of gamepad arrival events once per init.
    SDL_assert(!SDL_WasInit(SDL_INIT_GAMECONTROLLER));
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) failed: %s",
                     SDL_GetError());
    }

#if !SDL_VERSION_ATLEAST(2, 0, 9)
    SDL_assert(!SDL_WasInit(SDL_INIT_HAPTIC));
    if (SDL_InitSubSystem(SDL_INIT_HAPTIC) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_HAPTIC) failed: %s",
                     SDL_GetError());
    }
#endif

    // Initialize the gamepad mask with currently attached gamepads to avoid
    // causing gamepads to unexpectedly disappear and reappear on the host
    // during stream startup as we detect currently attached gamepads one at a time.
    m_GamepadMask = getAttachedGamepadMask();

    SDL_zero(m_GamepadState);
    SDL_zero(m_LastTouchDownEvent);
    SDL_zero(m_LastTouchUpEvent);
    SDL_zero(m_TouchDownEvent);
}

SdlInputHandler::~SdlInputHandler()
{
#ifdef HAVE_WINDOWS_RAW_TOUCHPAD
    m_WindowsTouchpadInput.reset();
#endif
    cancelNativeTouchpadContacts();
    resetRemoteCursor();

    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (m_GamepadState[i].mouseEmulationTimer != 0) {
            Session::get()->notifyMouseEmulationMode(false);
            SDL_RemoveTimer(m_GamepadState[i].mouseEmulationTimer);
        }
#if !SDL_VERSION_ATLEAST(2, 0, 9)
        if (m_GamepadState[i].haptic != nullptr) {
            SDL_HapticClose(m_GamepadState[i].haptic);
        }
#endif
        if (m_GamepadState[i].controller != nullptr) {
            SDL_GameControllerClose(m_GamepadState[i].controller);
        }
    }

    SDL_RemoveTimer(m_LongPressTimer);
    SDL_RemoveTimer(m_LeftButtonReleaseTimer);
    SDL_RemoveTimer(m_RightButtonReleaseTimer);
    SDL_RemoveTimer(m_DragTimer);

#if !SDL_VERSION_ATLEAST(2, 0, 9)
    SDL_QuitSubSystem(SDL_INIT_HAPTIC);
    SDL_assert(!SDL_WasInit(SDL_INIT_HAPTIC));
#endif

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_assert(!SDL_WasInit(SDL_INIT_GAMECONTROLLER));

    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    SDL_assert(!SDL_WasInit(SDL_INIT_JOYSTICK));

    // Return background event handling to off
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");

    // Restore the ignored devices
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES, m_OldIgnoreDevices.toUtf8());
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, m_OldIgnoreDevicesExcept.toUtf8());

#ifdef STEAM_LINK
    // Hide SDL's cursor on Steam Link after quitting the stream.
    // FIXME: We should also do this for other situations where SDL
    // and Qt will draw their own mouse cursors like KMSDRM or RPi
    // video backends.
    SDL_ShowCursor(SDL_DISABLE);
#endif
}

void SdlInputHandler::setWindow(SDL_Window *window)
{
    m_Window = window;

#ifdef HAVE_WINDOWS_RAW_TOUCHPAD
    if (m_NativeTouchpadEnabled && !m_WindowsTouchpadInput) {
        auto windowsTouchpadInput = std::make_unique<WindowsTouchpadInput>(this);
        if (windowsTouchpadInput->initialize(window)) {
            m_WindowsTouchpadInput = std::move(windowsTouchpadInput);
        }
    }
#endif

#ifdef Q_OS_WIN32
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(m_Window, &info) && info.subsystem == SDL_SYSWM_WINDOWS) {
        ImmAssociateContext(info.info.win.window, NULL);
    }
#endif
}

int SdlInputHandler::getLocalCursorMode() const
{
    return m_LocalCursorMode.load(std::memory_order_relaxed);
}

int SdlInputHandler::getCapturedCursorVisibilityState() const
{
    if (m_MouseCursorCapturedVisibilityState == SDL_DISABLE) {
        return SDL_DISABLE;
    }

    if (getLocalCursorMode() == LI_CURSOR_MODE_LOCAL &&
        (LiGetHostFeatureFlags() & LI_FF_CURSOR_SHAPE) != 0) {
        return m_RemoteCursorVisible ? SDL_ENABLE : SDL_DISABLE;
    }

    return SDL_ENABLE;
}

void SdlInputHandler::applyCapturedCursorState()
{
    if (getLocalCursorMode() == LI_CURSOR_MODE_LOCAL &&
        m_RemoteCursor != nullptr) {
        SDL_SetCursor(m_RemoteCursor);
    }
    SDL_ShowCursor(getCapturedCursorVisibilityState());
}

void SdlInputHandler::resetRemoteCursor()
{
    if (m_RemoteCursor == nullptr) {
        return;
    }

    if (SDL_GetCursor() == m_RemoteCursor) {
        SDL_SetCursor(SDL_GetDefaultCursor());
    }
    SDL_FreeCursor(m_RemoteCursor);
    m_RemoteCursor = nullptr;
}

void SdlInputHandler::synchronizeLocalCursorMode()
{
    const int cursorMode = m_AbsoluteMouseMode &&
                           m_MouseCursorCapturedVisibilityState == SDL_ENABLE ?
                               LI_CURSOR_MODE_LOCAL : LI_CURSOR_MODE_VIDEO;
    m_LocalCursorMode.store(cursorMode, std::memory_order_relaxed);
    const int result = LiSetCursorMode(cursorMode);
    if (result != LI_CURSOR_MODE_OK &&
        result != LI_CURSOR_MODE_ERR_UNSUPPORTED &&
        result != LI_CURSOR_MODE_ERR_NOT_CONNECTED) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to synchronize local cursor mode: %d",
                    result);
    }

    if (cursorMode == LI_CURSOR_MODE_VIDEO) {
        resetRemoteCursor();
    }
    else {
        // The host will immediately send the authoritative state. Keep the
        // default cursor visible until that update arrives.
        m_RemoteCursorVisible = true;
    }
}

void SdlInputHandler::updateRemoteCursor(const RemoteCursorUpdate& update)
{
    m_RemoteCursorVisible = update.visible;

    if (update.hasShape) {
        const qsizetype expectedSize =
            static_cast<qsizetype>(update.width) * update.height * 4;
        if (update.width == 0 || update.height == 0 ||
            update.hotspotX < 0 || update.hotspotX >= update.width ||
            update.hotspotY < 0 || update.hotspotY >= update.height ||
            update.bgra.size() != expectedSize) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Ignoring invalid remote cursor shape %u",
                        update.shapeId);
        }
        else {
            SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
                const_cast<char*>(update.bgra.constData()),
                update.width,
                update.height,
                32,
                update.width * 4,
                SDL_PIXELFORMAT_BGRA32);
            if (surface == nullptr) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Failed to create remote cursor surface: %s",
                            SDL_GetError());
            }
            else {
                SDL_Cursor* cursor =
                    SDL_CreateColorCursor(surface, update.hotspotX, update.hotspotY);
                SDL_FreeSurface(surface);
                if (cursor == nullptr) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Failed to create remote cursor: %s",
                                SDL_GetError());
                }
                else {
                    resetRemoteCursor();
                    m_RemoteCursor = cursor;
                }
            }
        }
    }

    if (isCaptureActive()) {
        applyCapturedCursorState();
    }
}

void SdlInputHandler::raiseAllKeys(bool clearKeys)
{
    if (m_KeysDown.isEmpty()) {
        return;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Raising %d keys%s",
                (int)m_KeysDown.count(),
                clearKeys ? "" : " (keeping local state for retry)");

    int failedCount = 0;
    auto keysDown = m_KeysDown;

    for (auto keyDown : std::as_const(keysDown)) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                     "Raising key: vk=0x%04x",
                     (int)keyDown);

        int rc = LiSendKeyboardEvent(keyDown, KEY_ACTION_UP, 0);
        if (rc == 0) {
            if (clearKeys) {
                m_KeysDown.remove(keyDown);
            }
        }
        else {
            failedCount++;
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "LiSendKeyboardEvent failed while raising key: rc=%d vk=0x%04x",
                        rc,
                        (int)keyDown);
        }
    }

    if (clearKeys && failedCount != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Keeping %d keys marked down for a later retry",
                    failedCount);
    }
}

void SdlInputHandler::notifyMouseLeave()
{
    if (m_NeedsManualCaptureOnLeave) {
        // SDL on Windows doesn't send the mouse button up until the mouse re-enters the window
        // after leaving it. This breaks some of the Aero snap gestures, so we'll capture it to
        // allow us to receive the mouse button up events later.
        //
        // On macOS and X11, capturing the mouse allows us to receive mouse motion outside the
        // window (button up already worked without capture).
        if (m_AbsoluteMouseMode && isCaptureActive()) {
            // NB: Not using SDL_GetGlobalMouseState() because we want our state not the system's
            Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);
            for (Uint32 button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_X2; button++) {
                if (mouseState & SDL_BUTTON(button)) {
                    SDL_CaptureMouse(SDL_TRUE);
                    break;
                }
            }
        }
    }
}

void SdlInputHandler::notifyFocusLost()
{
    Uint32 windowFlags = SDL_GetWindowFlags(m_Window);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Input focus lost: windowFlags=0x%08x keysDown=%d capture=%d",
                (unsigned int)windowFlags,
                (int)m_KeysDown.count(),
                isCaptureActive() ? 1 : 0);

    cancelNativeTouchpadContacts();

    // Release mouse cursor when another window is activated (e.g. by using ALT+TAB).
    // This lets user to interact with our window's title bar and with the buttons in it.
    // Doing this while the window is full-screen breaks the transition out of FS
    // (desktop and exclusive), so we must check for that before releasing mouse capture.
    if (!(windowFlags & SDL_WINDOW_FULLSCREEN) && !m_AbsoluteMouseMode) {
        setCaptureActive(false);
    }

    // Raise all keys that are currently pressed. If we don't do this, certain keys
    // used in shortcuts that cause focus loss (such as Alt+Tab) may get stuck down.
    raiseAllKeys();
}

void SdlInputHandler::notifyFocusGained()
{
#ifdef Q_OS_WIN32
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(m_Window, &info) && info.subsystem == SDL_SYSWM_WINDOWS) {
        ImmAssociateContext(info.info.win.window, NULL);
    }
#endif
}

bool SdlInputHandler::isCaptureActive()
{
    if (SDL_GetRelativeMouseMode()) {
        return true;
    }

    // Some platforms don't support SDL_SetRelativeMouseMode
    return m_FakeMouseCaptureActive;
}

void SdlInputHandler::updateKeyboardGrabState()
{
    bool shouldGrab = m_CaptureSystemKeysMode != StreamingPreferences::CSK_OFF && isCaptureActive();
    if (shouldGrab) {
        Uint32 windowFlags = SDL_GetWindowFlags(m_Window);
        if (m_CaptureSystemKeysMode == StreamingPreferences::CSK_FULLSCREEN &&
            !(windowFlags & SDL_WINDOW_FULLSCREEN)) {
            // Ungrab if it's fullscreen only and we left fullscreen
            shouldGrab = false;
        }
    }

    // Don't close the window on Alt+F4 when keyboard grab is enabled
    SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, shouldGrab ? "1" : "0");

#if SDL_VERSION_ATLEAST(2, 0, 15)
    // On SDL 2.0.15+, we can get keyboard-only grab on Win32, X11, and Wayland.
    // SDL 2.0.18 adds keyboard grab on macOS (if built with non-AppStore APIs).
    SDL_SetWindowKeyboardGrab(m_Window, shouldGrab ? SDL_TRUE : SDL_FALSE);
#endif

    m_KeyboardCaptureActive = shouldGrab;
}

bool SdlInputHandler::isSystemKeyCaptureActive()
{
    if (m_CaptureSystemKeysMode == StreamingPreferences::CSK_OFF) {
        return false;
    }

    if (m_Window == nullptr) {
        return false;
    }

    // NB: We used to check SDL_WINDOW_KEYBOARD_GRABBED here, but this isn't
    // always set when capture "fails" on SDL3, even though the user may have
    // configured the compositor to pass through system keys to us anyway.
    // See issues #1776 and #1900 for details.
    Uint32 windowFlags = SDL_GetWindowFlags(m_Window);
    if (!(windowFlags & SDL_WINDOW_INPUT_FOCUS) || !m_KeyboardCaptureActive) {
        return false;
    }

    if (m_CaptureSystemKeysMode == StreamingPreferences::CSK_FULLSCREEN &&
            !(windowFlags & SDL_WINDOW_FULLSCREEN)) {
        return false;
    }

    return true;
}

void SdlInputHandler::setCaptureActive(bool active)
{
    if (active) {
        // If we're in relative mode, try to activate SDL's relative mouse mode
        if (m_AbsoluteMouseMode || SDL_SetRelativeMouseMode(SDL_TRUE) < 0) {
            m_FakeMouseCaptureActive = true;
            // Relative mouse mode didn't work or was disabled, so apply the
            // cursor shape and visibility directly.
            applyCapturedCursorState();
        }

        // Synchronize the client and host cursor when activating absolute capture
        if (m_AbsoluteMouseMode) {
            int mouseX, mouseY;
            int windowX, windowY;

            // We have to use SDL_GetGlobalMouseState() because macOS may not reflect
            // the new position of the mouse when outside the window.
            SDL_GetGlobalMouseState(&mouseX, &mouseY);

            // Convert global mouse state to window-relative
            SDL_GetWindowPosition(m_Window, &windowX, &windowY);
            mouseX -= windowX;
            mouseY -= windowY;

            if (isMouseInVideoRegion(mouseX, mouseY)) {
                // Synthesize a mouse event to synchronize the cursor
                SDL_MouseMotionEvent motionEvent = {};
                motionEvent.type = SDL_MOUSEMOTION;
                motionEvent.timestamp = SDL_GetTicks();
                motionEvent.windowID = SDL_GetWindowID(m_Window);
                motionEvent.x = mouseX;
                motionEvent.y = mouseY;
                handleMouseMotionEvent(&motionEvent);
            }
        }
    }
    else {
        // A capture transition can happen without a window focus event (for
        // example, when the user explicitly releases input). Ensure Sunshine
        // never retains contacts from the previous capture state.
        cancelNativeTouchpadContacts();

        if (m_RemoteCursor != nullptr && SDL_GetCursor() == m_RemoteCursor) {
            SDL_SetCursor(SDL_GetDefaultCursor());
        }

        if (m_FakeMouseCaptureActive) {
            // Display the cursor again
            SDL_ShowCursor(SDL_ENABLE);
            m_FakeMouseCaptureActive = false;
        }
        else {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }
    }

    // Update mouse pointer region constraints
    updatePointerRegionLock();

    // Now update the keyboard grab
    updateKeyboardGrabState();
}

void SdlInputHandler::handleTouchFingerEvent(SDL_TouchFingerEvent* event)
{
#if SDL_VERSION_ATLEAST(2, 0, 10)
    SDL_TouchDeviceType deviceType = SDL_GetTouchDeviceType(event->touchId);
    if (deviceType == SDL_TOUCH_DEVICE_INDIRECT_ABSOLUTE && m_NativeTouchpadEnabled) {
        handleNativeTouchpadEvent(event);
        return;
    }
    else if (deviceType == SDL_TOUCH_DEVICE_INDIRECT_RELATIVE && m_NativeTouchpadEnabled) {
        // Relative indirect devices don't provide physical surface coordinates,
        // so they cannot be represented by the native touchpad protocol.
        handleRelativeFingerEvent(event);
        return;
    }
    else if (deviceType != SDL_TOUCH_DEVICE_DIRECT) {
        // With native touchpad input disabled, SDL leaves trackpads on the
        // traditional mouse path. Ignore any other indirect touch callbacks.
        return;
    }
#elif defined(Q_OS_DARWIN)
    // SDL2 sends touch events from trackpads by default on
    // macOS. This totally screws our actual mouse handling,
    // so we must explicitly ignore touch events on macOS
    // until SDL 2.0.10 where we have SDL_GetTouchDeviceType()
    // to tell them apart.
    return;
#endif

    if (m_AbsoluteTouchMode) {
        handleAbsoluteFingerEvent(event);
    }
    else if (m_NativeTouchpadEnabled) {
        // A touchscreen in virtual trackpad mode provides the same normalized
        // contact data as an indirect absolute touchpad. Reuse the native
        // touchpad transport so the host can handle multi-touch gestures, with
        // the existing software pointer path as the compatibility fallback.
        handleNativeTouchpadEvent(event);
    }
    else {
        handleRelativeFingerEvent(event);
    }
}
