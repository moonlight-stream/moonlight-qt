#include "input.h"
#include "pen_sdl3.h"

#include <Limelight.h>
#include "SDL_compat.h"
#include "streaming/streamutils.h"

#include <QtMath>

#ifndef SDL_PEN_MOUSEID
#define SDL_PEN_MOUSEID ((Uint32)-2)
#endif

static void moonlightPenHandler(const MoonlightPenRawEvent* event, void* userdata)
{
    auto* handler = static_cast<SdlInputHandler*>(userdata);
    if (handler) {
        handler->handleSdl3PenEvent(event);
    }
}

void SdlInputHandler::startSdl3PenCapture()
{
    if (!(LiGetHostFeatureFlags() & LI_FF_PEN_TOUCH_EVENTS)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Host does not advertise pen/touch support; SDL3 pen capture disabled");
        return;
    }

    if (Moonlight_Pen_Start(moonlightPenHandler, this)) {
        m_Sdl3PenCaptureActive = true;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL3 pen capture active (sdl2-compat filter wrap)");
    }
    else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to enable SDL3 pen capture (is libSDL3 loaded via sdl2-compat?)");
    }
}

void SdlInputHandler::stopSdl3PenCapture()
{
    if (m_Sdl3PenCaptureActive) {
        Moonlight_Pen_Stop();
        m_Sdl3PenCaptureActive = false;
    }
}

void SdlInputHandler::ensureSdl3PenFilter()
{
    if (m_Sdl3PenCaptureActive) {
        Moonlight_Pen_EnsureFilter();
    }
}

void SdlInputHandler::handleSdl3PenEvent(const MoonlightPenRawEvent* event)
{
    if (!m_Window || !event) {
        return;
    }

    if (!(LiGetHostFeatureFlags() & LI_FF_PEN_TOUCH_EVENTS)) {
        return;
    }

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(m_Window, &windowWidth, &windowHeight);
    if (windowWidth <= 0 || windowHeight <= 0) {
        return;
    }

    SDL_Rect src, dst;
    src.x = src.y = 0;
    src.w = m_StreamWidth;
    src.h = m_StreamHeight;
    dst.x = dst.y = 0;
    dst.w = windowWidth;
    dst.h = windowHeight;
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    float clampedX = qMin(qMax(event->x, (float)dst.x), (float)(dst.x + dst.w));
    float clampedY = qMin(qMax(event->y, (float)dst.y), (float)(dst.y + dst.h));
    float normX = (clampedX - dst.x) / (float)dst.w;
    float normY = (clampedY - dst.y) / (float)dst.h;

    uint16_t rotation = LI_ROT_UNKNOWN;
    if (!qIsNaN(event->rotationDegrees)) {
        // Sunshine expects 0..359
        float rot = event->rotationDegrees;
        while (rot < 0.0f) {
            rot += 360.0f;
        }
        while (rot >= 360.0f) {
            rot -= 360.0f;
        }
        rotation = (uint16_t)(rot + 0.5f);
    }

    LiSendPenEvent(event->eventType,
                   event->toolType,
                   event->penButtons,
                   normX,
                   normY,
                   event->pressure,
                   0.0f,
                   0.0f,
                   rotation,
                   event->tilt);

    if (!m_DisabledTouchFeedback) {
        disableTouchFeedback();
        m_DisabledTouchFeedback = true;
    }
}

bool SdlInputHandler::isSyntheticPenMouseId(Uint32 which)
{
    return which == SDL_PEN_MOUSEID;
}
