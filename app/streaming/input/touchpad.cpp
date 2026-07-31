#include "input.h"

#include <Limelight.h>
#include "streaming/session.h"

void SdlInputHandler::selectNativeTouchpadTransport()
{
    if (m_NativeTouchpadTransport != NTT_UNKNOWN) {
        return;
    }

    uint32_t hostFeatures = LiGetHostFeatureFlags();
    if (hostFeatures & LI_FF_TOUCHPAD_FRAME_EVENTS) {
        m_NativeTouchpadTransport = NTT_FRAME;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Native touchpad transport selected: frame (hostFeatures=0x%08x)",
                    hostFeatures);
    }
    else if (hostFeatures & LI_FF_TOUCHPAD_EVENTS) {
        m_NativeTouchpadTransport = NTT_INDIVIDUAL;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Native touchpad transport selected: individual (hostFeatures=0x%08x)",
                    hostFeatures);
    }
    else {
        m_NativeTouchpadTransport = NTT_SOFTWARE_POINTER;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Native touchpad transport selected: software-pointer (hostFeatures=0x%08x)",
                    hostFeatures);
    }
}

void SdlInputHandler::handleNativeTouchpadEvent(SDL_TouchFingerEvent* event)
{
    if (!isCaptureActive()) {
        // Not capturing
        return;
    }

    selectNativeTouchpadTransport();

    uint8_t eventType;
    switch (event->type) {
    case SDL_FINGERDOWN: eventType = LI_TOUCH_EVENT_DOWN; break;
    case SDL_FINGERMOTION: eventType = LI_TOUCH_EVENT_MOVE; break;
    case SDL_FINGERUP: eventType = LI_TOUCH_EVENT_UP; break;
    default: return;
    }

    if (m_NativeTouchpadTransport == NTT_SOFTWARE_POINTER) {
#ifdef HAVE_MACOS_NATIVE_TOUCHPAD
        updateMacTouchpadGesture(event);
#endif
        handleRelativeFingerEvent(event);
        return;
    }

    if ((!m_ActiveTouchpadContacts.isEmpty() || !m_IgnoredTouchpadContacts.isEmpty()) &&
            event->touchId != m_ActiveTouchpadId) {
        if (eventType != LI_TOUCH_EVENT_DOWN) {
            // Ignore delayed events from a touch surface that no longer owns
            // the native touchpad stream.
            return;
        }

        // The wire protocol has no device identifier. Finish contacts from the
        // previous device before switching to another touch surface.
        cancelSdlTouchpadContacts();
    }

#ifdef HAVE_MACOS_NATIVE_TOUCHPAD
    updateMacTouchpadGesture(event);
#endif

    if (m_IgnoredTouchpadContacts.contains(event->fingerId)) {
        if (eventType == LI_TOUCH_EVENT_UP || eventType == LI_TOUCH_EVENT_CANCEL) {
            m_IgnoredTouchpadContacts.remove(event->fingerId);
            if (m_ActiveTouchpadContacts.isEmpty() && m_IgnoredTouchpadContacts.isEmpty()) {
                m_ActiveTouchpadId = 0;
            }
        }
        return;
    }

    bool contactIsActive = m_ActiveTouchpadContacts.contains(event->fingerId);
    bool hadMultipleContacts = m_ActiveTouchpadContacts.size() >= 2;
    if (eventType == LI_TOUCH_EVENT_DOWN && !contactIsActive &&
            m_ActiveTouchpadContacts.size() >= MAX_TOUCHPAD_FRAME_CONTACTS) {
        m_IgnoredTouchpadContacts.insert(event->fingerId);
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Native touchpad contact limit reached; ignoring fingerId=%lld",
                    static_cast<long long>(event->fingerId));
        return;
    }
    if (eventType != LI_TOUCH_EVENT_DOWN && !contactIsActive) {
        return;
    }

    m_ActiveTouchpadId = event->touchId;

#ifdef HAVE_WINDOWS_RAW_TOUCHPAD
    if (!m_ActiveWindowsTouchpadContacts.isEmpty() || m_WindowsTouchpadButtonDown ||
            m_WindowsTouchpadButtonUsesMouseFallback) {
        // The wire protocol has no input-source identifier. A valid SDL
        // contact takes ownership from Windows Raw Input before it is sent.
        cancelWindowsTouchpadContacts();
    }
