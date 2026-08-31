#pragma once

#include <functional>

class MacOverlayEventMonitor
{
public:
    explicit MacOverlayEventMonitor(std::function<void()> wakeCallback);
    ~MacOverlayEventMonitor();

    MacOverlayEventMonitor(const MacOverlayEventMonitor&) = delete;
    MacOverlayEventMonitor& operator=(const MacOverlayEventMonitor&) = delete;

    bool attach(void* nativeView);
    void detach();
    bool isAttached() const { return m_Monitor != nullptr; }

private:
    std::function<void()> m_WakeCallback;
    void* m_Monitor = nullptr;
    long m_WindowNumber = 0;
};
