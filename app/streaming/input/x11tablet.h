#pragma once

#include "SDL_compat.h"

#include <cstdint>
#include <functional>

struct StylusEvent {
    uint8_t eventType;   // LI_TOUCH_EVENT_*
    uint8_t toolType;    // LI_TOOL_TYPE_*
    uint8_t penButtons;  // LI_PEN_BUTTON_*
    float x;
    float y;
    float pressure;
    float contactMajor;
    float contactMinor;
    uint16_t rotation;
    uint8_t tilt;
};

class X11TabletManager
{
public:
    X11TabletManager();
    ~X11TabletManager();

    bool initialize(SDL_Window* window, int streamWidth, int streamHeight);
    void shutdown();
    void setStreamDimensions(int streamWidth, int streamHeight);

    bool isActive() const;
    bool wantsPolling() const;

    void pumpEvents(const std::function<void(const StylusEvent&)>& handler);

private:
    struct DeviceState;

    bool setupXInput2();
    void refreshDevices();
    void handleDeviceEvent(void* eventData, int eventType, const std::function<void(const StylusEvent&)>& handler);
    bool normalizeWindowCoords(double x, double y, float* outX, float* outY);
    bool readValuator(void* eventData, int index, double* value) const;
    float normalizeValuator(double value, double min, double max) const;

    bool m_Initialized;
    bool m_HasTabletDevices;
    SDL_Window* m_Window;
    int m_StreamWidth;
    int m_StreamHeight;
};