#endif

    if (m_PendingTouchpadContactCount > 0 &&
            (event->touchId != m_PendingTouchpadId ||
             event->timestamp != m_PendingTouchpadTimestamp)) {
        sendPendingTouchpadFrame();
    }
    if (m_PendingTouchpadContactCount == MAX_TOUCHPAD_FRAME_CONTACTS) {
        sendPendingTouchpadFrame();
    }
    if (m_PendingTouchpadContactCount == 0) {
        m_PendingTouchpadId = event->touchId;
        m_PendingTouchpadTimestamp = event->timestamp;
    }

    NativeTouchpadContact contact = {
        eventType,
        // The wire protocol uses 32-bit pointer IDs, so keep the low 32 bits of SDL's 64-bit finger ID.
        static_cast<uint32_t>(event->fingerId),
        SDL_clamp(event->x, 0.0f, 1.0f),
        SDL_clamp(event->y, 0.0f, 1.0f),
        SDL_clamp(event->pressure, 0.0f, 1.0f),
    };
    m_PendingTouchpadContacts[m_PendingTouchpadContactCount++] = contact;

    if (eventType == LI_TOUCH_EVENT_UP || eventType == LI_TOUCH_EVENT_CANCEL) {
        m_ActiveTouchpadContacts.remove(event->fingerId);
    }
    else {
        m_ActiveTouchpadContacts.insert(event->fingerId, contact);
    }
    const bool isMultiContactFrame = hadMultipleContacts || m_ActiveTouchpadContacts.size() >= 2;
    if (isMultiContactFrame) {
        m_LastTouchpadScrollTimestamp = event->timestamp;
    }
    if (m_ActiveTouchpadContacts.isEmpty() && m_IgnoredTouchpadContacts.isEmpty()) {
        m_ActiveTouchpadId = 0;
    }

    // A single-contact frame can be sent immediately for the lowest possible
    // pointer latency. SDL reports contacts separately for multi-contact
    // frames, so defer those until the current event batch has been collected.
    if (!isMultiContactFrame) {
        sendPendingTouchpadFrame();
        return;
    }

    // Queue a single event behind the current batch to collect the remaining
    // contacts that belong to this multi-contact frame.
    if (!m_TouchpadFlushEventQueued) {
        if (Session::queueTouchpadFrameFlush()) {
            m_TouchpadFlushEventQueued = true;
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Unable to queue touchpad frame flush: %s", SDL_GetError());
            sendPendingTouchpadFrame();
        }
    }
}

#ifdef HAVE_MACOS_NATIVE_TOUCHPAD
void SdlInputHandler::updateMacTouchpadGesture(const SDL_TouchFingerEvent* event)
{
    if (event->type == SDL_FINGERDOWN &&
            m_MacTouchpadGestureContacts.isEmpty()) {
        m_MacTouchpadGestureStartTimestamp = event->timestamp;
        m_MacTouchpadGesturePrimaryFinger = event->fingerId;
        m_MacTouchpadGestureStartX = event->x;
        m_MacTouchpadGestureStartY = event->y;
        m_MacTouchpadGestureMaxContacts = 1;
        m_MacTouchpadGestureMoved = false;
        m_MacTouchpadGestureHadPhysicalButton = false;
        m_MacTouchpadPendingTapButtons = 0;
    }

    if (event->type == SDL_FINGERDOWN) {
        m_MacTouchpadGestureContacts.insert(event->fingerId);
    }
    else if (event->type == SDL_FINGERUP) {
        m_MacTouchpadGestureContacts.remove(event->fingerId);
    }

    m_MacTouchpadGestureMaxContacts =
            qMax(m_MacTouchpadGestureMaxContacts,
                 static_cast<int>(m_MacTouchpadGestureContacts.size()));

    if (event->fingerId == m_MacTouchpadGesturePrimaryFinger) {
        const float deltaX = event->x - m_MacTouchpadGestureStartX;
        const float deltaY = event->y - m_MacTouchpadGestureStartY;
        const float maxMovement = MACOS_TOUCHPAD_TAP_MAX_MOVEMENT;
        if (deltaX * deltaX + deltaY * deltaY > maxMovement * maxMovement) {
            m_MacTouchpadGestureMoved = true;
        }
    }

    if (!m_MacTouchpadGestureContacts.isEmpty()) {
        return;
    }

    const Uint32 gestureDuration =
            event->timestamp - m_MacTouchpadGestureStartTimestamp;
    if (!m_MacTouchpadGestureHadPhysicalButton &&
            !m_MacTouchpadGestureMoved &&
            m_MacTouchpadGestureMaxContacts > 0 &&
            m_MacTouchpadGestureMaxContacts <= 2 &&
            gestureDuration <= MACOS_TOUCHPAD_TAP_MAX_DURATION_MS) {
        m_MacTouchpadPendingTapButtons =
                m_MacTouchpadGestureMaxContacts == 1 ?
                    SDL_BUTTON(SDL_BUTTON_LEFT) :
                    SDL_BUTTON(SDL_BUTTON_RIGHT);
        m_MacTouchpadLastTapTimestamp = event->timestamp;
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT,
                     "macOS trackpad tap candidate armed: fingers=%d timestamp=%u",
                     m_MacTouchpadGestureMaxContacts, event->timestamp);
    }
    else {
        m_MacTouchpadPendingTapButtons = 0;
        m_MacTouchpadLastTapTimestamp = 0;
    }

    m_MacTouchpadGestureStartTimestamp = 0;
    m_MacTouchpadGesturePrimaryFinger = 0;
    m_MacTouchpadGestureMaxContacts = 0;
    m_MacTouchpadGestureMoved = false;
    m_MacTouchpadGestureHadPhysicalButton = false;
}

