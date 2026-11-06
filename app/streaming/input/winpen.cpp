// Native pen passthrough for Windows (Wacom / Windows Ink tablets and pen displays).
//
// SDL2 only exposes pens as touch events without tilt, eraser, barrel button or
// hover, and it drops indirect digitizers (screenless tablets) entirely. Here we
// subclass the stream window and read WM_POINTER pen messages directly, then
// forward them with LiSendPenEvent() so Sunshine can recreate a virtual pen
// tablet on the host with pressure, tilt, eraser, barrel button and hover.

#include "input.h"

#ifdef Q_OS_WIN32

#include <Limelight.h>
#include "SDL_compat.h"
#include <SDL_syswm.h>
#include "streaming/streamutils.h"

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cmath>
#include <vector>

static constexpr UINT_PTR k_PenSubclassId = 0x4D4C5045; // 'MLPE'
static constexpr double k_Pi = 3.14159265358979323846;

static LRESULT CALLBACK penSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                        UINT_PTR subclassId, DWORD_PTR refData)
{
    auto handler = reinterpret_cast<SdlInputHandler*>(refData);

    switch (msg) {
    case WM_POINTERENTER:
    case WM_POINTERLEAVE:
    case WM_POINTERDOWN:
    case WM_POINTERUP:
    case WM_POINTERUPDATE:
    case WM_POINTERCAPTURECHANGED:
        if (handler->handleWin32PenMessage(msg, wParam, lParam)) {
            // Consumed: skipping DefWindowProc also stops Windows from
            // promoting the pen to mouse/touch input, so the host doesn't
            // receive a duplicate mouse click for every stroke.
            return 0;
        }
        break;

    case WM_NCDESTROY:
        handler->handleWin32PenMessage(msg, wParam, lParam);
        RemoveWindowSubclass(hwnd, penSubclassProc, subclassId);
        break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void SdlInputHandler::installWin32PenHook()
{
    if (m_Window == nullptr) {
        return;
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(m_Window, &info) || info.subsystem != SDL_SYSWM_WINDOWS) {
        return;
    }

    HWND hwnd = info.info.win.window;
    if (SetWindowSubclass(hwnd, penSubclassProc, k_PenSubclassId, reinterpret_cast<DWORD_PTR>(this))) {
        m_PenHwnd = hwnd;
        m_PenInContact = m_PenHovering = false;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Native pen passthrough enabled");
    }
    else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SetWindowSubclass() failed for pen passthrough: %lu",
                    GetLastError());
    }
}

void SdlInputHandler::removeWin32PenHook()
{
    if (m_PenHwnd != nullptr) {
        RemoveWindowSubclass(static_cast<HWND>(m_PenHwnd), penSubclassProc, k_PenSubclassId);
        m_PenHwnd = nullptr;
    }
    m_PenInContact = m_PenHovering = false;
}

