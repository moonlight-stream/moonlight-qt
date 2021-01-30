#include <Limelight.h>
#include <SDL.h>
#include "streaming/session.h"
#include "settings/mappingmanager.h"
#include "path.h"
#include "utils.h"

#include <QtGlobal>
#include <QDir>
#include <QGuiApplication>

#define MOUSE_POLLING_INTERVAL 5

#ifdef Q_OS_WIN32
HHOOK g_KeyboardHook;
#endif

SdlInputHandler::SdlInputHandler(StreamingPreferences& prefs, NvComputer*, int streamWidth, int streamHeight)
    : m_MultiController(prefs.multiController),
      m_GamepadMouse(prefs.gamepadMouse),
      m_SwapMouseButtons(prefs.swapMouseButtons),
      m_ReverseScrollDirection(prefs.reverseScrollDirection),
      m_SwapFaceButtons(prefs.swapFaceButtons),
      m_MouseMoveTimer(0),
      m_MousePositionLock(0),
      m_MouseWasInVideoRegion(false),
      m_PendingMouseButtonsAllUpOnVideoRegionLeave(false),
      m_FakeCaptureActive(false),
      m_CaptureSystemKeysEnabled(prefs.captureSysKeys || !WMUtils::isRunningWindowManager()),
      m_MouseCursorCapturedVisibilityState(SDL_DISABLE),
      m_PendingKeyCombo(KeyComboMax),
      m_LongPressTimer(0),
      m_StreamWidth(streamWidth),
      m_StreamHeight(streamHeight),
      m_AbsoluteMouseMode(prefs.absoluteMouseMode),
      m_AbsoluteTouchMode(prefs.absoluteTouchMode),
      m_PendingMouseLeaveButtonUp(0),
      m_LeftButtonReleaseTimer(0),
      m_RightButtonReleaseTimer(0),
      m_DragTimer(0),
      m_DragButton(0),
      m_NumFingersDown(0)
{
    // Allow gamepad input when the app doesn't have focus if requested
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, prefs.backgroundGamepad ? "1" : "0");

    // If absolute mouse mode is enabled, use relative mode warp (which
    // is via normal motion events that are influenced by mouse acceleration).
    // Otherwise, we'll use raw input capture which is straight from the device
    // without modification by the OS.
    SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP,
                            prefs.absoluteMouseMode ? "1" : "0",
                            SDL_HINT_OVERRIDE);

    // If we're grabbing system keys, enable the X11 keyboard grab in SDL
    SDL_SetHintWithPriority(SDL_HINT_GRAB_KEYBOARD,
                            m_CaptureSystemKeysEnabled ? "1" : "0",
                            SDL_HINT_OVERRIDE);

    // Allow clicks to pass through to us when focusing the window. If we're in
    // absolute mouse mode, this will avoid the user having to click twice to
    // trigger a click on the host if the Moonlight window is not focused. In
    // relative mode, the click event will trigger the mouse to be recaptured.
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    // Enabling extended input reports allows rumble to function on Bluetooth PS4/PS5
    // controllers, but breaks DirectInput applications. We will enable it because
    // it's likely that working rumble is what the user is expecting. If they don't
    // want this behavior, they can override it with the environment variable.
    SDL_SetHint("SDL_JOYSTICK_HIDAPI_PS4_RUMBLE", "1");
    SDL_SetHint("SDL_JOYSTICK_HIDAPI_PS5_RUMBLE", "1");

    // Populate special key combo configuration
    m_SpecialKeyCombos[KeyComboQuit].keyCombo = KeyComboQuit;
    m_SpecialKeyCombos[KeyComboQuit].keyCode = SDLK_q;
    m_SpecialKeyCombos[KeyComboQuit].scanCode = SDL_SCANCODE_Q;
    m_SpecialKeyCombos[KeyComboQuit].enabled = true;

    m_SpecialKeyCombos[KeyComboUngrabInput].keyCombo = KeyComboUngrabInput;
    m_SpecialKeyCombos[KeyComboUngrabInput].keyCode = SDLK_z;
    m_SpecialKeyCombos[KeyComboUngrabInput].scanCode = SDL_SCANCODE_Z;
    m_SpecialKeyCombos[KeyComboUngrabInput].enabled = QGuiApplication::platformName() != "eglfs";

    m_SpecialKeyCombos[KeyComboToggleFullScreen].keyCombo = KeyComboToggleFullScreen;
    m_SpecialKeyCombos[KeyComboToggleFullScreen].keyCode = SDLK_x;
    m_SpecialKeyCombos[KeyComboToggleFullScreen].scanCode = SDL_SCANCODE_X;
    m_SpecialKeyCombos[KeyComboToggleFullScreen].enabled = QGuiApplication::platformName() != "eglfs";

    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].keyCombo = KeyComboToggleStatsOverlay;
    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].keyCode = SDLK_s;
    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].scanCode = SDL_SCANCODE_S;
    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].enabled = true;

    m_SpecialKeyCombos[KeyComboToggleMouseMode].keyCombo = KeyComboToggleMouseMode;
    m_SpecialKeyCombos[KeyComboToggleMouseMode].keyCode = SDLK_m;
    m_SpecialKeyCombos[KeyComboToggleMouseMode].scanCode = SDL_SCANCODE_M;
    m_SpecialKeyCombos[KeyComboToggleMouseMode].enabled = QGuiApplication::platformName() != "eglfs";

    m_SpecialKeyCombos[KeyComboToggleCursorHide].keyCombo = KeyComboToggleCursorHide;
    m_SpecialKeyCombos[KeyComboToggleCursorHide].keyCode = SDLK_c;
    m_SpecialKeyCombos[KeyComboToggleCursorHide].scanCode = SDL_SCANCODE_C;
    m_SpecialKeyCombos[KeyComboToggleCursorHide].enabled = true;

    m_SpecialKeyCombos[KeyComboToggleMinimize].keyCombo = KeyComboToggleMinimize;
    m_SpecialKeyCombos[KeyComboToggleMinimize].keyCode = SDLK_d;
    m_SpecialKeyCombos[KeyComboToggleMinimize].scanCode = SDL_SCANCODE_D;
    m_SpecialKeyCombos[KeyComboToggleMinimize].enabled = QGuiApplication::platformName() != "eglfs";

    m_OldIgnoreDevices = SDL_GetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES);
    m_OldIgnoreDevicesExcept = SDL_GetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT);

    QString streamIgnoreDevices = qgetenv("STREAM_GAMECONTROLLER_IGNORE_DEVICES");
    QString streamIgnoreDevicesExcept = qgetenv("STREAM_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT");

    if (!streamIgnoreDevices.isEmpty() && !streamIgnoreDevices.endsWith(',')) {
        streamIgnoreDevices += ',';
    }
    streamIgnoreDevices += m_OldIgnoreDevices;

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
    SDL_zero(m_MousePositionReport);

    SDL_AtomicSet(&m_MouseDeltaX, 0);
    SDL_AtomicSet(&m_MouseDeltaY, 0);
    SDL_AtomicSet(&m_MousePositionUpdated, 0);

    Uint32 pollingInterval = QString(qgetenv("MOUSE_POLLING_INTERVAL")).toUInt();
    if (pollingInterval == 0) {
        pollingInterval = MOUSE_POLLING_INTERVAL;
    }
    else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using custom mouse polling interval: %u ms",
                    pollingInterval);
    }

    m_MouseMoveTimer = SDL_AddTimer(pollingInterval, SdlInputHandler::mouseMoveTimerCallback, this);