bool SdlInputHandler::sendMacTouchpadButtonState(bool down)
{
    const uint8_t buttonState = down ? LI_TOUCHPAD_BUTTON_PRIMARY : 0;

    if (m_NativeTouchpadTransport == NTT_FRAME) {
        int rc = LiSendTouchpadFrameEvent(0, nullptr, nullptr, nullptr, nullptr,
                                          nullptr, LI_ROT_UNKNOWN, 0, 0,
                                          buttonState);
        if (rc == 0) {
            return true;
        }
        if (rc != LI_ERR_UNSUPPORTED) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "LiSendTouchpadFrameEvent for macOS button failed: %d", rc);
            return false;
        }

        if (LiGetHostFeatureFlags() & LI_FF_TOUCHPAD_EVENTS) {
            m_NativeTouchpadTransport = NTT_INDIVIDUAL;
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Native touchpad transport downgraded: frame -> individual");
        }
        else {
            m_NativeTouchpadTransport = NTT_SOFTWARE_POINTER;
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Native touchpad transport downgraded: frame -> software-pointer");
            transitionNativeTouchpadToSoftwarePointer();
            return false;
        }
    }

    if (m_NativeTouchpadTransport == NTT_INDIVIDUAL) {
        int rc = LiSendTouchpadEvent(LI_TOUCH_EVENT_BUTTON_ONLY, 0,
                                     0.0f, 0.0f, 0.0f,
                                     0.0f, 0.0f, LI_ROT_UNKNOWN,
                                     0, 0, buttonState);
        if (rc == 0) {
            return true;
        }
        if (rc == LI_ERR_UNSUPPORTED) {
            m_NativeTouchpadTransport = NTT_SOFTWARE_POINTER;
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Native touchpad transport downgraded: individual -> software-pointer");
            transitionNativeTouchpadToSoftwarePointer();
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "LiSendTouchpadEvent for macOS button failed: %d", rc);
        }
    }

    return false;
}

