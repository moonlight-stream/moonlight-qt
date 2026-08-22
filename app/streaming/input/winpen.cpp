#include "input.h"

#ifdef HAVE_WINDOWS_PEN_INPUT

#include <Limelight.h>
#include <SDL_syswm.h>
#include <Windows.h>
#include <CommCtrl.h>

#include "penhistory.h"
#include "streaming/streamutils.h"

#include <QtMath>

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <new>

namespace {

constexpr UINT_PTR WINDOWS_PEN_SUBCLASS_ID = 0x4D4C504E; // "MLPN"
constexpr Uint32 PEN_HISTORY_REPORT_INTERVAL_MS = 5000;

Uint32 windowsPenStateKey(const POINTER_PEN_INFO& sample)
{
    Uint32 stateKey = 0;
    if (sample.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) {
        stateKey |= 0x01;
    }
    if (sample.pointerInfo.pointerFlags & POINTER_FLAG_CANCELED) {
        stateKey |= 0x02;
    }
    if (sample.penFlags & PEN_FLAG_BARREL) {
        stateKey |= 0x04;
    }
    if (sample.penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER)) {
        stateKey |= 0x08;
    }
    return stateKey;
}

using SetWindowSubclassFn = BOOL (WINAPI *)(HWND, SUBCLASSPROC, UINT_PTR, DWORD_PTR);
using RemoveWindowSubclassFn = BOOL (WINAPI *)(HWND, SUBCLASSPROC, UINT_PTR);
using DefSubclassProcFn = LRESULT (WINAPI *)(HWND, UINT, WPARAM, LPARAM);

HMODULE s_ComCtl32 = nullptr;
SetWindowSubclassFn s_SetWindowSubclass = nullptr;
RemoveWindowSubclassFn s_RemoveWindowSubclass = nullptr;
DefSubclassProcFn s_DefSubclassProc = nullptr;

struct WindowsPenSubclassContext
{
    explicit WindowsPenSubclassContext(SdlInputHandler* handler) : inputHandler(handler) {}

    std::mutex mutex;
    SdlInputHandler* inputHandler;
    std::atomic_bool orphaned{false};
    std::atomic_bool windowDestroyed{false};
};

bool loadWindowSubclassApis()
{
    if (s_SetWindowSubclass && s_RemoveWindowSubclass && s_DefSubclassProc) {
        return true;
    }

    if (!s_ComCtl32) {
        s_ComCtl32 = LoadLibraryW(L"comctl32.dll");
    }
    if (!s_ComCtl32) {
        return false;
    }

    s_SetWindowSubclass = reinterpret_cast<SetWindowSubclassFn>(
            GetProcAddress(s_ComCtl32, "SetWindowSubclass"));
    s_RemoveWindowSubclass = reinterpret_cast<RemoveWindowSubclassFn>(
            GetProcAddress(s_ComCtl32, "RemoveWindowSubclass"));
    s_DefSubclassProc = reinterpret_cast<DefSubclassProcFn>(
            GetProcAddress(s_ComCtl32, "DefSubclassProc"));

    return s_SetWindowSubclass && s_RemoveWindowSubclass && s_DefSubclassProc;
}

LRESULT CALLBACK windowsPenSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                        UINT_PTR, DWORD_PTR refData)
{
    auto* context = reinterpret_cast<WindowsPenSubclassContext*>(refData);
    bool handled = false;
    if (context) {
        std::lock_guard<std::mutex> lock(context->mutex);
        if (context->inputHandler) {
            handled = context->inputHandler->handleWindowsPenPointerMessage(
                    message, static_cast<Uint64>(wParam));
        }
    }

    if (handled) {
        // Consuming the native pen message also prevents Windows and SDL from
        // promoting the same stroke into synthetic touch and mouse events.
        return 0;
    }

    if (message == WM_NCDESTROY && context) {
        context->windowDestroyed.store(true);
        if (s_RemoveWindowSubclass) {
            s_RemoveWindowSubclass(hwnd, windowsPenSubclassProc,
                                   WINDOWS_PEN_SUBCLASS_ID);
        }
    }

    const LRESULT result = s_DefSubclassProc(hwnd, message, wParam, lParam);
    if (message == WM_NCDESTROY && context && context->orphaned.load()) {
        delete context;
    }
    return result;
}

}

