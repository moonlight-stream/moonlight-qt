#pragma once

#include <SDL.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

extern "C" {
#include "gip_bridge.h"
}

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
// over USB by gip_bridge's Rust core and fed in here instead.
class GipBridgeController
{
public:
    GipBridgeController();
    ~GipBridgeController();

    GipBridgeController(const GipBridgeController&) = delete;
    GipBridgeController& operator=(const GipBridgeController&) = delete;

private:
    void run();
    bool waitUnlessStopping(int ms);

    static int SDLCALL rumbleCallback(void* userdata, Uint16 lowFreqRumble, Uint16 highFreqRumble);

    std::thread m_Thread;
    std::atomic<bool> m_Stop;
    std::mutex m_Mutex;
    std::condition_variable m_Cv;

    GipDevice* m_Device;
    int m_JoystickDeviceIndex;
    SDL_Joystick* m_Joystick;
};
