//
// Compatibility header for older versions of SDL.
// Include this instead of SDL.h directly.
//

#pragma once

#include <SDL.h>
#include <stdbool.h>

// This is a pure C header for compatibility with SDL.h
#ifdef __cplusplus
extern "C" {
#endif

// These SLDC_* functions and constants are special SDL-like things
// used to abstract certain SDL2 vs SDL3 differences.
void* SDLC_Win32_GetHwnd(SDL_Window* window);
void* SDLC_MacOS_GetWindow(SDL_Window* window);
void* SDLC_X11_GetDisplay(SDL_Window* window);
unsigned long SDLC_X11_GetWindow(SDL_Window* window);
void* SDLC_Wayland_GetDisplay(SDL_Window* window);
void* SDLC_Wayland_GetSurface(SDL_Window* window);
int SDLC_KMSDRM_GetFd(SDL_Window* window);
int SDLC_KMSDRM_GetDevIndex(SDL_Window* window);

typedef enum {
    SDLC_VIDEO_UNKNOWN,
    SDLC_VIDEO_WIN32,
    SDLC_VIDEO_MACOS,
    SDLC_VIDEO_X11,
    SDLC_VIDEO_WAYLAND,
    SDLC_VIDEO_KMSDRM,
} SDLC_VideoDriver;
SDLC_VideoDriver SDLC_GetVideoDriver();

bool SDLC_IsFullscreen(SDL_Window* window);
bool SDLC_IsFullscreenExclusive(SDL_Window* window);
bool SDLC_IsFullscreenDesktop(SDL_Window* window);
void SDLC_EnterFullscreen(SDL_Window* window, bool exclusive);
void SDLC_LeaveFullscreen(SDL_Window* window);

SDL_Window* SDLC_CreateWindowWithFallback(const char *title,
                                          int x, int y, int w, int h,
                                          Uint32 requiredFlags,
                                          Uint32 optionalFlags);

void SDLC_FlushWindowEvents();

#define SDLC_SUCCESS(x) ((x) == 0)
#define SDLC_FAILURE(x) ((x) != 0)

// SDL_FRect wasn't added until 2.0.10
#if !SDL_VERSION_ATLEAST(2, 0, 10)
typedef struct SDL_FRect
{
    float x;
    float y;
    float w;
    float h;
} SDL_FRect;
#endif

#ifndef SDL_THREAD_PRIORITY_TIME_CRITICAL
#define SDL_THREAD_PRIORITY_TIME_CRITICAL SDL_THREAD_PRIORITY_HIGH
#endif

#ifndef SDL_HINT_VIDEO_X11_FORCE_EGL
#define SDL_HINT_VIDEO_X11_FORCE_EGL "SDL_VIDEO_X11_FORCE_EGL"
#endif

#ifndef SDL_HINT_KMSDRM_REQUIRE_DRM_MASTER
#define SDL_HINT_KMSDRM_REQUIRE_DRM_MASTER "SDL_KMSDRM_REQUIRE_DRM_MASTER"
#endif

#ifndef SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED
#define SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED "SDL_ALLOW_ALT_TAB_WHILE_GRABBED"
#endif

#ifndef SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE
#define SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE "SDL_JOYSTICK_HIDAPI_PS4_RUMBLE"
#endif

#ifndef SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE
#define SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE "SDL_JOYSTICK_HIDAPI_PS5_RUMBLE"
#endif

#ifndef SDL_HINT_WINDOWS_USE_D3D9EX
#define SDL_HINT_WINDOWS_USE_D3D9EX "SDL_WINDOWS_USE_D3D9EX"
#endif

#ifndef SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS
#define SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS "SDL_GAMECONTROLLER_USE_BUTTON_LABELS"
#endif

#ifndef SDL_HINT_MOUSE_RELATIVE_SCALING
#define SDL_HINT_MOUSE_RELATIVE_SCALING "SDL_MOUSE_RELATIVE_SCALING"
#endif

#ifndef SDL_HINT_AUDIO_DEVICE_APP_NAME
#define SDL_HINT_AUDIO_DEVICE_APP_NAME "SDL_AUDIO_DEVICE_APP_NAME"
#endif

#ifndef SDL_HINT_APP_NAME
#define SDL_HINT_APP_NAME "SDL_APP_NAME"
#endif

#ifndef SDL_HINT_MOUSE_AUTO_CAPTURE
#define SDL_HINT_MOUSE_AUTO_CAPTURE "SDL_MOUSE_AUTO_CAPTURE"
#endif

#ifndef SDL_HINT_VIDEO_WAYLAND_EMULATE_MOUSE_WARP
#define SDL_HINT_VIDEO_WAYLAND_EMULATE_MOUSE_WARP "SDL_VIDEO_WAYLAND_EMULATE_MOUSE_WARP"
#endif

// SDL3 renamed hints
#define SDL_HINT_VIDEO_FORCE_EGL SDL_HINT_VIDEO_X11_FORCE_EGL

// Events
#define SDL_EVENT_QUIT SDL_QUIT
#define SDL_EVENT_CLIPBOARD_UPDATE SDL_CLIPBOARDUPDATE
#define SDL_EVENT_GAMEPAD_ADDED SDL_CONTROLLERDEVICEADDED
#define SDL_EVENT_GAMEPAD_REMAPPED SDL_CONTROLLERDEVICEREMAPPED
#define SDL_EVENT_GAMEPAD_REMOVED SDL_CONTROLLERDEVICEREMOVED
#define SDL_EVENT_GAMEPAD_SENSOR_UPDATE SDL_CONTROLLERSENSORUPDATE
#define SDL_EVENT_GAMEPAD_BUTTON_DOWN SDL_CONTROLLERBUTTONDOWN
#define SDL_EVENT_GAMEPAD_BUTTON_UP SDL_CONTROLLERBUTTONUP
#define SDL_EVENT_GAMEPAD_AXIS_MOTION SDL_CONTROLLERAXISMOTION
#define SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN SDL_CONTROLLERTOUCHPADDOWN
#define SDL_EVENT_GAMEPAD_TOUCHPAD_UP SDL_CONTROLLERTOUCHPADUP
#define SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION SDL_CONTROLLERTOUCHPADMOTION
#define SDL_EVENT_JOYSTICK_ADDED SDL_JOYDEVICEADDED
#define SDL_EVENT_JOYSTICK_REMOVED SDL_JOYDEVICEREMOVED
#define SDL_EVENT_JOYSTICK_BATTERY_UPDATED SDL_JOYBATTERYUPDATED
#define SDL_EVENT_FINGER_DOWN SDL_FINGERDOWN
#define SDL_EVENT_FINGER_MOTION SDL_FINGERMOTION
#define SDL_EVENT_FINGER_UP SDL_FINGERUP
#define SDL_EVENT_KEY_DOWN SDL_KEYDOWN
#define SDL_EVENT_KEY_UP SDL_KEYUP
#define SDL_EVENT_MOUSE_BUTTON_DOWN SDL_MOUSEBUTTONDOWN
#define SDL_EVENT_MOUSE_BUTTON_UP SDL_MOUSEBUTTONUP
#define SDL_EVENT_MOUSE_MOTION SDL_MOUSEMOTION
#define SDL_EVENT_MOUSE_WHEEL SDL_MOUSEWHEEL
#define SDL_EVENT_RENDER_DEVICE_RESET SDL_RENDER_DEVICE_RESET
#define SDL_EVENT_RENDER_TARGETS_RESET SDL_RENDER_TARGETS_RESET
#define SDL_EVENT_USER SDL_USEREVENT

#define SDL_EVENT_WINDOW_FOCUS_GAINED SDL_WINDOWEVENT_FOCUS_GAINED
#define SDL_EVENT_WINDOW_FOCUS_LOST SDL_WINDOWEVENT_FOCUS_LOST
#define SDL_EVENT_WINDOW_MOUSE_ENTER SDL_WINDOWEVENT_ENTER
#define SDL_EVENT_WINDOW_MOUSE_LEAVE SDL_WINDOWEVENT_LEAVE
#define SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED SDL_WINDOWEVENT_SIZE_CHANGED
#define SDL_EVENT_WINDOW_SHOWN SDL_WINDOWEVENT_SHOWN
#define SDL_EVENT_WINDOW_DISPLAY_CHANGED SDL_WINDOWEVENT_DISPLAY_CHANGED

#define SDL_BUTTON_MASK(x) SDL_BUTTON(x)

#define gbutton cbutton
#define gaxis caxis
#define gdevice cdevice
#define gsensor csensor
#define gtouchpad ctouchpad

#define fingerID fingerId
#define touchID touchId

#define KEY_DOWN(x) ((x)->state == SDL_PRESSED)
#define KEY_KEY(x) ((x)->keysym.sym)
#define KEY_MOD(x) ((x)->keysym.mod)
#define KEY_SCANCODE(x) ((x)->keysym.scancode)

// Gamepad
#define SDL_INIT_GAMEPAD SDL_INIT_GAMECONTROLLER

#define SDL_GAMEPAD_BUTTON_SOUTH SDL_CONTROLLER_BUTTON_A
#define SDL_GAMEPAD_BUTTON_EAST SDL_CONTROLLER_BUTTON_B
#define SDL_GAMEPAD_BUTTON_WEST SDL_CONTROLLER_BUTTON_X
#define SDL_GAMEPAD_BUTTON_NORTH SDL_CONTROLLER_BUTTON_Y
#define SDL_GAMEPAD_BUTTON_LEFT_SHOULDER SDL_CONTROLLER_BUTTON_LEFTSHOULDER
#define SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
#define SDL_GAMEPAD_BUTTON_DPAD_UP SDL_CONTROLLER_BUTTON_DPAD_UP
#define SDL_GAMEPAD_BUTTON_DPAD_DOWN SDL_CONTROLLER_BUTTON_DPAD_DOWN
#define SDL_GAMEPAD_BUTTON_DPAD_LEFT SDL_CONTROLLER_BUTTON_DPAD_LEFT
#define SDL_GAMEPAD_BUTTON_DPAD_RIGHT SDL_CONTROLLER_BUTTON_DPAD_RIGHT
#define SDL_GAMEPAD_BUTTON_START SDL_CONTROLLER_BUTTON_START
#define SDL_GAMEPAD_BUTTON_TOUCHPAD SDL_CONTROLLER_BUTTON_TOUCHPAD

#define SDL_GAMEPAD_BINDTYPE_AXIS SDL_CONTROLLER_BINDTYPE_AXIS
#define SDL_GAMEPAD_BINDTYPE_BUTTON SDL_CONTROLLER_BINDTYPE_BUTTON
#define SDL_GAMEPAD_BINDTYPE_HAT SDL_CONTROLLER_BINDTYPE_HAT
#define SDL_GAMEPAD_BINDTYPE_NONE SDL_CONTROLLER_BINDTYPE_NONE

#define SDL_GAMEPAD_AXIS_LEFTX SDL_CONTROLLER_AXIS_LEFTX
#define SDL_GAMEPAD_AXIS_LEFTY SDL_CONTROLLER_AXIS_LEFTY
#define SDL_GAMEPAD_AXIS_RIGHTX SDL_CONTROLLER_AXIS_RIGHTX
#define SDL_GAMEPAD_AXIS_RIGHTY SDL_CONTROLLER_AXIS_RIGHTY
#define SDL_GAMEPAD_AXIS_LEFT_TRIGGER SDL_CONTROLLER_AXIS_TRIGGERLEFT
#define SDL_GAMEPAD_AXIS_RIGHT_TRIGGER SDL_CONTROLLER_AXIS_TRIGGERRIGHT

// SDL_OpenGamepad() not defined due to differing semantics
SDL_JoystickID* SDL_GetGamepads(int *count);
#define SDL_OpenGamepad(x) SDL_GameControllerOpen(x)
#define SDL_CloseGamepad(x) SDL_GameControllerClose(x)
#define SDL_GetGamepadMapping(x) SDL_GameControllerMapping(x)
#define SDL_GetGamepadName(x) SDL_GameControllerName(x)
#define SDL_GetGamepadVendor(x) SDL_GameControllerGetVendor(x)
#define SDL_GetGamepadProduct(x) SDL_GameControllerGetProduct(x)
#define SDL_GetGamepadJoystick(x) SDL_GameControllerGetJoystick(x)
#define SDL_GamepadHasButton(x, y) SDL_GameControllerHasButton(x, y)
#define SDL_GamepadHasSensor(x, y) SDL_GameControllerHasSensor(x, y)
#define SDL_SetGamepadPlayerIndex(x, y) SDL_GameControllerSetPlayerIndex(x, y)
#define SDL_GamepadSensorEnabled(x, y) SDL_GameControllerIsSensorEnabled(x, y)
#define SDL_SetGamepadSensorEnabled(x, y, z) SDL_GameControllerSetSensorEnabled(x, y, z)
#define SDL_GetNumGamepadTouchpads(x) SDL_GameControllerGetNumTouchpads(x)
#define SDL_SetGamepadLED(x, r, g, b) SDL_GameControllerSetLED(x, r, g, b)
#define SDL_RumbleGamepad(x, y, z, w) SDL_GameControllerRumble(x, y, z, w)
#define SDL_RumbleGamepadTriggers(x, y, z, w) SDL_GameControllerRumbleTriggers(x, y, z, w)
#define SDL_GetGamepadAxis(x, y) SDL_GameControllerGetAxis(x, y)
#define SDL_GetGamepadType(x) SDL_GameControllerGetType(x)
#define SDL_IsGamepad(x) SDL_IsGameController(x)

#define SDL_GAMEPAD_TYPE_STANDARD SDL_CONTROLLER_TYPE_UNKNOWN
#define SDL_GAMEPAD_TYPE_VIRTUAL SDL_CONTROLLER_TYPE_VIRTUAL
#define SDL_GAMEPAD_TYPE_XBOX360 SDL_CONTROLLER_TYPE_XBOX360
#define SDL_GAMEPAD_TYPE_XBOXONE SDL_CONTROLLER_TYPE_XBOXONE
#define SDL_GAMEPAD_TYPE_PS3 SDL_CONTROLLER_TYPE_PS3
#define SDL_GAMEPAD_TYPE_PS4 SDL_CONTROLLER_TYPE_PS4
#define SDL_GAMEPAD_TYPE_PS5 SDL_CONTROLLER_TYPE_PS5
#define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT
#define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR
#define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT
#define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO

// SDL_OpenGamepad() not defined due to differing semantics
SDL_JoystickID* SDL_GetJoysticks(int *count);
#define SDL_OpenJoystick(x) SDL_JoystickOpen(x)
#define SDL_CloseJoystick(x) SDL_JoystickClose(x)
#define SDL_GetJoystickID(x) SDL_JoystickInstanceID(x)
#define SDL_GetJoystickGUID(x) SDL_JoystickGetGUID(x)
#define SDL_GetJoystickPowerLevel(x) SDL_JoystickCurrentPowerLevel(x)
#define SDL_GetNumJoystickAxes(x) SDL_JoystickNumAxes(x)
#define SDL_GetNumJoystickBalls(x) SDL_JoystickNumBalls(x)
#define SDL_GetNumJoystickButtons(x) SDL_JoystickNumButtons(x)
#define SDL_GetNumJoystickHats(x) SDL_JoystickNumHats(x)
#define SDL_GetJoystickGUIDForID(x) SDL_JoystickGetDeviceGUID(x)

typedef SDL_ControllerAxisEvent SDL_GamepadAxisEvent;
typedef SDL_ControllerButtonEvent SDL_GamepadButtonEvent;
typedef SDL_ControllerSensorEvent SDL_GamepadSensorEvent;
typedef SDL_ControllerTouchpadEvent SDL_GamepadTouchpadEvent;
typedef SDL_ControllerDeviceEvent SDL_GamepadDeviceEvent;
typedef SDL_GameController SDL_Gamepad;
typedef SDL_GameControllerButton SDL_GamepadButton;

// Audio
#define SDL_AUDIO_F32 AUDIO_F32SYS

#define SDL_ResumeAudioDevice(x) SDL_PauseAudioDevice(x, 0)

// Atomics
#define SDL_GetAtomicInt(x) SDL_AtomicGet(x)
#define SDL_SetAtomicInt(x, y) SDL_AtomicSet(x, y)
#define SDL_GetAtomicPointer(x) SDL_AtomicGetPtr(x)
#define SDL_SetAtomicPointer(x, y) SDL_AtomicSetPtr(x, y)

#define SDL_LockSpinlock(x) SDL_AtomicLock(x)
#define SDL_TryLockSpinlock(x) SDL_AtomicTryLock(x)
#define SDL_UnlockSpinlock(x) SDL_AtomicUnlock(x)

typedef SDL_atomic_t SDL_AtomicInt;

// Video
#define SDL_KMOD_CTRL KMOD_CTRL
#define SDL_KMOD_ALT KMOD_ALT
#define SDL_KMOD_SHIFT KMOD_SHIFT
#define SDL_KMOD_GUI KMOD_GUI

#define SDL_WINDOW_HIGH_PIXEL_DENSITY SDL_WINDOW_ALLOW_HIGHDPI

#define SDL_SetWindowFullscreenMode(x, y) SDL_SetWindowDisplayMode(x, y)
#define SDL_GetDisplayForWindow(x) SDL_GetWindowDisplayIndex(x)
#define SDL_GetRenderViewport(x, y) SDL_RenderGetViewport(x, y)
#define SDL_DestroySurface(x) SDL_FreeSurface(x)
#define SDL_RenderTexture(x, y, z, w) SDL_RenderCopy(x, y, z, w)
#define SDL_GetPrimaryDisplay() (0)

#define SDL_CreateSurfaceFrom(w, h, fmt, pixels, pitch) SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, SDL_BITSPERPIXEL(fmt), pitch, fmt)

#define SDLC_DEFAULT_RENDER_DRIVER -1

typedef int SDL_DisplayID;
SDL_DisplayID * SDL_GetDisplays(int *count);
const SDL_DisplayMode * SDL_GetWindowFullscreenMode(SDL_Window *window);

// Misc
#define SDL_GetNumLogicalCPUCores() SDL_GetCPUCount()
#define SDL_IOFromConstMem(x, y) SDL_RWFromConstMem(x, y)
#define SDL_SetCurrentThreadPriority(x) SDL_SetThreadPriority(x)
#define SDL_GUIDToString(x, y, z) SDL_JoystickGetGUIDString(x, y, z)

#ifdef __cplusplus
}
#endif
