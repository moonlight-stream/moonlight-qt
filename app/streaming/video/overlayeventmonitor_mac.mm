#include "overlayeventmonitor_mac.h"

#import <AppKit/AppKit.h>

#include <utility>

MacOverlayEventMonitor::MacOverlayEventMonitor(std::function<void()> wakeCallback)
    : m_WakeCallback(std::move(wakeCallback))
{
}

MacOverlayEventMonitor::~MacOverlayEventMonitor()
{
    detach();
}

bool MacOverlayEventMonitor::attach(void* nativeView)
{
    @autoreleasepool {
        NSView* view = static_cast<NSView*>(nativeView);
        NSWindow* window = view.window;
        if (window == nil) {
            detach();
            return false;
        }

        const NSInteger windowNumber = window.windowNumber;
        if (m_Monitor != nullptr && m_WindowNumber == windowNumber) {
            return true;
        }

        detach();

        const NSEventMask pointerEventMask =
                NSEventMaskLeftMouseDown |
                NSEventMaskLeftMouseUp |
                NSEventMaskRightMouseDown |
                NSEventMaskRightMouseUp |
                NSEventMaskOtherMouseDown |
                NSEventMaskOtherMouseUp |
                NSEventMaskMouseMoved |
                NSEventMaskLeftMouseDragged |
                NSEventMaskRightMouseDragged |
                NSEventMaskOtherMouseDragged |
                NSEventMaskMouseEntered |
                NSEventMaskMouseExited |
                NSEventMaskCursorUpdate |
                NSEventMaskScrollWheel |
                NSEventMaskTabletPoint |
                NSEventMaskTabletProximity |
                NSEventMaskGesture |
                NSEventMaskMagnify |
                NSEventMaskSwipe |
                NSEventMaskRotate |
                NSEventMaskBeginGesture |
                NSEventMaskEndGesture |
                NSEventMaskSmartMagnify |
                NSEventMaskPressure |
                NSEventMaskDirectTouch;

        const auto callback = m_WakeCallback;
        id monitor = [NSEvent addLocalMonitorForEventsMatchingMask:pointerEventMask
                                                          handler:^NSEvent*(NSEvent* event) {
            if (event.windowNumber == windowNumber) {
                callback();
            }
            return event;
        }];
        m_Monitor = [monitor retain];
        m_WindowNumber = windowNumber;
        return m_Monitor != nullptr;
    }
}

void MacOverlayEventMonitor::detach()
{
    @autoreleasepool {
        id monitor = static_cast<id>(m_Monitor);
        if (monitor != nil) {
            [NSEvent removeMonitor:monitor];
            [monitor release];
            m_Monitor = nullptr;
        }
        m_WindowNumber = 0;
    }
}
