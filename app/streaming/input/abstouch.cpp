#include "input.h"

#include <Limelight.h>
#include <SDL.h>
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

    // Observations on Windows 10: x and y appear to be relative to 0,0 of the window client area.
    // Although SDL documentation states they are 0.0 - 1.0 float values, they can actually be higher
    // or lower than those values as touch events continue for touches started within the client area that
    // leave the client area during a drag motion.
    // dx and dy are deltas from the last touch event, not the first touch down.

    // Ignore touch down events with more than one finger
    if (event->type == SDL_FINGERDOWN && SDL_GetNumTouchFingers(event->touchId) > 1) {
        return;
    }

    // Ignore touch move and touch up events from the non-primary finger
    if (event->type != SDL_FINGERDOWN && event->fingerId != m_LastTouchDownEvent.fingerId) {
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
    if (event->type != SDL_FINGERDOWN ||
            event->timestamp - m_LastTouchUpEvent.timestamp > DOUBLE_TAP_DEAD_ZONE_DELAY ||
            qSqrt(qPow(event->x - m_LastTouchUpEvent.x, 2) + qPow(event->y - m_LastTouchUpEvent.y, 2)) > DOUBLE_TAP_DEAD_ZONE_DELTA) {
        // Scale window-relative events to be video-relative and clamp to video region
        short x = qMin(qMax((int)(event->x * windowWidth), dst.x), dst.x + dst.w);
        short y = qMin(qMax((int)(event->y * windowHeight), dst.y), dst.y + dst.h);

        // Update the cursor position relative to the video region
        LiSendMousePositionEvent(x - dst.x, y - dst.y, dst.w, dst.h);
    }

    if (event->type == SDL_FINGERDOWN) {
        m_LastTouchDownEvent = *event;

        // Start/restart the long press timer
        SDL_RemoveTimer(m_LongPressTimer);
        m_LongPressTimer = SDL_AddTimer(LONG_PRESS_ACTIVATION_DELAY,
                                        longPressTimerCallback,
                                        nullptr);

        // Left button down on finger down
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
    }
    else if (event->type == SDL_FINGERUP) {
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