bool SdlInputHandler::handleWin32PenMessage(unsigned int msg, uintptr_t wParam, intptr_t)
{
    if (msg == WM_NCDESTROY) {
        m_PenHwnd = nullptr;
        m_PenInContact = m_PenHovering = false;
        return false;
    }

    HWND hwnd = static_cast<HWND>(m_PenHwnd);
    if (hwnd == nullptr) {
        return false;
    }

    UINT32 pointerId = GET_POINTERID_WPARAM(wParam);
    POINTER_INPUT_TYPE pointerType;
    if (!GetPointerType(pointerId, &pointerType) || pointerType != PT_PEN) {
        return false;
    }

    bool penActive = m_PenInContact || m_PenHovering;

    if (msg == WM_POINTERCAPTURECHANGED) {
        if (penActive) {
            LiSendPenEvent(LI_TOUCH_EVENT_CANCEL, LI_TOOL_TYPE_PEN, 0, 0.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, LI_ROT_UNKNOWN, LI_TILT_UNKNOWN);
        }
        m_PenInContact = m_PenHovering = false;
        return false;
    }

    if (!penActive) {
        // Only take over the pen while streaming to a host that understands pen
        // events and while input is captured. Otherwise let Windows turn it into
        // a mouse click as usual (that click is what activates input capture).
        if (!(LiGetHostFeatureFlags() & LI_FF_PEN_TOUCH_EVENTS) || !isCaptureActive()) {
            return false;
        }

        // Let a pen-up we never saw the matching pen-down for fall through to
        // SDL, so it doesn't end up with a stuck mouse button.
        if (msg == WM_POINTERUP) {
            return false;
        }
    }

    if (msg == WM_POINTERLEAVE) {
        if (m_PenHovering && !m_PenInContact) {
            LiSendPenEvent(LI_TOUCH_EVENT_HOVER_LEAVE, LI_TOOL_TYPE_PEN, 0, 0.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, LI_ROT_UNKNOWN, LI_TILT_UNKNOWN);
            m_PenHovering = false;
        }
        return true;
    }

    POINTER_PEN_INFO latest;
    if (!GetPointerPenInfo(pointerId, &latest)) {
        return penActive;
    }

    // Replay coalesced samples so fast strokes keep their full report rate.
    // History is returned newest-first.
    std::vector<POINTER_PEN_INFO> frames;
    UINT32 historyCount = latest.pointerInfo.historyCount;
    if (msg == WM_POINTERUPDATE && historyCount > 1) {
        frames.resize(historyCount);
        if (GetPointerPenInfoHistory(pointerId, &historyCount, frames.data())) {
            frames.resize(historyCount);
            std::reverse(frames.begin(), frames.end());
        }
        else {
            frames.assign(1, latest);
        }
    }
    else {
        frames.assign(1, latest);
    }

    RECT clientRect;
    if (!GetClientRect(hwnd, &clientRect)) {
        return true;
    }

    SDL_Rect src, dst;
    src.x = src.y = 0;
    src.w = m_StreamWidth;
    src.h = m_StreamHeight;
    dst.x = dst.y = 0;
    dst.w = clientRect.right - clientRect.left;
    dst.h = clientRect.bottom - clientRect.top;
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);
    if (dst.w <= 0 || dst.h <= 0) {
        return true;
    }

    for (const POINTER_PEN_INFO& frame : frames) {
        const POINTER_FLAGS flags = frame.pointerInfo.pointerFlags;
        const bool inContact = (flags & POINTER_FLAG_INCONTACT) != 0;
        const bool inRange = (flags & POINTER_FLAG_INRANGE) != 0;

        // Window-relative -> video-relative, clamped to the video region
        POINT pt = frame.pointerInfo.ptPixelLocation;
        ScreenToClient(hwnd, &pt);
        float x = std::clamp((pt.x - dst.x) / (float)dst.w, 0.0f, 1.0f);
        float y = std::clamp((pt.y - dst.y) / (float)dst.h, 0.0f, 1.0f);

        uint8_t toolType = (frame.penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER)) ?
                               LI_TOOL_TYPE_ERASER : LI_TOOL_TYPE_PEN;
        uint8_t buttons = (frame.penFlags & PEN_FLAG_BARREL) ? LI_PEN_BUTTON_PRIMARY : 0;

        // Windows reports pressure as 0..1024
        float pressure = 0.0f;
        if (frame.penMask & PEN_MASK_PRESSURE) {
            pressure = std::clamp(frame.pressure / 1024.0f, 0.0f, 1.0f);
        }

        // Convert Windows tilt X/Y into Moonlight's polar form: tilt is the angle
        // from vertical (0..90) and rotation is the azimuth (0..360). This is the
        // exact inverse of Sunshine's conversion back to tilt X/Y.
        uint16_t rotation = LI_ROT_UNKNOWN;
        uint8_t tilt = LI_TILT_UNKNOWN;
        if (frame.penMask & (PEN_MASK_TILT_X | PEN_MASK_TILT_Y)) {
            double tiltX = std::clamp<int>(frame.tiltX, -89, 89) * k_Pi / 180.0;
            double tiltY = std::clamp<int>(frame.tiltY, -89, 89) * k_Pi / 180.0;
            double tanX = std::tan(tiltX);
            double tanY = std::tan(tiltY);

            double tiltDeg = std::atan(std::sqrt(tanX * tanX + tanY * tanY)) * 180.0 / k_Pi;
            double rotDeg = 0.0;
            if (tanX != 0.0 || tanY != 0.0) {
                rotDeg = std::atan2(-tanX, tanY) * 180.0 / k_Pi;
                if (rotDeg < 0.0) {
                    rotDeg += 360.0;
                }
            }

            tilt = (uint8_t)std::clamp<long>(std::lround(tiltDeg), 0, 90);
            rotation = (uint16_t)(std::lround(rotDeg) % 360);
        }

        uint8_t eventType;
        if (inContact) {
            eventType = m_PenInContact ? LI_TOUCH_EVENT_MOVE : LI_TOUCH_EVENT_DOWN;
            m_PenInContact = true;
            m_PenHovering = false;
        }
        else if (m_PenInContact) {
            eventType = LI_TOUCH_EVENT_UP;
            pressure = 0.0f;
            m_PenInContact = false;
            m_PenHovering = inRange;
        }
        else if (inRange) {
            // Hover distance isn't reported by Windows; 0.0 means unknown
            eventType = LI_TOUCH_EVENT_HOVER;
            pressure = 0.0f;
            m_PenHovering = true;
        }
        else if (m_PenHovering) {
            eventType = LI_TOUCH_EVENT_HOVER_LEAVE;
            pressure = 0.0f;
            m_PenHovering = false;
        }
        else {
            continue;
        }

        LiSendPenEvent(eventType, toolType, buttons, x, y, pressure,
                       0.0f, 0.0f, rotation, tilt);
    }

    if (!m_DisabledTouchFeedback) {
        // Suppress Windows Ink tap/press-and-hold visuals over the stream
        disableTouchFeedback();
        m_DisabledTouchFeedback = true;
    }

    return true;
}

#endif // Q_OS_WIN32