bool SdlInputHandler::shouldSuppressMacTouchpadMouseButtonEvent(
        const SDL_MouseButtonEvent* event)
{
    if (!m_NativeTouchpadEnabled ||
            m_NativeTouchpadTransport == NTT_UNKNOWN ||
            event->button < SDL_BUTTON_LEFT ||
            event->button > SDL_BUTTON_X2) {
        return false;
    }

    const Uint32 buttonMask = SDL_BUTTON(event->button);
    if (event->state == SDL_RELEASED) {
        if (!(m_MacTouchpadSuppressedMouseButtons & buttonMask)) {
            return false;
        }

        m_MacTouchpadSuppressedMouseButtons &= ~buttonMask;
        if (m_MacTouchpadButtonDown &&
                m_MacTouchpadSuppressedMouseButtons == 0) {
            sendMacTouchpadButtonState(false);
            m_MacTouchpadButtonDown = false;
        }
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT,
                     "Suppressed macOS trackpad mouse button release: button=%d",
                     static_cast<int>(event->button));
        return true;
    }

    if (event->state != SDL_PRESSED || !isCaptureActive()) {
        return false;
    }

    const bool hasActiveContacts = !m_MacTouchpadGestureContacts.isEmpty();
    const bool hasCorrelatedTap =
            (m_MacTouchpadPendingTapButtons & buttonMask) &&
            event->timestamp - m_MacTouchpadLastTapTimestamp <=
                MACOS_TOUCHPAD_CLICK_CORRELATION_TIMEOUT_MS;
    if (!hasActiveContacts && !hasCorrelatedTap) {
        if (m_MacTouchpadPendingTapButtons != 0 &&
                event->timestamp - m_MacTouchpadLastTapTimestamp >
                    MACOS_TOUCHPAD_CLICK_CORRELATION_TIMEOUT_MS) {
            m_MacTouchpadPendingTapButtons = 0;
            m_MacTouchpadLastTapTimestamp = 0;
        }
        return false;
    }

    if (hasActiveContacts &&
            m_NativeTouchpadTransport == NTT_SOFTWARE_POINTER) {
        // The legacy relative-touch path will otherwise synthesize another
        // tap after this real click. Let the real mouse event through, but
        // invalidate tap and long-press emulation for the current gesture.
        m_MacTouchpadGestureHadPhysicalButton = true;
        SDL_RemoveTimer(m_DragTimer);
        m_DragTimer = 0;
        for (int i = 0; i < MAX_FINGERS; i++) {
            m_TouchDownEvent[i].timestamp = 0;
        }
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT,
                     "Using legacy macOS trackpad mouse button: button=%d",
                     static_cast<int>(event->button));
        return false;
    }

    if (hasActiveContacts) {
        m_MacTouchpadGestureHadPhysicalButton = true;
        if (!m_MacTouchpadButtonDown) {
            if (!sendMacTouchpadButtonState(true)) {
                return false;
            }
            m_MacTouchpadButtonDown = true;
        }
    }

    m_MacTouchpadPendingTapButtons = 0;
    m_MacTouchpadLastTapTimestamp = 0;
    m_MacTouchpadSuppressedMouseButtons |= buttonMask;
    SDL_LogDebug(SDL_LOG_CATEGORY_INPUT,
                 "Suppressed promoted macOS trackpad mouse button press: button=%d source=%s",
                 static_cast<int>(event->button),
                 hasActiveContacts ? "active-contact" : "tap");
    return true;
}

void SdlInputHandler::resetMacTouchpadState()
{
    if (m_MacTouchpadButtonDown) {
        sendMacTouchpadButtonState(false);
    }

    // Preserve suppressed buttons until SDL delivers their matching releases.
    m_MacTouchpadPendingTapButtons = 0;
    m_MacTouchpadLastTapTimestamp = 0;
    m_MacTouchpadGestureStartTimestamp = 0;
    m_MacTouchpadGesturePrimaryFinger = 0;
    m_MacTouchpadGestureContacts.clear();
    m_MacTouchpadGestureStartX = 0;
    m_MacTouchpadGestureStartY = 0;
    m_MacTouchpadGestureMaxContacts = 0;
    m_MacTouchpadGestureMoved = false;
    m_MacTouchpadGestureHadPhysicalButton = false;
    m_MacTouchpadButtonDown = false;
}
#endif