bool SdlInputHandler::initializeWindowsPenInput()
{
    if (!m_Window) {
        return false;
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(m_Window, &info) || info.subsystem != SDL_SYSWM_WINDOWS) {
        return false;
    }

    HWND hwnd = info.info.win.window;
    auto* installedContext =
            static_cast<WindowsPenSubclassContext*>(m_WindowsPenSubclassContext);
    if (m_WindowsPenSubclassInstalled && m_WindowsPenWindow == hwnd &&
            installedContext && !installedContext->windowDestroyed.load()) {
        return true;
    }

    shutdownWindowsPenInput(true);

    if (!loadWindowSubclassApis()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                    "Windows pen input unavailable: window subclass APIs not found");
        return false;
    }

    auto* context = new (std::nothrow) WindowsPenSubclassContext(this);
    if (!context) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                    "Windows pen input unavailable: failed to allocate subclass context");
        return false;
    }

    if (!s_SetWindowSubclass(hwnd, windowsPenSubclassProc, WINDOWS_PEN_SUBCLASS_ID,
                             reinterpret_cast<DWORD_PTR>(context))) {
        delete context;
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                    "Windows pen input unavailable: SetWindowSubclass() failed");
        return false;
    }

    m_WindowsPenWindow = hwnd;
    m_WindowsPenSubclassContext = context;
    m_WindowsPenSubclassInstalled = true;
    return true;
}

bool SdlInputHandler::trySendWindowsPenCancel()
{
    if (!(LiGetHostFeatureFlags() & LI_FF_PEN_TOUCH_EVENTS)) {
        m_WindowsPenCancelPending = false;
        return true;
    }

    m_WindowsPenCancelPending = LiSendPenEvent(
            LI_TOUCH_EVENT_CANCEL_ALL, LI_TOOL_TYPE_UNKNOWN, 0,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            LI_ROT_UNKNOWN, LI_TILT_UNKNOWN) != 0;
    if (!m_WindowsPenCancelPending) {
        m_WindowsPenLastSentStateValid = false;
    }
    return !m_WindowsPenCancelPending;
}

void SdlInputHandler::cancelWindowsPenInput(bool suppressPointer)
{
    const Uint32 pointerId = m_WindowsPenPointerTracked ?
                                 m_WindowsPenPointerId :
                                 m_WindowsPenFallbackPointerId;
    if (m_WindowsPenPointerTracked ||
            m_WindowsPenFallbackPointerId != UINT32_MAX ||
            m_WindowsPenCancelPending) {
        trySendWindowsPenCancel();
    }

    m_WindowsPenPointerId = 0;
    m_WindowsPenFallbackPointerId = UINT32_MAX;
    m_WindowsPenPointerTracked = false;
    m_WindowsPenLastSentStateValid = false;

    if (suppressPointer) {
        if (pointerId != UINT32_MAX) {
            m_WindowsPenSuppressedPointerId = pointerId;
        }
    }
    else {
        m_WindowsPenSuppressedPointerId = UINT32_MAX;
    }
}

void SdlInputHandler::routeWindowsPenPointerToSdl(Uint32 pointerId)
{
    // Both paths ultimately use LiSendPenEvent(), so keep the existing remote
    // pen state and only switch the local event source.
    m_WindowsPenPointerId = 0;
    m_WindowsPenPointerTracked = false;
    m_WindowsPenFallbackPointerId = pointerId;
    m_WindowsPenLastSentStateValid = false;
}

