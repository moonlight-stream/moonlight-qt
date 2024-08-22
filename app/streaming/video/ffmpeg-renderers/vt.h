#pragma once

#include "renderer.h"

#ifdef __OBJC__
#import <Metal/Metal.h>
class VTBaseRenderer : public IFFmpegRenderer {
public:
    bool checkDecoderCapabilities(id<MTLDevice> device, PDECODER_PARAMETERS params);
};
#endif

// A factory is required to avoid pulling in
// incompatible Objective-C headers.

class VTMetalRendererFactory {
public:
    static
    IFFmpegRenderer* createRenderer(bool hwAccel);
};

class VTRendererFactory {
public:
    static
    IFFmpegRenderer* createRenderer();
};