void SdlInputHandler::sendNativeTouchpadContacts(const NativeTouchpadContact* contacts,
                                                 int contactCount,
                                                 bool transitionToSoftwarePointer,
                                                 uint8_t buttonState,
                                                 uint16_t deviceWidthMm,
                                                 uint16_t deviceHeightMm)
{
    if (contactCount <= 0) {
        return;
    }

    uint8_t eventTypes[MAX_TOUCHPAD_FRAME_CONTACTS];
    uint32_t pointerIds[MAX_TOUCHPAD_FRAME_CONTACTS];
    float x[MAX_TOUCHPAD_FRAME_CONTACTS];
    float y[MAX_TOUCHPAD_FRAME_CONTACTS];
    float pressure[MAX_TOUCHPAD_FRAME_CONTACTS];

    for (int i = 0; i < contactCount; i++) {
        eventTypes[i] = contacts[i].eventType;
        pointerIds[i] = contacts[i].pointerId;
        x[i] = contacts[i].x;
        y[i] = contacts[i].y;
        pressure[i] = contacts[i].pressure;
    }

    if (m_NativeTouchpadTransport == NTT_FRAME) {
        int rc = LiSendTouchpadFrameEvent(static_cast<uint8_t>(contactCount),
                                          eventTypes, pointerIds, x, y, pressure,
                                          LI_ROT_UNKNOWN, deviceWidthMm, deviceHeightMm,
                                          buttonState);
        if (rc == LI_ERR_UNSUPPORTED) {
            if (LiGetHostFeatureFlags() & LI_FF_TOUCHPAD_EVENTS) {
                m_NativeTouchpadTransport = NTT_INDIVIDUAL;
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Native touchpad transport downgraded: frame -> individual");
            }
            else {
                m_NativeTouchpadTransport = NTT_SOFTWARE_POINTER;
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Native touchpad transport downgraded: frame -> software-pointer");
                if (transitionToSoftwarePointer) {
                    transitionNativeTouchpadToSoftwarePointer();
                }
                return;
            }
        }
        else {
            if (rc != 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "LiSendTouchpadFrameEvent failed: %d", rc);
            }
            return;
        }
    }

    if (m_NativeTouchpadTransport == NTT_INDIVIDUAL) {
        for (int i = 0; i < contactCount; i++) {
            int rc = LiSendTouchpadEvent(eventTypes[i], pointerIds[i], x[i], y[i], pressure[i],
                                         0.0f, 0.0f, LI_ROT_UNKNOWN, deviceWidthMm,
                                         deviceHeightMm, buttonState);
            if (rc == LI_ERR_UNSUPPORTED) {
                m_NativeTouchpadTransport = NTT_SOFTWARE_POINTER;
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Native touchpad transport downgraded: individual -> software-pointer");
                if (transitionToSoftwarePointer) {
                    transitionNativeTouchpadToSoftwarePointer();
                }
                return;
            }
            else if (rc != 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "LiSendTouchpadEvent failed: %d", rc);
            }
        }
    }
}

