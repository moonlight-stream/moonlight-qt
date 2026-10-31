#include "input.h"

#import <AppKit/AppKit.h>

// SDL2 doesn't tell us whether a scroll event is inertial momentum or real finger
// movement, and it ignores trackpad pinch gestures entirely. A local event monitor
// sees each NSEvent before SDL does, so we can drop or handle those ourselves.
void SdlInputHandler::installMacEventMonitor(bool dropScrollMomentum, bool pinchToZoom)
{
    NSEventMask mask = 0;
    if (dropScrollMomentum) {
        mask |= NSEventMaskScrollWheel;
    }
    if (pinchToZoom) {
        mask |= NSEventMaskMagnify;
    }

    id monitor = [NSEvent addLocalMonitorForEventsMatchingMask:mask handler:^NSEvent*(NSEvent* event) {
        if (event.type == NSEventTypeScrollWheel) {
            // Momentum events follow a trackpad swipe after the fingers lift
            return event.momentumPhase != NSEventPhaseNone ? nil : event;
        }
        else if (event.type == NSEventTypeMagnify) {
            handlePinchGesture(event.magnification,
                               event.phase & NSEventPhaseBegan,
                               event.phase & (NSEventPhaseEnded | NSEventPhaseCancelled));
            return nil;
        }

        return event;
    }];

    // Keep the monitor alive until we remove it
    m_MacEventMonitor = (void*)CFBridgingRetain(monitor);
}

void SdlInputHandler::removeMacEventMonitor()
{
    if (m_MacEventMonitor != nullptr) {
        [NSEvent removeMonitor:(id)CFBridgingRelease(m_MacEventMonitor)];
        m_MacEventMonitor = nullptr;
    }

    endPinchGesture();
}
