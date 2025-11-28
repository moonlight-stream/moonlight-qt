/**
 * @file app/streaming/video/ffmpeg-renderers/vt.h
 * @brief VideoToolbox renderer implementation for macOS/iOS.
 */

#pragma once

#include "renderer.h"

#ifdef __OBJC__
#import <Metal/Metal.h>
/**
 * @brief Base VideoToolbox renderer using Metal for macOS/iOS.
 */
class VTBaseRenderer : public IFFmpegRenderer {
public:
    VTBaseRenderer(IFFmpegRenderer::RendererType type);
    virtual ~VTBaseRenderer();
    bool checkDecoderCapabilities(id<MTLDevice> device, PDECODER_PARAMETERS params);
    void setHdrMode(bool enabled) override;

protected:
    bool isAppleSilicon();

    bool m_HdrMetadataChanged; // Manual reset
    CFDataRef m_MasteringDisplayColorVolume;
    CFDataRef m_ContentLightLevelInfo;
};
#endif

// A factory is required to avoid pulling in
// incompatible Objective-C headers.

/**
 * @brief Factory for creating VideoToolbox Metal renderers.
 */
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