#ifdef HAVE_WINDOWS_RAW_TOUCHPAD
void SdlInputHandler::handleWindowsTouchpadFrame(uint64_t deviceId,
                                                 const uint32_t* pointerIds,
                                                 const float* x, const float* y,
                                                 const float* pressure,
                                                 const uint8_t* touching,
                                                 int contactCount,
                                                 bool hasContactFrame,
                                                 bool buttonDown,
                                                 uint16_t deviceWidthMm,
                                                 uint16_t deviceHeightMm)
{
    if (!isCaptureActive() || m_AbsoluteMouseMode) {
        cancelWindowsTouchpadContacts();
        return;
    }

    selectNativeTouchpadTransport();

    bool hasCurrentContact = false;
    if (hasContactFrame) {
        for (int i = 0; i < contactCount && i < MAX_TOUCHPAD_FRAME_CONTACTS; i++) {
            if (touching[i]) {
                hasCurrentContact = true;
                break;
            }
        }
    }

    const bool hasWindowsState = !m_ActiveWindowsTouchpadContacts.isEmpty() ||
            m_WindowsTouchpadButtonDown || m_WindowsTouchpadButtonUsesMouseFallback;
    const bool currentDeviceOwnsWindowsState =
            m_ActiveWindowsTouchpadDevice == deviceId && hasWindowsState;
    const bool currentFrameIsActive = hasCurrentContact || buttonDown;

    if (hasWindowsState && m_ActiveWindowsTouchpadDevice != deviceId) {
        if (!currentFrameIsActive) {
            // Ignore an empty or delayed release frame from another Raw Input
            // device instead of allowing it to steal the active stream.
            return;
        }
        cancelWindowsTouchpadContacts();
    }

    const bool windowsSourceActive = currentFrameIsActive || currentDeviceOwnsWindowsState;
    if (windowsSourceActive &&
            (m_PendingTouchpadContactCount > 0 || !m_ActiveTouchpadContacts.isEmpty() ||
             !m_IgnoredTouchpadContacts.isEmpty())) {
        // Windows Raw Input is now active. Cancel SDL touch-surface contacts
        // first because the native wire protocol cannot distinguish sources.
        cancelSdlTouchpadContacts();
    }

    if (m_NativeTouchpadTransport == NTT_SOFTWARE_POINTER) {
        m_ActiveWindowsTouchpadContacts.clear();
        m_LastWindowsTouchpadFrameTicks = 0;
        if (m_WindowsTouchpadButtonUsesMouseFallback && !buttonDown) {
            sendWindowsTouchpadMouseButton(false);
            m_WindowsTouchpadButtonUsesMouseFallback = false;
        }
        m_ActiveWindowsTouchpadDevice = m_WindowsTouchpadButtonUsesMouseFallback ? deviceId : 0;
        m_WindowsTouchpadButtonDown = m_WindowsTouchpadButtonUsesMouseFallback && buttonDown;
        return;
    }

    m_ActiveWindowsTouchpadDevice = deviceId;
    m_WindowsTouchpadWidthMm = deviceWidthMm;
    m_WindowsTouchpadHeightMm = deviceHeightMm;
    m_LastWindowsTouchpadFrameTicks = SDL_GetTicks();
    if (m_LastWindowsTouchpadFrameTicks == 0) {
        m_LastWindowsTouchpadFrameTicks = 1;
    }

    const bool buttonChanged = buttonDown != m_WindowsTouchpadButtonDown;
    bool mouseFallbackTransition = false;
    if (m_WindowsTouchpadButtonUsesMouseFallback && !buttonDown) {
        sendWindowsTouchpadMouseButton(false);
        m_WindowsTouchpadButtonUsesMouseFallback = false;
        mouseFallbackTransition = true;
    }
    else if (!m_WindowsTouchpadButtonUsesMouseFallback && buttonDown &&
             !(hasContactFrame ? hasCurrentContact :
                                 !m_ActiveWindowsTouchpadContacts.isEmpty())) {
        // Button 1 may be reported with no capacitive contact. Preserve those
        // presses through the mouse protocol because Sunshine has no pointer
        // slot to carry a native button state in that case.
        sendWindowsTouchpadMouseButton(true);
        m_WindowsTouchpadButtonUsesMouseFallback = true;
        mouseFallbackTransition = true;
    }

    const uint8_t buttonState = buttonDown && !m_WindowsTouchpadButtonUsesMouseFallback ?
            LI_TOUCHPAD_BUTTON_PRIMARY : 0;
    if (!hasContactFrame) {
        if (buttonChanged && !mouseFallbackTransition) {
            NativeTouchpadContact changes[MAX_TOUCHPAD_FRAME_CONTACTS];
            int changeCount = 0;
            for (auto it = m_ActiveWindowsTouchpadContacts.cbegin();
                 it != m_ActiveWindowsTouchpadContacts.cend(); ++it) {
                NativeTouchpadContact contact = it.value();
                contact.eventType = LI_TOUCH_EVENT_MOVE;
                changes[changeCount++] = contact;
            }
            sendNativeTouchpadContacts(changes, changeCount, false, buttonState,
                                       m_WindowsTouchpadWidthMm,
                                       m_WindowsTouchpadHeightMm);
        }
        m_WindowsTouchpadButtonDown = buttonDown;
        if (m_ActiveWindowsTouchpadContacts.isEmpty() && !buttonDown) {
            m_ActiveWindowsTouchpadDevice = 0;
        }
        return;
    }

    QHash<uint32_t, NativeTouchpadContact> currentContacts;
    NativeTouchpadContact currentChanges[MAX_TOUCHPAD_FRAME_CONTACTS];
    int currentChangeCount = 0;
    NativeTouchpadContact changes[MAX_TOUCHPAD_FRAME_CONTACTS];
    int changeCount = 0;

    auto flushChanges = [&]() {
        if (changeCount > 0) {
            sendNativeTouchpadContacts(changes, changeCount, false, buttonState,
                                       m_WindowsTouchpadWidthMm,
                                       m_WindowsTouchpadHeightMm);
            changeCount = 0;
        }
    };

    for (int i = 0; i < contactCount && i < MAX_TOUCHPAD_FRAME_CONTACTS; i++) {
        if (!touching[i]) {
            continue;
        }

        NativeTouchpadContact contact = {
            static_cast<uint8_t>(m_ActiveWindowsTouchpadContacts.contains(pointerIds[i]) ?
                                 LI_TOUCH_EVENT_MOVE : LI_TOUCH_EVENT_DOWN),
            pointerIds[i],
            SDL_clamp(x[i], 0.0f, 1.0f),
            SDL_clamp(y[i], 0.0f, 1.0f),
            SDL_clamp(pressure[i], 0.0f, 1.0f),
        };
        currentContacts.insert(contact.pointerId, contact);
        currentChanges[currentChangeCount++] = contact;
    }

    // Release old pointer IDs before sending new Down events. This ensures the
    // host has a free slot when a full five-contact frame replaces a contact.
    for (auto it = m_ActiveWindowsTouchpadContacts.cbegin();
         it != m_ActiveWindowsTouchpadContacts.cend(); ++it) {
        if (currentContacts.contains(it.key())) {
            continue;
        }

        NativeTouchpadContact contact = it.value();
        contact.eventType = LI_TOUCH_EVENT_UP;
        changes[changeCount++] = contact;
        if (changeCount == MAX_TOUCHPAD_FRAME_CONTACTS) {
            flushChanges();
        }
    }
    flushChanges();

    for (int i = 0; i < currentChangeCount; i++) {
        changes[changeCount++] = currentChanges[i];
        if (changeCount == MAX_TOUCHPAD_FRAME_CONTACTS) {
            flushChanges();
        }
    }
    flushChanges();

    m_ActiveWindowsTouchpadContacts = currentContacts;
    m_WindowsTouchpadButtonDown = buttonDown;
    if (currentContacts.isEmpty() && !buttonDown) {
        m_ActiveWindowsTouchpadDevice = 0;
    }
}

