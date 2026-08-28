#pragma once

#include <cstdint>
#include <memory>

#include <Limelight.h>

class MacDualSenseHapticsRenderer
{
public:
    MacDualSenseHapticsRenderer();
    ~MacDualSenseHapticsRenderer();

    MacDualSenseHapticsRenderer(const MacDualSenseHapticsRenderer&) = delete;
    MacDualSenseHapticsRenderer& operator=(const MacDualSenseHapticsRenderer&) = delete;

    void setControllerTarget(int controllerNumber);
    void reset();
    bool submit(const LI_DS5_HAPTICS_IR_FRAME_V2& frame, bool& startedNative);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