void SdlInputHandler::recordWindowsPenHistoryStats(Uint32 availableSamples,
                                                   Uint32 replayedSamples)
{
    const Uint32 droppedSamples =
            availableSamples > replayedSamples ? availableSamples - replayedSamples : 0;
    if (droppedSamples == 0) {
        return;
    }

    const Uint32 now = SDL_GetTicks();
    m_WindowsPenHistoryStats.updateCount++;
    m_WindowsPenHistoryStats.availableSamples += availableSamples;
    m_WindowsPenHistoryStats.replayedSamples += replayedSamples;
    m_WindowsPenHistoryStats.droppedSamples += droppedSamples;
    m_WindowsPenHistoryStats.maxDepth =
            std::max(m_WindowsPenHistoryStats.maxDepth, availableSamples);

    if (m_WindowsPenHistoryStats.lastReportTicks != 0 &&
            now - m_WindowsPenHistoryStats.lastReportTicks <
            PEN_HISTORY_REPORT_INTERVAL_MS) {
        return;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Windows pen history: updates=%u available=%u replayed=%u "
                "dropped=%u maxDepth=%u",
                m_WindowsPenHistoryStats.updateCount,
                m_WindowsPenHistoryStats.availableSamples,
                m_WindowsPenHistoryStats.replayedSamples,
                m_WindowsPenHistoryStats.droppedSamples,
                m_WindowsPenHistoryStats.maxDepth);

    m_WindowsPenHistoryStats = {};
    m_WindowsPenHistoryStats.lastReportTicks = now;
}

void SdlInputHandler::shutdownWindowsPenInput(bool suppressPointer)
{
    cancelWindowsPenInput(suppressPointer);

    auto* context = static_cast<WindowsPenSubclassContext*>(m_WindowsPenSubclassContext);
    if (context) {
        // Wait for an in-flight callback to finish before invalidating the
        // handler pointer. The context remains valid even if detachment fails.
        std::lock_guard<std::mutex> lock(context->mutex);
        context->inputHandler = nullptr;
    }

    bool detached = !m_WindowsPenSubclassInstalled ||
            (context && context->windowDestroyed.load());
    HWND hwnd = static_cast<HWND>(m_WindowsPenWindow);
    if (!detached && (!hwnd || !IsWindow(hwnd))) {
        // Window destruction automatically removes its subclass callbacks.
        detached = true;
    }
    else if (!detached && s_RemoveWindowSubclass &&
             s_RemoveWindowSubclass(hwnd, windowsPenSubclassProc,
                                    WINDOWS_PEN_SUBCLASS_ID)) {
        detached = true;
    }

    if (context) {
        if (detached) {
            delete context;
        }
        else {
            // The callback can no longer reach this handler. Keep the stable
            // context alive until WM_NCDESTROY removes the subclass.
            context->orphaned.store(true);
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                        "Windows pen input: failed to remove window subclass");
        }
    }

    m_WindowsPenWindow = nullptr;
    m_WindowsPenSubclassContext = nullptr;
    m_WindowsPenSubclassInstalled = false;
}