void SdlInputHandler::cancelWindowsTouchpadContacts(uint64_t deviceId)
{
    if (deviceId != 0 && deviceId != m_ActiveWindowsTouchpadDevice) {
        return;
    }

    NativeTouchpadContact contacts[MAX_TOUCHPAD_FRAME_CONTACTS];
    int contactCount = 0;
    for (auto it = m_ActiveWindowsTouchpadContacts.cbegin();
         it != m_ActiveWindowsTouchpadContacts.cend(); ++it) {
        NativeTouchpadContact contact = it.value();
        contact.eventType = LI_TOUCH_EVENT_CANCEL;
        contacts[contactCount++] = contact;
    }
    if (contactCount > 0) {
        sendNativeTouchpadContacts(contacts, contactCount, false, 0,
                                   m_WindowsTouchpadWidthMm,
                                   m_WindowsTouchpadHeightMm);
    }
    if (m_WindowsTouchpadButtonUsesMouseFallback) {
        sendWindowsTouchpadMouseButton(false);
        m_WindowsTouchpadButtonUsesMouseFallback = false;
    }

    m_ActiveWindowsTouchpadContacts.clear();
    m_ActiveWindowsTouchpadDevice = 0;
    m_LastWindowsTouchpadFrameTicks = 0;
    m_WindowsTouchpadButtonDown = false;
    m_WindowsTouchpadWidthMm = 0;
    m_WindowsTouchpadHeightMm = 0;
}

void SdlInputHandler::sendWindowsTouchpadMouseButton(bool down)
{
    const int button = m_SwapMouseButtons ? BUTTON_RIGHT : BUTTON_LEFT;
    LiSendMouseButtonEvent(down ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE, button);
}

bool SdlInputHandler::shouldSuppressWindowsTouchpadMouseEvent(Uint32 mouseId)
{
    if (m_LastWindowsTouchpadFrameTicks == 0 ||
            m_NativeTouchpadTransport == NTT_UNKNOWN ||
            m_NativeTouchpadTransport == NTT_SOFTWARE_POINTER ||
            m_AbsoluteMouseMode) {
        return false;
    }

    constexpr Uint32 SDL3_GLOBAL_MOUSE_ID = 0;
    if (mouseId != SDL3_GLOBAL_MOUSE_ID) {
        return false;
    }

    const Uint32 frameAge = SDL_GetTicks() - m_LastWindowsTouchpadFrameTicks;

    // Windows promotes touchpad gestures through SDL's global mouse device.
    // Bound suppression to recent Raw Input so a missing final HID report
    // cannot permanently disable legacy mouse input.
    return frameAge <= TOUCHPAD_SCROLL_SUPPRESSION_TIMEOUT_MS;
}

