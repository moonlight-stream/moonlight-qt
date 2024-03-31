#pragma once

#include "renderer.h"

// A factory is required to avoid pulling in
// incompatible Objective-C headers.

class VTMetalRendererFactory {
public:
    static
    IFFmpegRenderer* createRenderer();
};

class VTRendererFactory {
public:
    static
    IFFmpegRenderer* createRenderer();
};