bool SdlInputHandler::handleWindowsPenPointerMessage(unsigned int message, Uint64 wParamValue)
{
    if (!m_Window || !m_WindowsPenWindow ||
            !(LiGetHostFeatureFlags() & LI_FF_PEN_TOUCH_EVENTS)) {
        return false;
    }

    switch (message) {
    case WM_POINTERENTER:
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP:
    case WM_POINTERLEAVE:
    case WM_POINTERCAPTURECHANGED:
        break;
    default:
        return false;
    }

    const UINT32 pointerId = GET_POINTERID_WPARAM(static_cast<WPARAM>(wParamValue));
    const bool terminalMessage = message == WM_POINTERUP ||
            message == WM_POINTERLEAVE || message == WM_POINTERCAPTURECHANGED;
    const bool hasInputFocus =
            (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_INPUT_FOCUS) != 0;

    if (m_WindowsPenCancelPending && !trySendWindowsPenCancel()) {
        // Do not send more pen state until the remote reset has been queued.
        return true;
    }

    if (pointerId == m_WindowsPenSuppressedPointerId) {
        // The remote state for this pointer was cancelled during a focus,
        // capture, or transport failure. Do not resume it with a MOVE that has
        // no matching DOWN. Hover may resume after contact ends at UP.
        if (message == WM_POINTERDOWN ||
                IS_POINTER_NEW_WPARAM(static_cast<WPARAM>(wParamValue))) {
            // Windows may recycle an ID after the old pointer lifetime ends
            // without delivering its terminal message to this window. A new
            // pointer marker is safe to treat as a fresh sequence.
            m_WindowsPenSuppressedPointerId = UINT32_MAX;
        }
        else {
            if (terminalMessage) {
                m_WindowsPenSuppressedPointerId = UINT32_MAX;
            }
            return true;
        }
    }

    if (pointerId == m_WindowsPenFallbackPointerId) {
        // Once SDL handles a pointer message, keep the rest of that pen's
        // proximity lifetime on the same path. WM_POINTERUP ends contact but
        // the pen may continue sending hover updates until it leaves range.
        if (message == WM_POINTERLEAVE || message == WM_POINTERCAPTURECHANGED) {
            m_WindowsPenFallbackPointerId = UINT32_MAX;
        }
        // While the window remains unfocused, consume the remainder locally.
        // Once focus is gained, let SDL continue the fallback sequence.
        return !hasInputFocus;
    }

    POINTER_INPUT_TYPE pointerType = PT_POINTER;
    bool isPen = GetPointerType(pointerId, &pointerType) && pointerType == PT_PEN;
    if (!isPen && terminalMessage && m_WindowsPenPointerTracked &&
            pointerId == m_WindowsPenPointerId) {
        // Windows can discard type/details before a terminal notification.
        isPen = true;
    }
    if (!isPen) {
        if (m_WindowsPenPointerTracked && pointerId == m_WindowsPenPointerId) {
            routeWindowsPenPointerToSdl(pointerId);
        }
        return false;
    }

    if (!hasInputFocus) {
        // Let the initial contact follow SDL's normal window activation path.
        // Once focus is lost, consume the remaining pen sequence locally so it
        // cannot re-enter through SDL's synthetic touch fallback after cancel.
        if (message == WM_POINTERDOWN) {
            routeWindowsPenPointerToSdl(pointerId);
            return false;
        }
        return true;
    }

    if (m_WindowsPenPointerTracked && pointerId != m_WindowsPenPointerId) {
        cancelWindowsPenInput();
    }

    POINTER_PEN_INFO currentInfo = {};
    if (!GetPointerPenInfo(pointerId, &currentInfo)) {
        if (!terminalMessage) {
            routeWindowsPenPointerToSdl(pointerId);
            return false;
        }

        if (!m_WindowsPenPointerTracked || pointerId != m_WindowsPenPointerId) {
            return false;
        }
        cancelWindowsPenInput();
        return true;
    }

    auto cleanUpTerminalState = [&]() {
        if (!m_WindowsPenPointerTracked || pointerId != m_WindowsPenPointerId) {
            return false;
        }

        cancelWindowsPenInput();
        return true;
    };

    const bool currentInfoCanceled =
            (currentInfo.pointerInfo.pointerFlags & POINTER_FLAG_CANCELED) != 0;

    HWND hwnd = static_cast<HWND>(m_WindowsPenWindow);
    RECT clientRect = {};
    if (!GetClientRect(hwnd, &clientRect)) {
        if (terminalMessage || currentInfoCanceled) {
            return cleanUpTerminalState();
        }
        routeWindowsPenPointerToSdl(pointerId);
        return false;
    }

    SDL_Rect src = { 0, 0, m_StreamWidth, m_StreamHeight };
    SDL_Rect dst = { 0, 0, static_cast<int>(clientRect.right - clientRect.left),
                     static_cast<int>(clientRect.bottom - clientRect.top) };
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);
    if (dst.w <= 0 || dst.h <= 0) {
        if (terminalMessage || currentInfoCanceled) {
            return cleanUpTerminalState();
        }
        routeWindowsPenPointerToSdl(pointerId);
        return false;
    }

    m_WindowsPenPointerId = pointerId;
    m_WindowsPenPointerTracked = true;

    auto eventTypeForSample = [message](const POINTER_PEN_INFO& sample) -> uint8_t {
        if (sample.pointerInfo.pointerFlags & POINTER_FLAG_CANCELED) {
            return LI_TOUCH_EVENT_CANCEL;
        }

        switch (message) {
        case WM_POINTERDOWN:
            return LI_TOUCH_EVENT_DOWN;
        case WM_POINTERUP:
            return LI_TOUCH_EVENT_UP;
        case WM_POINTERLEAVE:
            return LI_TOUCH_EVENT_HOVER_LEAVE;
        case WM_POINTERCAPTURECHANGED:
            return LI_TOUCH_EVENT_CANCEL;
        case WM_POINTERENTER:
        case WM_POINTERUPDATE:
        default:
            return (sample.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) ?
                        LI_TOUCH_EVENT_MOVE : LI_TOUCH_EVENT_HOVER;
        }
    };

    bool coordinateConversionFailed = false;
    bool transportFailed = false;
    auto sendPenSample = [&](const POINTER_PEN_INFO& sample) {
        POINT point = sample.pointerInfo.ptPixelLocation;
        if (!ScreenToClient(hwnd, &point)) {
            coordinateConversionFailed = true;
            return false;
        }

        const int x = qMin(qMax(static_cast<int>(point.x), dst.x), dst.x + dst.w) - dst.x;
        const int y = qMin(qMax(static_cast<int>(point.y), dst.y), dst.y + dst.h) - dst.y;
        const float normalizedX = static_cast<float>(x) / dst.w;
        const float normalizedY = static_cast<float>(y) / dst.h;

        const bool inContact = (sample.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) != 0;
        float pressure = 0.0f;
        if (inContact && (sample.penMask & PEN_MASK_PRESSURE)) {
            pressure = qBound(0.0f, static_cast<float>(sample.pressure) / 1024.0f, 1.0f);
        }

        const uint8_t eventType = eventTypeForSample(sample);
        const uint8_t toolType = (sample.penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER)) ?
                                     LI_TOOL_TYPE_ERASER : LI_TOOL_TYPE_PEN;
        const uint8_t penButtons =
                eventType == LI_TOUCH_EVENT_CANCEL ||
                eventType == LI_TOUCH_EVENT_HOVER_LEAVE ? 0 :
                (sample.penFlags & PEN_FLAG_BARREL ? LI_PEN_BUTTON_PRIMARY : 0);

        // POINTER_PEN_INFO::rotation is barrel twist, while Moonlight rotation
        // is the tool azimuth in the screen plane. Derive azimuth and tilt from
        // the Windows X/Y tilt axes instead.
        uint16_t rotation = LI_ROT_UNKNOWN;
        uint8_t tilt = LI_TILT_UNKNOWN;
        if (sample.penMask & (PEN_MASK_TILT_X | PEN_MASK_TILT_Y)) {
            const int tiltX = (sample.penMask & PEN_MASK_TILT_X) ? sample.tiltX : 0;
            const int tiltY = (sample.penMask & PEN_MASK_TILT_Y) ? sample.tiltY : 0;
            const double tanX = qTan(qDegreesToRadians(static_cast<double>(tiltX)));
            const double tanY = qTan(qDegreesToRadians(static_cast<double>(tiltY)));
            const double tiltFromNormal = qAtan(qSqrt(tanX * tanX + tanY * tanY));
            tilt = static_cast<uint8_t>(qBound(
                    0, qRound(qRadiansToDegrees(tiltFromNormal)), 90));

            if ((tiltX != 0 || tiltY != 0) &&
                    qAbs(tiltX) != 90 && qAbs(tiltY) != 90) {
                // Sunshine reconstructs Windows tilt axes as
                // X=-sin(rotation)*tan(tilt), Y=cos(rotation)*tan(tilt).
                // Use the inverse conversion so the remote axes match the
                // local POINTER_PEN_INFO values instead of being flipped.
                double rotationRad = qAtan2(-tanX, tanY);
                if (rotationRad < 0.0) {
                    rotationRad += qDegreesToRadians(360.0);
                }
                rotation = static_cast<uint16_t>(
                        qRound(qRadiansToDegrees(rotationRad)) % 360);
            }
        }

        if (LiSendPenEvent(eventType, toolType, penButtons,
                           normalizedX, normalizedY, pressure, 0.0f, 0.0f,
                           rotation, tilt) != 0) {
            transportFailed = true;
            return false;
        }
        m_WindowsPenLastSentStateKey = windowsPenStateKey(sample);
        m_WindowsPenLastSentStateValid = true;
        return true;
    };

    bool sentAllSamples = true;
    if (message == WM_POINTERUPDATE && currentInfo.pointerInfo.historyCount > 1) {
        const UINT32 capacity = std::min(
                currentInfo.pointerInfo.historyCount,
                static_cast<UINT32>(PenHistory::MAX_HISTORY_SAMPLES));
        std::array<POINTER_PEN_INFO, PenHistory::MAX_HISTORY_SAMPLES> history = {};
        UINT32 available = capacity;
        if (GetPointerPenInfoHistory(pointerId, &available, history.data())) {
            const UINT32 valid = std::min(capacity, available);
            if (valid == 0) {
                sentAllSamples = sendPenSample(currentInfo);
                recordWindowsPenHistoryStats(currentInfo.pointerInfo.historyCount,
                                             sentAllSamples ? 1 : 0);
            }
            else {
                std::array<std::uint32_t, PenHistory::MAX_HISTORY_SAMPLES> timestamps = {};
                std::array<std::uint32_t, PenHistory::MAX_HISTORY_SAMPLES> stateKeys = {};
                for (UINT32 i = 0; i < valid; i++) {
                    timestamps[i] = history[i].pointerInfo.dwTime;
                    stateKeys[i] = windowsPenStateKey(history[i]);
                }

                const PenHistory::ReplaySelection selection =
                        PenHistory::selectReplaySamples(
                                timestamps.data(), stateKeys.data(), valid, GetTickCount(),
                                m_WindowsPenLastSentStateValid,
                                m_WindowsPenLastSentStateKey);

                UINT32 replayedSamples = 0;
                for (std::size_t i = 0; i < selection.count; i++) {
                    if (!sendPenSample(history[selection.oldestFirstIndices[i]])) {
                        sentAllSamples = false;
                        break;
                    }
                    replayedSamples++;
                }
                recordWindowsPenHistoryStats(currentInfo.pointerInfo.historyCount,
                                             replayedSamples);
            }
        }
        else {
            sentAllSamples = sendPenSample(currentInfo);
            recordWindowsPenHistoryStats(currentInfo.pointerInfo.historyCount,
                                         sentAllSamples ? 1 : 0);
        }
    }
    else {
        sentAllSamples = sendPenSample(currentInfo);
    }

    if (!sentAllSamples && transportFailed) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                    "Windows pen input queue rejected an event; cancelling pointer %u",
                    pointerId);
        cancelWindowsPenInput(true);
        if (terminalMessage || currentInfoCanceled) {
            m_WindowsPenSuppressedPointerId = UINT32_MAX;
        }
        return true;
    }

    if (!sentAllSamples && coordinateConversionFailed) {
        if (terminalMessage || currentInfoCanceled) {
            return cleanUpTerminalState();
        }
        routeWindowsPenPointerToSdl(pointerId);
        return false;
    }

    if (!m_DisabledTouchFeedback) {
        disableTouchFeedback();
        m_DisabledTouchFeedback = true;
    }

    if (currentInfoCanceled && !terminalMessage) {
        // A cancelled pointer can still be followed by messages for the same
        // lifetime. Keep it suppressed until contact or proximity ends.
        m_WindowsPenPointerId = 0;
        m_WindowsPenPointerTracked = false;
        m_WindowsPenSuppressedPointerId = pointerId;
    }
    else if (message == WM_POINTERLEAVE || message == WM_POINTERCAPTURECHANGED ||
             currentInfoCanceled) {
        m_WindowsPenPointerId = 0;
        m_WindowsPenPointerTracked = false;
    }

    return true;
}

#endif
