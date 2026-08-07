#pragma once

#include <SDL.h>

#include <cstdint>
#include <dispatch/dispatch.h>
#include <vector>

// GipUsbDevice is an Objective-C class (gipusbdevice.h/.mm). This header is
// included from plain C++ translation units too (input.h -> input.cpp), so
// its type is forward-declared as opaque here rather than imported - the
// standard pattern for mixing an Objective-C pointer into a header usable
// from both Objective-C++ and plain C++. Only gipbridge.mm (Objective-C++)
// ever dereferences it.
#ifdef __OBJC__
@class GipUsbDevice;
#else
typedef struct objc_object GipUsbDevice;
#endif

// Bridges a GIP-protocol controller (Xbox One/Series-generation accessories,
// e.g. 8BitDo's Xbox-licensed pads) into this process's SDL instance as a
// virtual game controller.
//
// macOS has no HID class driver for these controllers, and creating a
// system-wide virtual HID device requires an Apple entitlement
// (com.apple.developer.hid.virtual.device) that Apple restricts to
// virtualization software vendors. SDL's virtual joystick API has no such
// restriction because it never touches the OS HID stack - it only exists
// inside the process that created it - so the controller is read directly
// over USB (via GipUsbDevice/IOUSBHost) and fed in here instead.
class GipBridgeController
{
public:
    GipBridgeController();
    ~GipBridgeController();

    GipBridgeController(const GipBridgeController&) = delete;
    GipBridgeController& operator=(const GipBridgeController&) = delete;

private:
    void startScanTimer();
    void tryOpen();
    void handleData(const uint8_t* bytes, size_t length);
    void handleDisconnect();
    void attachVirtualJoystick();
    void detachVirtualJoystick();
    void checkRumbleWatchdog();

    static int SDLCALL rumbleCallback(void* userdata, Uint16 lowFreqRumble, Uint16 highFreqRumble);

    GipUsbDevice* m_UsbDevice;
    dispatch_queue_t m_ScanQueue;
    dispatch_source_t m_ScanTimer;

    int m_JoystickDeviceIndex;
    SDL_Joystick* m_Joystick;

    std::vector<uint8_t> m_PendingBytes;
    bool m_GuidePressed;

    // GIP treats a repeated packet sequence number as a retransmission of an
    // already-handled command and silently ignores it - rumble is called
    // repeatedly over the life of a connection (unlike the one-shot power/LED
    // packets), so it needs a sequence number that actually keeps changing,
    // or a later "stop" command can be dropped by the controller's firmware
    // and an earlier effect's programmed duration/repeat count keeps playing.
    uint8_t m_RumbleSequence;

    // CFAbsoluteTimeGetCurrent() deadline for the rumble watchdog, or 0 if
    // disarmed. SdlInputHandler::rumble() calls SDL_GameControllerRumble()
    // with a 30s duration on every host-forwarded rumble update, relying on
    // SDL's own rumble_expiration timeout (checked in SDL_UpdateJoysticks())
    // to zero the motors if updates stop arriving - observed on hardware to
    // not actually fire for this virtual joystick, leaving a stuck-on rumble
    // indefinitely. This watchdog is a redundant safety net enforcing that
    // same 30s contract independently of whether SDL's own timeout works.
    double m_RumbleWatchdogDeadline;
};