#ifdef Q_OS_WIN32
    if (m_CaptureSystemKeysEnabled) {
        // If system key capture is enabled, install the window hook required to intercept
        // these key presses and block them from the OS itself.
        g_KeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, SdlInputHandler::keyboardHookProc, GetModuleHandle(NULL), 0);
    }
#endif
}

SdlInputHandler::~SdlInputHandler()
{
#ifdef Q_OS_WIN32
    if (g_KeyboardHook != nullptr) {
        UnhookWindowsHookEx(g_KeyboardHook);
        g_KeyboardHook = nullptr;

        // If we're terminating because of the user pressing the quit combo,
        // we won't have a chance to inform SDL as the modifier keys raise.
        // This will leave stale modifier flags in the SDL_Keyboard global
        // inside SDL which will show up on our next key event if the user
        // streams again. Avoid this by explicitly zeroing mod state when
        // ending the stream.
        //
        // This is only needed for the case where we're hooking the keyboard
        // because we're generating synthetic SDL_KEYDOWN/SDL_KEYUP events
        // which don't properly maintain SDL's internal keyboard state,
        // so we're forced to invoke SDL_SetModState() ourselves to set mods.
        SDL_SetModState(KMOD_NONE);
    }
#endif

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

    SDL_RemoveTimer(m_MouseMoveTimer);
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
}