bool SdlInputHandler::shouldSuppressWindowsTouchpadMouseButtonEvent(
        const SDL_MouseButtonEvent* event)
{
    constexpr Uint32 SDL3_GLOBAL_MOUSE_ID = 0;
    if (event->which != SDL3_GLOBAL_MOUSE_ID ||
            event->button < SDL_BUTTON_LEFT || event->button > SDL_BUTTON_X2) {
        return false;
    }

    const Uint32 buttonMask = SDL_BUTTON(event->button);
    if (event->state == SDL_RELEASED) {
        if (m_SuppressedWindowsTouchpadMouseButtons & buttonMask) {
            m_SuppressedWindowsTouchpadMouseButtons &= ~buttonMask;
            return true;
        }
        return false;
    }

    if (event->state == SDL_PRESSED) {
        m_SuppressedWindowsTouchpadMouseButtons &= ~buttonMask;
        if (isCaptureActive() &&
                ((m_WindowsTouchpadButtonUsesMouseFallback &&
                  event->button == SDL_BUTTON_LEFT) ||
                 shouldSuppressWindowsTouchpadMouseEvent(event->which))) {
            m_SuppressedWindowsTouchpadMouseButtons |= buttonMask;
            return true;
        }
    }

    return false;
}
#endif

void SdlInputHandler::transitionNativeTouchpadToSoftwarePointer()
{
    // Seed the legacy relative-touch path with active contacts because it
    // cannot process Move/Up events until it has seen matching Down events.
    SDL_zero(m_TouchDownEvent);
    int relativeFingerCount = 0;
    for (int i = 0; i < SDL_GetNumTouchFingers(m_ActiveTouchpadId) &&
                    relativeFingerCount < MAX_FINGERS; i++) {
        SDL_Finger* finger = SDL_GetTouchFinger(m_ActiveTouchpadId, i);
        if (finger == nullptr || !m_ActiveTouchpadContacts.contains(finger->id)) {
            continue;
        }

        const NativeTouchpadContact& contact = m_ActiveTouchpadContacts[finger->id];
        SDL_TouchFingerEvent& downEvent = m_TouchDownEvent[relativeFingerCount++];
        downEvent.type = SDL_FINGERDOWN;
        downEvent.touchId = m_ActiveTouchpadId;
        downEvent.fingerId = finger->id;
        downEvent.timestamp = 0;
        downEvent.x = contact.x;
        downEvent.y = contact.y;
        downEvent.pressure = contact.pressure;
    }
    m_NumFingersDown = relativeFingerCount;
}

void SdlInputHandler::sendPendingTouchpadFrame()
{
    if (m_PendingTouchpadContactCount == 0) {
        return;
    }
    uint8_t buttonState = 0;
#ifdef HAVE_MACOS_NATIVE_TOUCHPAD
    buttonState = m_MacTouchpadButtonDown ? LI_TOUCHPAD_BUTTON_PRIMARY : 0;
#endif
    sendNativeTouchpadContacts(m_PendingTouchpadContacts,
                               m_PendingTouchpadContactCount,
                               true, buttonState);
    m_PendingTouchpadContactCount = 0;
    m_PendingTouchpadId = 0;
    m_PendingTouchpadTimestamp = 0;
}

void SdlInputHandler::flushPendingTouchpadFrameEvent()
{
    m_TouchpadFlushEventQueued = false;
    sendPendingTouchpadFrame();
}

void SdlInputHandler::cancelSdlTouchpadContacts()
{
    sendPendingTouchpadFrame();

#ifdef HAVE_MACOS_NATIVE_TOUCHPAD
    resetMacTouchpadState();
#endif

    if (!m_ActiveTouchpadContacts.isEmpty()) {
        const NativeTouchpadContact cancelAll = {
            LI_TOUCH_EVENT_CANCEL_ALL, 0, 0.0f, 0.0f, 0.0f,
        };
        sendNativeTouchpadContacts(&cancelAll, 1);
    }

    m_ActiveTouchpadContacts.clear();
    m_IgnoredTouchpadContacts.clear();
    m_ActiveTouchpadId = 0;
}

void SdlInputHandler::cancelNativeTouchpadContacts()
{
    cancelSdlTouchpadContacts();
    cancelRelativeTouchpadState();

#ifdef HAVE_WINDOWS_RAW_TOUCHPAD
    cancelWindowsTouchpadContacts();
#endif
}
