#include "input.h"

#include <Limelight.h>
#include "SDL_compat.h"
#include "streaming/streamutils.h"

#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

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

        HWND window = (HWND)SDLC_Win32_GetHwnd(m_Window);
        for (FEEDBACK_TYPE ft : feedbackTypes) {
            BOOL val = FALSE;
            fnSetWindowFeedbackSetting(window, ft, 0, sizeof(val), &val);
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
    case SDL_EVENT_FINGER_DOWN :
        eventType = LI_TOUCH_EVENT_DOWN;
        break;
    case SDL_EVENT_FINGER_MOTION :
        eventType = LI_TOUCH_EVENT_MOVE;
        break;
    case SDL_EVENT_FINGER_UP :
        eventType = LI_TOUCH_EVENT_UP;
        break;
    default:
        return;
    }

    uint32_t pointerId;

    // If the pointer ID is larger than we can fit, just CRC it and use that as the ID.
    if ((uint64_t) event->fingerID > UINT32_MAX) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QByteArrayView bav((char*)&event->fingerID, sizeof(event->fingerID));
        pointerId = qChecksum(bav);
#else
        pointerId = qChecksum((char*)&event->fingerID, sizeof(event->fingerID));
#endif
    }
    else {
        pointerId = (uint32_t) event->fingerID;
    }

    // Try to send it as a native pen/touch event, otherwise fall back to our touch emulation
    if (LiGetHostFeatureFlags() & LI_FF_PEN_TOUCH_EVENTS) {
#if SDL_VERSION_ATLEAST(2, 0, 22)
        bool isPen = false;

        int numTouchDevices = SDL_GetNumTouchDevices();
        for (int i = 0; i < numTouchDevices; i++) {
            if (event->touchID == SDL_GetTouchDevice(i)) {
                const char* touchName = SDL_GetTouchName(i);

                // SDL will report "pen" as the name of pen input devices on Windows.
                // https://github.com/libsdl-org/SDL/pull/5926
                isPen = touchName && SDL_strcmp(touchName, "pen") == 0;
                break;
            }
        }

        if (isPen) {
            LiSendPenEvent(eventType, LI_TOOL_TYPE_PEN, 0, vidrelx / dst.w, vidrely / dst.h, event->pressure,
                           0.0f, 0.0f, LI_ROT_UNKNOWN, LI_TILT_UNKNOWN);
        }
        else
#endif
        {
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

    // Ignore touch down events with more than one finger
    if (event->type == SDL_EVENT_FINGER_DOWN && SDL_GetNumTouchFingers(event->touchID) > 1) {
        return;
    }

    // Ignore touch move and touch up events from the non-primary finger
    if (event->type != SDL_EVENT_FINGER_DOWN && event->fingerID != m_LastTouchDownEvent.fingerID) {
        return;
    }

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
    if (event->type != SDL_EVENT_FINGER_DOWN ||
            event->timestamp - m_LastTouchUpEvent.timestamp > DOUBLE_TAP_DEAD_ZONE_DELAY ||
            qSqrt(qPow(event->x - m_LastTouchUpEvent.x, 2) + qPow(event->y - m_LastTouchUpEvent.y, 2)) > DOUBLE_TAP_DEAD_ZONE_DELTA) {
        // Scale window-relative events to be video-relative and clamp to video region
        short x = qMin(qMax((int)(event->x * windowWidth), dst.x), dst.x + dst.w);
        short y = qMin(qMax((int)(event->y * windowHeight), dst.y), dst.y + dst.h);

        // Update the cursor position relative to the video region
        LiSendMousePositionEvent(x - dst.x, y - dst.y, dst.w, dst.h);
    }

    if (event->type == SDL_EVENT_FINGER_DOWN) {
        m_LastTouchDownEvent = *event;

        // Start/restart the long press timer
        SDL_RemoveTimer(m_LongPressTimer);
        m_LongPressTimer = SDL_AddTimer(LONG_PRESS_ACTIVATION_DELAY,
                                        longPressTimerCallback,
                                        nullptr);

        // Left button down on finger down
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
    }
    else if (event->type == SDL_EVENT_FINGER_UP) {
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