#ifdef Q_OS_WIN32
LRESULT CALLBACK SdlInputHandler::keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0 || nCode != HC_ACTION) {
        return CallNextHookEx(g_KeyboardHook, nCode, wParam, lParam);
    }

    bool keyCaptureActive;
    SDL_Event event = {};
    KBDLLHOOKSTRUCT* hookData = (KBDLLHOOKSTRUCT*)lParam;
    switch (hookData->vkCode) {
    case VK_LWIN:
        event.key.keysym.sym = SDLK_LGUI;
        event.key.keysym.scancode = SDL_SCANCODE_LGUI;
        break;
    case VK_RWIN:
        event.key.keysym.sym = SDLK_RGUI;
        event.key.keysym.scancode = SDL_SCANCODE_RGUI;
        break;
    case VK_LMENU:
        event.key.keysym.sym = SDLK_LALT;
        event.key.keysym.scancode = SDL_SCANCODE_LALT;
        break;
    case VK_RMENU:
        event.key.keysym.sym = SDLK_RALT;
        event.key.keysym.scancode = SDL_SCANCODE_RALT;
        break;
    case VK_LCONTROL:
        event.key.keysym.sym = SDLK_LCTRL;
        event.key.keysym.scancode = SDL_SCANCODE_LCTRL;
        break;
    case VK_RCONTROL:
        event.key.keysym.sym = SDLK_RCTRL;
        event.key.keysym.scancode = SDL_SCANCODE_RCTRL;
        break;
    default:
        // Bail quickly if it's not a key we care about
        goto NextHook;
    }

    // Make sure we're in a state where we actually want to steal this event (and it is safe to do so)
    Session* session = Session::get();
    if (session == nullptr || session->m_InputHandler == nullptr) {
        goto NextHook;
    }

    keyCaptureActive = session->m_InputHandler->isSystemKeyCaptureActive();

    // If this is a key we're going to intercept, create the synthetic SDL event.
    // This is necessary because we are also hiding this key event from ourselves too.
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        event.type = SDL_KEYDOWN;
        event.key.state = SDL_PRESSED;
    }
    else {
        event.type = SDL_KEYUP;
        event.key.state = SDL_RELEASED;
    }

    // Drop the event if it's a key down and capture is not active.
    // For key up, we'll need to do a little more work to determine if we need to send this
    // event to SDL in order to keep its internal keyboard modifier state consistent.
    if (event.type == SDL_KEYDOWN && !keyCaptureActive) {
        goto NextHook;
    }

    event.key.keysym.mod = SDL_GetModState();
    switch (hookData->vkCode) {
    case VK_LWIN:
        if (event.key.state == SDL_PRESSED) {
            event.key.keysym.mod |= KMOD_LGUI;
        }
        else {
            event.key.keysym.mod &= ~KMOD_LGUI;
        }
        break;
    case VK_RWIN:
        if (event.key.state == SDL_PRESSED) {
            event.key.keysym.mod |= KMOD_RGUI;
        }
        else {
            event.key.keysym.mod &= ~KMOD_RGUI;
        }
        break;
    case VK_LMENU:
        if (event.key.state == SDL_PRESSED) {
            event.key.keysym.mod |= KMOD_LALT;
        }
        else {
            event.key.keysym.mod &= ~KMOD_LALT;
        }
        break;
    case VK_RMENU:
        if (event.key.state == SDL_PRESSED) {
            event.key.keysym.mod |= KMOD_RALT;
        }
        else {
            event.key.keysym.mod &= ~KMOD_RALT;
        }
        break;
    case VK_LCONTROL:
        if (event.key.state == SDL_PRESSED) {
            event.key.keysym.mod |= KMOD_LCTRL;
        }
        else {
            event.key.keysym.mod &= ~KMOD_LCTRL;
        }
        break;
    case VK_RCONTROL:
        if (event.key.state == SDL_PRESSED) {
            event.key.keysym.mod |= KMOD_RCTRL;
        }
        else {
            event.key.keysym.mod &= ~KMOD_RCTRL;
        }
        break;
    }

    // If the modifier state is unchanged and we're not capturing system keys,
    // drop the event. If the modifier state is changed, we need to send the
    // event to update SDL's state even if system key capture is now inactive
    // (due to focus loss, mouse capture toggled off, etc.) otherwise SDL won't
    // know that the modifier key has been lifted.
    if (event.key.keysym.mod == SDL_GetModState() && !keyCaptureActive) {
        goto NextHook;
    }

    event.key.timestamp = SDL_GetTicks();
    event.key.windowID = SDL_GetWindowID(session->m_Window);

    // NOTE: event.key.repeat is not populated in this path!
    SDL_PushEvent(&event);

    // Synchronize SDL's modifier state with the current state.
    // SDL won't do this on its own because it will never see the
    // WM_KEYUP/WM_KEYDOWN events.
    SDL_SetModState((SDL_Keymod)event.key.keysym.mod);

    // Eat the event only if key capture is active.
    // If capture is not active and we're just resyncing SDL's modifier state,
    // we need to ensure the key event is still delivered normally to the
    // window in focus.
    if (keyCaptureActive) {
        return 1;
    }

