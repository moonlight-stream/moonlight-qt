#include "input.h"

#include <Limelight.h>
#if HAVE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#include <SDL_syswm.h>
#endif
#include "streaming/streamutils.h"

#include <QtMath>

// How long the fingers must be stationary to start a right click
#define LONG_PRESS_ACTIVATION_DELAY 650

// How far the finger can move before it cancels a right click
#define LONG_PRESS_ACTIVATION_DELTA 0.01f

// How long the double tap deadzone stays in effect between touch up and touch down
#define DOUBLE_TAP_DEAD_ZONE_DELAY 250

// How far the finger can move before it can override the double tap deadzone
#define DOUBLE_TAP_DEAD_ZONE_DELTA 0.025f

Uint32 SdlInputHandler::longPressTimerCallback(Uint32, void*)
{
    // Raise the left click and start a right click
    LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
    LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_RIGHT);

    return 0;
}

void SdlInputHandler::disableTouchFeedback()
{
#ifdef Q_OS_WIN32
#if SDL_VERSION_ATLEAST(3, 0, 0)
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "win32") == 0) {
        HWND hwnd = (HWND)SDL_GetProperty(SDL_GetWindowProperties(m_Window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#else
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(m_Window, &info);

    if (info.subsystem == SDL_SYSWM_WINDOWS) {
        HWND hwnd = info.info.win.window;
#endif
        auto fnSetWindowFeedbackSetting = (decltype(SetWindowFeedbackSetting)*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowFeedbackSetting");
        if (fnSetWindowFeedbackSetting) {
            constexpr FEEDBACK_TYPE feedbackTypes[] = {
                FEEDBACK_TOUCH_CONTACTVISUALIZATION,
                FEEDBACK_PEN_BARRELVISUALIZATION,
                FEEDBACK_PEN_TAP,
                FEEDBACK_PEN_DOUBLETAP,
                FEEDBACK_PEN_PRESSANDHOLD,
                FEEDBACK_PEN_RIGHTTAP,
                FEEDBACK_TOUCH_TAP,
                FEEDBACK_TOUCH_DOUBLETAP,
                FEEDBACK_TOUCH_PRESSANDHOLD,
                FEEDBACK_TOUCH_RIGHTTAP,
                FEEDBACK_GESTURE_PRESSANDTAP,
            };

            for (FEEDBACK_TYPE ft : feedbackTypes) {
                BOOL val = FALSE;
                fnSetWindowFeedbackSetting(hwnd, ft, 0, sizeof(val), &val);
            }
        }
    }
#endif
}

void SdlInputHandler::handleAbsoluteFingerEvent(SDL_TouchFingerEvent* event)
{
    SDL_Rect src, dst;
    int windowWidth, windowHeight;

    SDL_GetWindowSize(m_Window, &windowWidth, &windowHeight);

    src.x = src.y = 0;
    src.w = m_StreamWidth;
    src.h = m_StreamHeight;

    dst.x = dst.y = 0;
    dst.w = windowWidth;
    dst.h = windowHeight;

    // Scale window-relative events to be video-relative and clamp to video region
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);
    float vidrelx = qMin(qMax((int)(event->x * windowWidth), dst.x), dst.x + dst.w) - dst.x;
    float vidrely = qMin(qMax((int)(event->y * windowHeight), dst.y), dst.y + dst.h) - dst.y;

    uint8_t eventType;
    switch (event->type) {
#if SDL_VERSION_ATLEAST(3, 0, 0)
    case SDL_EVENT_FINGER_DOWN:
        eventType = LI_TOUCH_EVENT_DOWN;
        break;
    case SDL_EVENT_FINGER_MOTION:
        eventType = LI_TOUCH_EVENT_MOVE;
        break;
    case SDL_EVENT_FINGER_UP:
        eventType = LI_TOUCH_EVENT_UP;
        break;
#else
    case SDL_FINGERDOWN:
        eventType = LI_TOUCH_EVENT_DOWN;
        break;
    case SDL_FINGERMOTION:
        eventType = LI_TOUCH_EVENT_MOVE;
        break;
    case SDL_FINGERUP:
        eventType = LI_TOUCH_EVENT_UP;
        break;
#endif
    default:
        return;
    }

    uint32_t pointerId;

    // If the pointer ID is larger than we can fit, just CRC it and use that as the ID.
    if ((uint64_t)event->fingerId > UINT32_MAX) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QByteArrayView bav((char*)&event->fingerId, sizeof(event->fingerId));
        pointerId = qChecksum(bav);
#else
        pointerId = qChecksum((char*)&event->fingerId, sizeof(event->fingerId));
#endif
    }
    else {
        pointerId = (uint32_t)event->fingerId;
    }

    // Try to send it as a native pen/touch event, otherwise fall back to our touch emulation
    if (LiGetHostFeatureFlags() & LI_FF_PEN_TOUCH_EVENTS) {
        bool isPen = false;
#if SDL_VERSION_ATLEAST(3, 0, 0)
        int numTouchDevices = 0;
        SDL_TouchID *touchId = SDL_GetTouchDevices(&numTouchDevices);
        for (int i = 0; i < numTouchDevices; i++) {
            if (event->touchId == touchId[i]) {
                const char* touchName = SDL_GetTouchDeviceName(touchId[i]);

                // SDL will report "pen" as the name of pen input devices on Windows.
                // https://github.com/libsdl-org/SDL/pull/5926
                isPen = touchName && SDL_strcmp(touchName, "pen") == 0;
                break;
            }
        }
        if (touchId != nullptr) {
            SDL_free((void*)touchId);
        }
#else
        int numTouchDevices = SDL_GetNumTouchDevices();
        for (int i = 0; i < numTouchDevices; i++) {
            if (event->touchId == SDL_GetTouchDevice(i)) {
                const char* touchName = SDL_GetTouchName(i);

                // SDL will report "pen" as the name of pen input devices on Windows.
                // https://github.com/libsdl-org/SDL/pull/5926
                isPen = touchName && SDL_strcmp(touchName, "pen") == 0;
                break;
            }
        }
#endif

        if (isPen) {
            LiSendPenEvent(eventType, LI_TOOL_TYPE_PEN, 0, vidrelx / dst.w, vidrely / dst.h, event->pressure,
                           0.0f, 0.0f, LI_ROT_UNKNOWN, LI_TILT_UNKNOWN);
        }
        else {
            LiSendTouchEvent(eventType, pointerId, vidrelx / dst.w, vidrely / dst.h, event->pressure,
                             0.0f, 0.0f, LI_ROT_UNKNOWN);
        }

        if (!m_DisabledTouchFeedback) {
            // Disable touch feedback when passing touch natively
            disableTouchFeedback();
            m_DisabledTouchFeedback = true;
        }
    }
    else {
        emulateAbsoluteFingerEvent(event);
    }
}

void SdlInputHandler::emulateAbsoluteFingerEvent(SDL_TouchFingerEvent* event)
{
    // Observations on Windows 10: x and y appear to be relative to 0,0 of the window client area.
    // Although SDL documentation states they are 0.0 - 1.0 float values, they can actually be higher
    // or lower than those values as touch events continue for touches started within the client area that
    // leave the client area during a drag motion.
    // dx and dy are deltas from the last touch event, not the first touch down.

#if SDL_VERSION_ATLEAST(3, 0, 0)
    // Ignore touch down events with more than one finger
    if (event->type == SDL_EVENT_FINGER_DOWN && SDL_GetNumTouchFingers(event->touchId) > 1) {
        return;
    }

    // Ignore touch move and touch up events from the non-primary finger
    if (event->type != SDL_EVENT_FINGER_DOWN && event->fingerId != m_LastTouchDownEvent.fingerId) {
        return;
    }
#else
    // Ignore touch down events with more than one finger
    if (event->type == SDL_FINGERDOWN && SDL_GetNumTouchFingers(event->touchId) > 1) {
        return;
    }

    // Ignore touch move and touch up events from the non-primary finger
    if (event->type != SDL_FINGERDOWN && event->fingerId != m_LastTouchDownEvent.fingerId) {
        return;
    }
#endif

    SDL_Rect src, dst;
    int windowWidth, windowHeight;

    SDL_GetWindowSize(m_Window, &windowWidth, &windowHeight);

    src.x = src.y = 0;
    src.w = m_StreamWidth;
    src.h = m_StreamHeight;

    dst.x = dst.y = 0;
    dst.w = windowWidth;
    dst.h = windowHeight;

    // Use the stream and window sizes to determine the video region
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    if (qSqrt(qPow(event->x - m_LastTouchDownEvent.x, 2) + qPow(event->y - m_LastTouchDownEvent.y, 2)) > LONG_PRESS_ACTIVATION_DELTA) {
        // Moved too far since touch down. Cancel the long press timer.
        SDL_RemoveTimer(m_LongPressTimer);
        m_LongPressTimer = 0;
    }

    // Don't reposition for finger down events within the deadzone. This makes double-clicking easier.
#if SDL_VERSION_ATLEAST(3, 0, 0)
    if (event->type != SDL_EVENT_FINGER_DOWN ||
#else
    if (event->type != SDL_FINGERDOWN ||
#endif
            event->timestamp - m_LastTouchUpEvent.timestamp > DOUBLE_TAP_DEAD_ZONE_DELAY ||
            qSqrt(qPow(event->x - m_LastTouchUpEvent.x, 2) + qPow(event->y - m_LastTouchUpEvent.y, 2)) > DOUBLE_TAP_DEAD_ZONE_DELTA) {
        // Scale window-relative events to be video-relative and clamp to video region
        short x = qMin(qMax((int)(event->x * windowWidth), dst.x), dst.x + dst.w);
        short y = qMin(qMax((int)(event->y * windowHeight), dst.y), dst.y + dst.h);

        // Update the cursor position relative to the video region
        LiSendMousePositionEvent(x - dst.x, y - dst.y, dst.w, dst.h);
    }

#if SDL_VERSION_ATLEAST(3, 0, 0)
    if (event->type == SDL_EVENT_FINGER_DOWN) {
#else
    if (event->type == SDL_FINGERDOWN) {
#endif
        m_LastTouchDownEvent = *event;

        // Start/restart the long press timer
        SDL_RemoveTimer(m_LongPressTimer);
        m_LongPressTimer = SDL_AddTimer(LONG_PRESS_ACTIVATION_DELAY,
                                        longPressTimerCallback,
                                        nullptr);

        // Left button down on finger down
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
    }
#if SDL_VERSION_ATLEAST(3, 0, 0)
    else if (event->type == SDL_EVENT_FINGER_UP) {
#else
    else if (event->type == SDL_FINGERUP) {
#endif
        m_LastTouchUpEvent = *event;

        // Cancel the long press timer
        SDL_RemoveTimer(m_LongPressTimer);
        m_LongPressTimer = 0;

        // Left button up on finger up
        LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);

        // Raise right button too in case we triggered a long press gesture
        LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_RIGHT);
    }
}
