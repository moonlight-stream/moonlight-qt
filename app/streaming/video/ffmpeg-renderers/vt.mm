// Nasty hack to avoid conflict between AVFoundation and
// libavutil both defining AVMediaType
#define AVMediaType AVMediaType_FFmpeg
#include "vt.h"
#include "pacer/pacer.h"
#undef AVMediaType

#include <SDL_syswm.h>
#include <Limelight.h>
#include "streaming/session.h"
#include "streaming/streamutils.h"
#include "path.h"

#import <Cocoa/Cocoa.h>
#import <VideoToolbox/VideoToolbox.h>
#import <AVFoundation/AVFoundation.h>
#import <dispatch/dispatch.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

struct CscParams
{
    vector_float3 matrix[3];
    vector_float3 offsets;
};

static const CscParams k_CscParams_Bt601Lim = {
    // CSC Matrix
    {
        { 1.1644f, 0.0f, 1.5960f },
        { 1.1644f, -0.3917f, -0.8129f },
        { 1.1644f, 2.0172f, 0.0f }
    },

    // Offsets
    { 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};
static const CscParams k_CscParams_Bt601Full = {
    // CSC Matrix
    {
        { 1.0f, 0.0f, 1.4020f },
        { 1.0f, -0.3441f, -0.7141f },
        { 1.0f, 1.7720f, 0.0f },
    },

    // Offsets
    { 0.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};
static const CscParams k_CscParams_Bt709Lim = {
    // CSC Matrix
    {
        { 1.1644f, 0.0f, 1.7927f },
        { 1.1644f, -0.2132f, -0.5329f },
        { 1.1644f, 2.1124f, 0.0f },
    },

    // Offsets
    { 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};
static const CscParams k_CscParams_Bt709Full = {
    // CSC Matrix
    {
        { 1.0f, 0.0f, 1.5748f },
        { 1.0f, -0.1873f, -0.4681f },
        { 1.0f, 1.8556f, 0.0f },
    },

    // Offsets
    { 0.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};
static const CscParams k_CscParams_Bt2020Lim = {
    // CSC Matrix
    {
        { 1.1644f, 0.0f, 1.6781f },
        { 1.1644f, -0.1874f, -0.6505f },
        { 1.1644f, 2.1418f, 0.0f },
    },

    // Offsets
    { 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};
static const CscParams k_CscParams_Bt2020Full = {
    // CSC Matrix
    {
        { 1.0f, 0.0f, 1.4746f },
        { 1.0f, -0.1646f, -0.5714f },
        { 1.0f, 1.8814f, 0.0f },
    },

    // Offsets
    { 0.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};

struct Vertex
{
    vector_float4 position;
    vector_float2 texCoord;
};

@interface VTView : NSView
- (NSView *)hitTest:(NSPoint)point;
@end

@implementation VTView

- (NSView *)hitTest:(NSPoint)point {
    Q_UNUSED(point);
    return nil;
}

@end

class VTRenderer : public IFFmpegRenderer
{
public:
    VTRenderer()
        : m_Window(nullptr),
          m_HwContext(nullptr),
          m_MetalLayer(nullptr),
          m_TextureCache(nullptr),
          m_CscParamsBuffer(nullptr),
          m_VideoVertexBuffer(nullptr),
          m_PipelineState(nullptr),
          m_ShaderLibrary(nullptr),
          m_CommandQueue(nullptr),
          m_StreamView(nullptr),
          m_DisplayLink(nullptr),
          m_LastColorSpace(-1),
          m_LastFullRange(false),
          m_VsyncMutex(nullptr),
          m_VsyncPassed(nullptr)
    {
        SDL_zero(m_OverlayTextFields);
        for (int i = 0; i < Overlay::OverlayMax; i++) {
            m_OverlayUpdateBlocks[i] = dispatch_block_create(DISPATCH_BLOCK_DETACHED, ^{
                updateOverlayOnMainThread((Overlay::OverlayType)i);
            });
        }
    }

    virtual ~VTRenderer() override
    { @autoreleasepool {
        // We may have overlay update blocks enqueued for execution.
        // We must cancel those to avoid a UAF.
        for (int i = 0; i < Overlay::OverlayMax; i++) {
            dispatch_block_cancel(m_OverlayUpdateBlocks[i]);
            Block_release(m_OverlayUpdateBlocks[i]);
        }

        if (m_DisplayLink != nullptr) {
            CVDisplayLinkStop(m_DisplayLink);
            CVDisplayLinkRelease(m_DisplayLink);
        }

        if (m_VsyncPassed != nullptr) {
            SDL_DestroyCond(m_VsyncPassed);
        }

        if (m_VsyncMutex != nullptr) {
            SDL_DestroyMutex(m_VsyncMutex);
        }

        if (m_HwContext != nullptr) {
            av_buffer_unref(&m_HwContext);
        }

        for (int i = 0; i < Overlay::OverlayMax; i++) {
            if (m_OverlayTextFields[i] != nullptr) {
                [m_OverlayTextFields[i] removeFromSuperview];
                [m_OverlayTextFields[i] release];
            }
        }

        if (m_StreamView != nullptr) {
            [m_StreamView removeFromSuperview];
            [m_StreamView release];
        }

        if (m_CscParamsBuffer != nullptr) {
            [m_CscParamsBuffer release];
        }

        if (m_VideoVertexBuffer != nullptr) {
            [m_VideoVertexBuffer release];
        }

        if (m_PipelineState != nullptr) {
            [m_PipelineState release];
        }

        if (m_ShaderLibrary != nullptr) {
            [m_ShaderLibrary release];
        }

        if (m_CommandQueue != nullptr) {
            [m_CommandQueue release];
        }

        if (m_TextureCache != nullptr) {
            CFRelease(m_TextureCache);
        }

        // It appears to be necessary to run the event loop after destroying
        // the AVSampleBufferDisplayLayer to avoid issue #973.
        SDL_PumpEvents();
    }}

    static
    CVReturn
    displayLinkOutputCallback(
        CVDisplayLinkRef displayLink,
        const CVTimeStamp* /* now */,
        const CVTimeStamp* /* vsyncTime */,
        CVOptionFlags,
        CVOptionFlags*,
        void *displayLinkContext)
    {
        auto me = reinterpret_cast<VTRenderer*>(displayLinkContext);

        SDL_assert(displayLink == me->m_DisplayLink);

        SDL_LockMutex(me->m_VsyncMutex);
        SDL_CondSignal(me->m_VsyncPassed);
        SDL_UnlockMutex(me->m_VsyncMutex);

        return kCVReturnSuccess;
    }

    bool initializeVsyncCallback(SDL_SysWMinfo* info)
    {
        NSScreen* screen = [info->info.cocoa.window screen];
        CVReturn status;
        if (screen == nullptr) {
            // Window not visible on any display, so use a
            // CVDisplayLink that can work with all active displays.
            // When we become visible, we'll recreate ourselves
            // and associate with the new screen.
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "NSWindow is not visible on any display");
            status = CVDisplayLinkCreateWithActiveCGDisplays(&m_DisplayLink);
        }
        else {
            CGDirectDisplayID displayId = [[screen deviceDescription][@"NSScreenNumber"] unsignedIntValue];
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "NSWindow on display: %x",
                        displayId);
            status = CVDisplayLinkCreateWithCGDisplay(displayId, &m_DisplayLink);
        }
        if (status != kCVReturnSuccess) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create CVDisplayLink: %d",
                         status);
            return false;
        }

        status = CVDisplayLinkSetOutputCallback(m_DisplayLink, displayLinkOutputCallback, this);
        if (status != kCVReturnSuccess) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "CVDisplayLinkSetOutputCallback() failed: %d",
                         status);
            return false;
        }

        // The CVDisplayLink callback uses these, so we must initialize them before
        // starting the callbacks.
        m_VsyncMutex = SDL_CreateMutex();
        m_VsyncPassed = SDL_CreateCond();

        status = CVDisplayLinkStart(m_DisplayLink);
        if (status != kCVReturnSuccess) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "CVDisplayLinkStart() failed: %d",
                         status);
            return false;
        }

        return true;
    }

    virtual void waitToRender() override
    {
        if (m_DisplayLink != nullptr) {
            // Vsync is enabled, so wait for a swap before returning
            SDL_LockMutex(m_VsyncMutex);
            if (SDL_CondWaitTimeout(m_VsyncPassed, m_VsyncMutex, 100) == SDL_MUTEX_TIMEDOUT) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "V-sync wait timed out after 100 ms");
            }
            SDL_UnlockMutex(m_VsyncMutex);
        }
    }

    bool updateVideoRegionSizeForFrame(AVFrame* frame)
    {
        // TODO: When we support seamless resizing, implement this properly!
        if (m_VideoVertexBuffer) {
            return true;
        }

        int drawableWidth, drawableHeight;
        SDL_Metal_GetDrawableSize(m_Window, &drawableWidth, &drawableHeight);

        // Determine the correct scaled size for the video region
        SDL_Rect src, dst;
        src.x = src.y = 0;
        src.w = frame->width;
        src.h = frame->height;
        dst.x = dst.y = 0;
        dst.w = drawableWidth;
        dst.h = drawableHeight;
        StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

        // Convert screen space to normalized device coordinates
        SDL_FRect renderRect;
        StreamUtils::screenSpaceToNormalizedDeviceCoords(&dst, &renderRect, drawableWidth, drawableHeight);

        Vertex verts[] =
        {
            { { renderRect.x, renderRect.y, 0.0f, 1.0f }, { 0.0f, 1.0f } },
            { { renderRect.x, renderRect.y+renderRect.h, 0.0f, 1.0f }, { 0.0f, 0} },
            { { renderRect.x+renderRect.w, renderRect.y, 0.0f, 1.0f }, { 1.0f, 1.0f} },
            { { renderRect.x+renderRect.w, renderRect.y+renderRect.h, 0.0f, 1.0f }, { 1.0f, 0} },
        };

        [m_VideoVertexBuffer release];
        m_VideoVertexBuffer = [m_MetalLayer.device newBufferWithBytes:verts length:sizeof(verts) options:MTLResourceStorageModePrivate];
        if (!m_VideoVertexBuffer) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create video vertex buffer");
            return false;
        }

        return true;
    }

    bool updateColorSpaceForFrame(AVFrame* frame)
    {
        int colorspace = getFrameColorspace(frame);
        bool fullRange = isFrameFullRange(frame);
        if (colorspace != m_LastColorSpace || fullRange != m_LastFullRange) {
            CGColorSpaceRef newColorSpace;
            void* paramBuffer;

            switch (colorspace) {
            case COLORSPACE_REC_709:
                m_MetalLayer.colorspace = newColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceITUR_709);
                m_MetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
                paramBuffer = (void*)(fullRange ? &k_CscParams_Bt709Full : &k_CscParams_Bt709Lim);
                break;
            case COLORSPACE_REC_2020:
                // https://developer.apple.com/documentation/metal/hdr_content/using_color_spaces_to_display_hdr_content
                if (frame->color_trc == AVCOL_TRC_SMPTE2084) {
                    if (@available(macOS 11.0, *)) {
                        m_MetalLayer.colorspace = newColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceITUR_2100_PQ);
                    }
                    else {
                        m_MetalLayer.colorspace = newColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020);
                    }
                    m_MetalLayer.pixelFormat = MTLPixelFormatBGR10A2Unorm;
                }
                else {
                    m_MetalLayer.colorspace = newColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020);
                    m_MetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
                }
                paramBuffer = (void*)(fullRange ? &k_CscParams_Bt2020Full : &k_CscParams_Bt2020Lim);
                break;
            default:
            case COLORSPACE_REC_601:
                m_MetalLayer.colorspace = newColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
                m_MetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
                paramBuffer = (void*)(fullRange ? &k_CscParams_Bt601Full : &k_CscParams_Bt601Lim);
                break;
            }

            // The CAMetalLayer retains the CGColorSpace
            CGColorSpaceRelease(newColorSpace);

            // Create the new colorspace parameter buffer for our fragment shader
            [m_CscParamsBuffer release];
            m_CscParamsBuffer = [m_MetalLayer.device newBufferWithBytes:paramBuffer length:sizeof(CscParams) options:MTLResourceStorageModePrivate];
            if (!m_CscParamsBuffer) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to create CSC parameters buffer");
                return false;
            }

            MTLRenderPipelineDescriptor *pipelineDesc = [[MTLRenderPipelineDescriptor new] autorelease];
            pipelineDesc.vertexFunction = [[m_ShaderLibrary newFunctionWithName:@"vs_draw"] autorelease];
            pipelineDesc.fragmentFunction = [[m_ShaderLibrary newFunctionWithName:@"ps_draw_biplanar"] autorelease];
            pipelineDesc.colorAttachments[0].pixelFormat = m_MetalLayer.pixelFormat;
            m_PipelineState = [m_MetalLayer.device newRenderPipelineStateWithDescriptor:pipelineDesc error:nullptr];
            if (!m_PipelineState) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to create render pipeline state");
                return false;
            }

            m_LastColorSpace = colorspace;
            m_LastFullRange = fullRange;
        }

        return true;
    }

    // Caller frees frame after we return
    virtual void renderFrame(AVFrame* frame) override
    { @autoreleasepool {
        CVPixelBufferRef pixBuf = reinterpret_cast<CVPixelBufferRef>(frame->data[3]);

        if (m_MetalLayer.preferredDevice != nullptr && m_MetalLayer.preferredDevice != m_MetalLayer.device) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Resetting renderer after preferred device changed");

            // Trigger the main thread to recreate the decoder
            SDL_Event event;
            event.type = SDL_RENDER_DEVICE_RESET;
            SDL_PushEvent(&event);
            return;
        }

        // Handle changes to the frame's colorspace from last time we rendered
        if (!updateColorSpaceForFrame(frame)) {
            // Trigger the main thread to recreate the decoder
            SDL_Event event;
            event.type = SDL_RENDER_DEVICE_RESET;
            SDL_PushEvent(&event);
            return;
        }

        // Handle changes to the video size or drawable size
        if (!updateVideoRegionSizeForFrame(frame)) {
            // Trigger the main thread to recreate the decoder
            SDL_Event event;
            event.type = SDL_RENDER_DEVICE_RESET;
            SDL_PushEvent(&event);
            return;
        }

        auto nextDrawable = [m_MetalLayer nextDrawable];

        // Create Metal textures for the planes of the CVPixelBuffer
        std::array<CVMetalTextureRef, 2> textures;
        for (size_t i = 0; i < textures.size(); i++) {
            MTLPixelFormat fmt;

            switch (CVPixelBufferGetPixelFormatType(pixBuf)) {
            case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
            case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
                fmt = (i == 0) ? MTLPixelFormatR8Unorm : MTLPixelFormatRG8Unorm;
                break;

            case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
            case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
                fmt = (i == 0) ? MTLPixelFormatR16Unorm : MTLPixelFormatRG16Unorm;
                break;

            default:
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Unknown pixel format: %x",
                             CVPixelBufferGetPixelFormatType(pixBuf));
                return;
            }

            CVReturn err = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, m_TextureCache, pixBuf, nullptr, fmt,
                                                                     CVPixelBufferGetWidthOfPlane(pixBuf, i),
                                                                     CVPixelBufferGetHeightOfPlane(pixBuf, i),
                                                                     i,
                                                                     &textures[i]);
            if (err != kCVReturnSuccess) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "CVMetalTextureCacheCreateTextureFromImage() failed: %d",
                             err);
                return;
            }
        }

        // Prepare a render pass to render into the next drawable
        auto renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        renderPassDescriptor.colorAttachments[0].texture = nextDrawable.texture;
        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
        renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

        // Bind textures and buffers then draw the video region
        auto commandBuffer = [m_CommandQueue commandBuffer];
        auto renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder setRenderPipelineState:m_PipelineState];
        for (size_t i = 0; i < textures.size(); i++) {
            [renderEncoder setFragmentTexture:CVMetalTextureGetTexture(textures[i]) atIndex:i];
        }
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
          // Free textures after completion of rendering per CVMetalTextureCache requirements
          for (const CVMetalTextureRef &tex : textures) {
              CFRelease(tex);
          }
        }];
        [renderEncoder setFragmentBuffer:m_CscParamsBuffer offset:0 atIndex:0];
        [renderEncoder setVertexBuffer:m_VideoVertexBuffer offset:0 atIndex:0];
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        [renderEncoder endEncoding];

        // Flip to the newly rendered buffer
        [commandBuffer presentDrawable: nextDrawable];
        [commandBuffer commit];
    }}

    bool checkDecoderCapabilities(PDECODER_PARAMETERS params) {
        if (params->videoFormat & VIDEO_FORMAT_MASK_H264) {
            if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_H264)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "No HW accelerated H.264 decode via VT");
                return false;
            }
        }
        else if (params->videoFormat & VIDEO_FORMAT_MASK_H265) {
            if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "No HW accelerated HEVC decode via VT");
                return false;
            }

            // HEVC Main10 requires more extensive checks because there's no
            // simple API to check for Main10 hardware decoding, and if we don't
            // have it, we'll silently get software decoding with horrible performance.
            if (params->videoFormat == VIDEO_FORMAT_H265_MAIN10) {
                id<MTLDevice> device = MTLCreateSystemDefaultDevice();
                if (device == nullptr) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Unable to get default Metal device");
                    return false;
                }

                // Exclude all GPUs earlier than macOSGPUFamily2
                // https://developer.apple.com/documentation/metal/mtlfeatureset/mtlfeatureset_macos_gpufamily2_v1
                if ([device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily2_v1]) {
                    if ([device.name containsString:@"Intel"]) {
                        // 500-series Intel GPUs are Skylake and don't support Main10 hardware decoding
                        if ([device.name containsString:@" 5"]) {
                            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                        "No HEVC Main10 support on Skylake iGPU");
                            [device release];
                            return false;
                        }
                    }
                    else if ([device.name containsString:@"AMD"]) {
                        // FirePro D, M200, and M300 series GPUs don't support Main10 hardware decoding
                        if ([device.name containsString:@"FirePro D"] ||
                                [device.name containsString:@" M2"] ||
                                [device.name containsString:@" M3"]) {
                            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                        "No HEVC Main10 support on AMD GPUs until Polaris");
                            [device release];
                            return false;
                        }
                    }
                }
                else {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "No HEVC Main10 support on macOS GPUFamily1 GPUs");
                    [device release];
                    return false;
                }

                [device release];
            }
        }
        else if (params->videoFormat & VIDEO_FORMAT_MASK_AV1) {
        #if __MAC_OS_X_VERSION_MAX_ALLOWED >= 130000
            if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_AV1)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "No HW accelerated AV1 decode via VT");
                return false;
            }

            // 10-bit is part of the Main profile for AV1, so it will always
            // be present on hardware that supports 8-bit.
        #else
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "AV1 requires building with Xcode 14 or later");
            return false;
        #endif
        }

        return true;
    }

    virtual bool initialize(PDECODER_PARAMETERS params) override
    { @autoreleasepool {
        int err;

        m_Window = params->window;

        if (!checkDecoderCapabilities(params)) {
            return false;
        }

        err = av_hwdevice_ctx_create(&m_HwContext,
                                     AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
                                     nullptr,
                                     nullptr,
                                     0);
        if (err < 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "av_hwdevice_ctx_create() failed for VT decoder: %d",
                        err);
            return false;
        }

        SDL_SysWMinfo info;

        SDL_VERSION(&info.version);

        if (!SDL_GetWindowWMInfo(params->window, &info)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "SDL_GetWindowWMInfo() failed: %s",
                        SDL_GetError());
            return false;
        }

        SDL_assert(info.subsystem == SDL_SYSWM_COCOA);

        // SDL adds its own content view to listen for events.
        // We need to add a subview for our display layer.
        NSView* contentView = info.info.cocoa.window.contentView;
        m_StreamView = [[VTView alloc] initWithFrame:contentView.bounds];

        // Associate a CAMetalLayer to our view
        m_StreamView.layer = m_MetalLayer = [CAMetalLayer layer];
        m_StreamView.wantsLayer = YES;

        // Choose a device
        m_MetalLayer.device = m_MetalLayer.preferredDevice;
        if (!m_MetalLayer.device) {
            m_MetalLayer.device = [MTLCreateSystemDefaultDevice() autorelease];
            if (!m_MetalLayer.device) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "No Metal device found!");
                return false;
            }
        }

        // Configure the Metal layer for rendering
        m_MetalLayer.wantsExtendedDynamicRangeContent = !!(params->videoFormat & VIDEO_FORMAT_MASK_10BIT);
        m_MetalLayer.displaySyncEnabled = params->enableVsync;
        m_MetalLayer.maximumDrawableCount = 2; // Double buffering

        [contentView addSubview: m_StreamView];

        // Create the Metal texture cache for our CVPixelBuffers
        CFStringRef keys[1] = { kCVMetalTextureUsage };
        NSUInteger values[1] = { MTLTextureUsageShaderRead };
        auto cacheAttributes = CFDictionaryCreate(kCFAllocatorDefault, (const void**)keys, (const void**)values, 1, nullptr, nullptr);
        err = CVMetalTextureCacheCreate(kCFAllocatorDefault, cacheAttributes, m_MetalLayer.device, nullptr, &m_TextureCache);
        CFRelease(cacheAttributes);

        if (err != kCVReturnSuccess) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "CVMetalTextureCacheCreate() failed: %d",
                         err);
            return false;
        }

        // Compile our shaders
        QString shaderSource = QString::fromUtf8(Path::readDataFile("vt_renderer.metal"));
        m_ShaderLibrary = [m_MetalLayer.device newLibraryWithSource:shaderSource.toNSString() options:nullptr error:nullptr];
        if (!m_ShaderLibrary) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to compile shaders");
            return false;
        }

        // Create a command queue for submission
        m_CommandQueue = [m_MetalLayer.device newCommandQueue];

        if (params->enableFramePacing) {
            if (!initializeVsyncCallback(&info)) {
                return false;
            }
        }

        return true;
    }}

    void updateOverlayOnMainThread(Overlay::OverlayType type)
    { @autoreleasepool {
        // Lazy initialization for the overlay
        if (m_OverlayTextFields[type] == nullptr) {
            m_OverlayTextFields[type] = [[NSTextField alloc] initWithFrame:m_StreamView.bounds];
            [m_OverlayTextFields[type] setBezeled:NO];
            [m_OverlayTextFields[type] setDrawsBackground:NO];
            [m_OverlayTextFields[type] setEditable:NO];
            [m_OverlayTextFields[type] setSelectable:NO];

            switch (type) {
            case Overlay::OverlayDebug:
                [m_OverlayTextFields[type] setAlignment:NSTextAlignmentLeft];
                break;
            case Overlay::OverlayStatusUpdate:
                [m_OverlayTextFields[type] setAlignment:NSTextAlignmentRight];
                break;
            default:
                break;
            }

            SDL_Color color = Session::get()->getOverlayManager().getOverlayColor(type);
            [m_OverlayTextFields[type] setTextColor:[NSColor colorWithSRGBRed:color.r / 255.0 green:color.g / 255.0 blue:color.b / 255.0 alpha:color.a / 255.0]];
            [m_OverlayTextFields[type] setFont:[NSFont messageFontOfSize:Session::get()->getOverlayManager().getOverlayFontSize(type)]];

            [m_StreamView addSubview: m_OverlayTextFields[type]];
        }

        // Update text contents
        [m_OverlayTextFields[type] setStringValue: [NSString stringWithUTF8String:Session::get()->getOverlayManager().getOverlayText(type)]];

        // Unhide if it's enabled
        [m_OverlayTextFields[type] setHidden: !Session::get()->getOverlayManager().isOverlayEnabled(type)];
    }}

    virtual void notifyOverlayUpdated(Overlay::OverlayType type) override
    {
        // We must do the actual UI updates on the main thread, so queue an
        // async callback on the main thread via GCD to do the UI update.
        dispatch_async(dispatch_get_main_queue(), m_OverlayUpdateBlocks[type]);
    }

    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary**) override
    {
        context->hw_device_ctx = av_buffer_ref(m_HwContext);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Using VideoToolbox Metal renderer");

        return true;
    }

    virtual bool needsTestFrame() override
    {
        // We used to trust VT to tell us whether decode will work, but
        // there are cases where it can lie because the hardware technically
        // can decode the format but VT is unserviceable for some other reason.
        // Decoding the test frame will tell us for sure whether it will work.
        return true;
    }

    int getDecoderColorspace() override
    {
        // macOS seems to handle Rec 601 best
        return COLORSPACE_REC_601;
    }

    int getDecoderCapabilities() override
    {
        return CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC |
               CAPABILITY_REFERENCE_FRAME_INVALIDATION_AV1;
    }

    int getRendererAttributes() override
    {
        // AVSampleBufferDisplayLayer supports HDR output
        return RENDERER_ATTRIBUTE_HDR_SUPPORT;
    }

private:
    SDL_Window* m_Window;
    AVBufferRef* m_HwContext;
    CAMetalLayer* m_MetalLayer;
    CVMetalTextureCacheRef m_TextureCache;
    id<MTLBuffer> m_CscParamsBuffer;
    id<MTLBuffer> m_VideoVertexBuffer;
    id<MTLRenderPipelineState> m_PipelineState;
    id<MTLLibrary> m_ShaderLibrary;
    id<MTLCommandQueue> m_CommandQueue;
    VTView* m_StreamView;
    dispatch_block_t m_OverlayUpdateBlocks[Overlay::OverlayMax];
    NSTextField* m_OverlayTextFields[Overlay::OverlayMax];
    CVDisplayLinkRef m_DisplayLink;
    int m_LastColorSpace;
    bool m_LastFullRange;
    SDL_mutex* m_VsyncMutex;
    SDL_cond* m_VsyncPassed;
};

IFFmpegRenderer* VTRendererFactory::createRenderer() {
    return new VTRenderer();
}