NextHook:
    return CallNextHookEx(g_KeyboardHook, nCode, wParam, lParam);
}
#endif

void SdlInputHandler::raiseAllKeys()
{
    if (m_KeysDown.isEmpty()) {
        return;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Raising %d keys",
                (int)m_KeysDown.count());

    for (auto keyDown : m_KeysDown) {
        LiSendKeyboardEvent(keyDown, KEY_ACTION_UP, 0);
    }

    m_KeysDown.clear();
}

void SdlInputHandler::notifyMouseLeave()
{
#ifdef Q_OS_WIN32
    // SDL on Windows doesn't send the mouse button up until the mouse re-enters the window
    // after leaving it. This breaks some of the Aero snap gestures, so we'll fake it here.
    if (m_AbsoluteMouseMode && isCaptureActive()) {
        // NB: Not using SDL_GetGlobalMouseState() because we want our state not the system's
        Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);
        for (Uint32 button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_X2; button++) {
            if (mouseState & SDL_BUTTON(button)) {
                m_PendingMouseLeaveButtonUp = button;
                break;
            }
        }
    }
#endif
}

void SdlInputHandler::notifyFocusLost()
{
    // Release mouse cursor when another window is activated (e.g. by using ALT+TAB).
    // This lets user to interact with our window's title bar and with the buttons in it.
    // Doing this while the window is full-screen breaks the transition out of FS
    // (desktop and exclusive), so we must check for that before releasing mouse capture.
    if (!(SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN) && !m_AbsoluteMouseMode) {
        setCaptureActive(false);
    }

    // Raise all keys that are currently pressed. If we don't do this, certain keys
    // used in shortcuts that cause focus loss (such as Alt+Tab) may get stuck down.
    raiseAllKeys();
}

bool SdlInputHandler::isCaptureActive()
{
    if (SDL_GetRelativeMouseMode()) {
        return true;
    }

    // Some platforms don't support SDL_SetRelativeMouseMode
    return m_FakeCaptureActive;
}

bool SdlInputHandler::isSystemKeyCaptureActive()
{
    if (!m_CaptureSystemKeysEnabled) {
        return false;
    }

    if (m_Window == nullptr) {
        return false;
    }

    Uint32 windowFlags = SDL_GetWindowFlags(m_Window);
    return (windowFlags & SDL_WINDOW_INPUT_FOCUS) &&
            (windowFlags & SDL_WINDOW_INPUT_GRABBED) &&
            (windowFlags & SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void SdlInputHandler::setCaptureActive(bool active)
{
    if (active) {
        // If we're in full-screen exclusive mode, grab the cursor so it can't accidentally leave our window.
        // If we're in full-screen desktop mode but system key capture is enabled, also grab the cursor (will grab the keyboard too on X11).
        if (((SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0 && m_CaptureSystemKeysEnabled) ||
                (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
            SDL_SetWindowGrab(m_Window, SDL_TRUE);
        }

        if (!m_AbsoluteMouseMode) {
            // If our window is occluded when mouse is captured, the mouse may
            // get stuck on top of the occluding window and not be properly
            // captured. We can avoid this by raising our window before we
            // capture the mouse.
            SDL_RaiseWindow(m_Window);
        }

        // If we're in relative mode, try to activate SDL's relative mouse mode
        if (m_AbsoluteMouseMode || SDL_SetRelativeMouseMode(SDL_TRUE) < 0) {
            // Relative mouse mode didn't work or was disabled, so we'll just hide the cursor
            SDL_ShowCursor(m_MouseCursorCapturedVisibilityState);
            m_FakeCaptureActive = true;
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
                updateMousePositionReport(mouseX, mouseY);
            }
        }
    }
    else {
        if (m_FakeCaptureActive) {
            // Display the cursor again
            SDL_ShowCursor(SDL_ENABLE);
            m_FakeCaptureActive = false;
        }
        else {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }

        // Allow the cursor to leave the bounds of our window again.
        SDL_SetWindowGrab(m_Window, SDL_FALSE);
    }
}

void SdlInputHandler::handleTouchFingerEvent(SDL_TouchFingerEvent* event)
{
#if SDL_VERSION_ATLEAST(2, 0, 10)
    if (SDL_GetTouchDeviceType(event->touchId) != SDL_TOUCH_DEVICE_DIRECT) {
        // Ignore anything that isn't a touchscreen. We may get callbacks
        // for trackpads, but we want to handle those in the mouse path.
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
    else {
        handleRelativeFingerEvent(event);
    }
}
