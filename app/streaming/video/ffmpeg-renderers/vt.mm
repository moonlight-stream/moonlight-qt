// Nasty hack to avoid conflict between AVFoundation and
// libavutil both defining AVMediaType
#define AVMediaType AVMediaType_FFmpeg
#include "vt.h"
#include "pacer/pacer.h"
#undef AVMediaType

#include <SDL_syswm.h>
#include <Limelight.h>
#include <streaming/session.h>

#include <mach/mach_time.h>
#include <mach/machine.h>
#include <sys/sysctl.h>
#import <Cocoa/Cocoa.h>
#import <VideoToolbox/VideoToolbox.h>
#import <AVFoundation/AVFoundation.h>
#import <dispatch/dispatch.h>
#import <Metal/Metal.h>

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
        : m_HwContext(nullptr),
          m_DisplayLayer(nullptr),
          m_FormatDesc(nullptr),
          m_ContentLightLevelInfo(nullptr),
          m_MasteringDisplayColorVolume(nullptr),
          m_StreamView(nullptr),
          m_DisplayLink(nullptr),
          m_LastColorSpace(-1),
          m_ColorSpace(nullptr),
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
    {
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

        if (m_FormatDesc != nullptr) {
            CFRelease(m_FormatDesc);
        }

        if (m_ColorSpace != nullptr) {
            CGColorSpaceRelease(m_ColorSpace);
        }

        if (m_MasteringDisplayColorVolume != nullptr) {
            CFRelease(m_MasteringDisplayColorVolume);
        }

        if (m_ContentLightLevelInfo != nullptr) {
            CFRelease(m_ContentLightLevelInfo);
        }

        for (int i = 0; i < Overlay::OverlayMax; i++) {
            if (m_OverlayTextFields[i] != nullptr) {
                [m_OverlayTextFields[i] removeFromSuperview];
            }
        }

        if (m_StreamView != nullptr) {
            [m_StreamView removeFromSuperview];
        }
    }

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

    virtual void setHdrMode(bool enabled) override
    {
        // Free existing HDR metadata
        if (m_MasteringDisplayColorVolume != nullptr) {
            CFRelease(m_MasteringDisplayColorVolume);
            m_MasteringDisplayColorVolume = nullptr;
        }
        if (m_ContentLightLevelInfo != nullptr) {
            CFRelease(m_ContentLightLevelInfo);
            m_ContentLightLevelInfo = nullptr;
        }

        // Store new HDR metadata if available
        SS_HDR_METADATA hdrMetadata;
        if (enabled && LiGetHdrMetadata(&hdrMetadata)) {
            if (hdrMetadata.displayPrimaries[0].x != 0 && hdrMetadata.maxDisplayLuminance != 0) {
                // This data is all in big-endian
                struct {
                  vector_ushort2 primaries[3];
                  vector_ushort2 white_point;
                  uint32_t luminance_max;
                  uint32_t luminance_min;
                } __attribute__((packed, aligned(4))) mdcv;

                // mdcv is in GBR order while SS_HDR_METADATA is in RGB order
                mdcv.primaries[0].x = __builtin_bswap16(hdrMetadata.displayPrimaries[1].x);
                mdcv.primaries[0].y = __builtin_bswap16(hdrMetadata.displayPrimaries[1].y);
                mdcv.primaries[1].x = __builtin_bswap16(hdrMetadata.displayPrimaries[2].x);
                mdcv.primaries[1].y = __builtin_bswap16(hdrMetadata.displayPrimaries[2].y);
                mdcv.primaries[2].x = __builtin_bswap16(hdrMetadata.displayPrimaries[0].x);
                mdcv.primaries[2].y = __builtin_bswap16(hdrMetadata.displayPrimaries[0].y);

                mdcv.white_point.x = __builtin_bswap16(hdrMetadata.whitePoint.x);
                mdcv.white_point.y = __builtin_bswap16(hdrMetadata.whitePoint.y);

                // These luminance values are in 10000ths of a nit
                mdcv.luminance_max = __builtin_bswap32((uint32_t)hdrMetadata.maxDisplayLuminance * 10000);
                mdcv.luminance_min = __builtin_bswap32(hdrMetadata.minDisplayLuminance);

                m_MasteringDisplayColorVolume = CFDataCreate(nullptr, (const UInt8*)&mdcv, sizeof(mdcv));
            }

            if (hdrMetadata.maxContentLightLevel != 0 && hdrMetadata.maxFrameAverageLightLevel != 0) {
                // This data is all in big-endian
                struct {
                    uint16_t max_content_light_level;
                    uint16_t max_frame_average_light_level;
                } __attribute__((packed, aligned(2))) cll;

                cll.max_content_light_level = __builtin_bswap16(hdrMetadata.maxContentLightLevel);
                cll.max_frame_average_light_level = __builtin_bswap16(hdrMetadata.maxFrameAverageLightLevel);

                m_ContentLightLevelInfo = CFDataCreate(nullptr, (const UInt8*)&cll, sizeof(cll));
            }
        }
    }

    // Caller frees frame after we return
    virtual void renderFrame(AVFrame* frame) override
    {
        OSStatus status;
        CVPixelBufferRef pixBuf = reinterpret_cast<CVPixelBufferRef>(frame->data[3]);

        if (m_DisplayLayer.status == AVQueuedSampleBufferRenderingStatusFailed) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Resetting failed AVSampleBufferDisplay layer");

            // Trigger the main thread to recreate the decoder
            SDL_Event event;
            event.type = SDL_RENDER_TARGETS_RESET;
            SDL_PushEvent(&event);
            return;
        }

        // FFmpeg 5.0+ sets the CVPixelBuffer attachments properly now, so we don't have to
        // fix them up ourselves (except CGColorSpace and PAR attachments).

        // The VideoToolbox decoder attaches pixel aspect ratio information to the CVPixelBuffer
        // which will rescale the video stream in accordance with the host display resolution
        // to preserve the original aspect ratio of the host desktop. This behavior currently
        // differs from the behavior of all other Moonlight Qt renderers, so we will strip
        // these attachments for consistent behavior.
        CVBufferRemoveAttachment(pixBuf, kCVImageBufferPixelAspectRatioKey);

        // Reset m_ColorSpace if the colorspace changes. This can happen when
        // a game enters HDR mode (Rec 601 -> Rec 2020).
        int colorspace = getFrameColorspace(frame);
        if (colorspace != m_LastColorSpace) {
            if (m_ColorSpace != nullptr) {
                CGColorSpaceRelease(m_ColorSpace);
                m_ColorSpace = nullptr;
            }

            switch (colorspace) {
            case COLORSPACE_REC_709:
                m_ColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceITUR_709);
                break;
            case COLORSPACE_REC_2020:
                m_ColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020);
                break;
            case COLORSPACE_REC_601:
                m_ColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
                break;
            }

            m_LastColorSpace = colorspace;
        }

        if (m_ColorSpace != nullptr) {
            CVBufferSetAttachment(pixBuf, kCVImageBufferCGColorSpaceKey, m_ColorSpace, kCVAttachmentMode_ShouldPropagate);
        }

        // Attach HDR metadata if it has been provided by the host
        if (m_MasteringDisplayColorVolume != nullptr) {
            CVBufferSetAttachment(pixBuf, kCVImageBufferMasteringDisplayColorVolumeKey, m_MasteringDisplayColorVolume, kCVAttachmentMode_ShouldPropagate);
        }
        if (m_ContentLightLevelInfo != nullptr) {
            CVBufferSetAttachment(pixBuf, kCVImageBufferContentLightLevelInfoKey, m_ContentLightLevelInfo, kCVAttachmentMode_ShouldPropagate);
        }

        // If the format has changed or doesn't exist yet, construct it with the
        // pixel buffer data
        if (!m_FormatDesc || !CMVideoFormatDescriptionMatchesImageBuffer(m_FormatDesc, pixBuf)) {
            if (m_FormatDesc != nullptr) {
                CFRelease(m_FormatDesc);
            }
            status = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault,
                                                                  pixBuf, &m_FormatDesc);
            if (status != noErr) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "CMVideoFormatDescriptionCreateForImageBuffer() failed: %d",
                             status);
                return;
            }
        }

        // Queue this sample for the next v-sync
        CMSampleTimingInfo timingInfo = {
            .duration = kCMTimeInvalid,
            .presentationTimeStamp = CMClockMakeHostTimeFromSystemUnits(mach_absolute_time()),
            .decodeTimeStamp = kCMTimeInvalid,
        };

        CMSampleBufferRef sampleBuffer;
        status = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault,
                                                          pixBuf,
                                                          m_FormatDesc,
                                                          &timingInfo,
                                                          &sampleBuffer);
        if (status != noErr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "CMSampleBufferCreateReadyWithImageBuffer() failed: %d",
                         status);
            return;
        }

        [m_DisplayLayer enqueueSampleBuffer:sampleBuffer];

        CFRelease(sampleBuffer);
    }

    virtual bool initialize(PDECODER_PARAMETERS params) override
    {
        int err;

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

        m_DisplayLayer = [[AVSampleBufferDisplayLayer alloc] init];
        m_DisplayLayer.bounds = m_StreamView.bounds;
        m_DisplayLayer.position = CGPointMake(CGRectGetMidX(m_StreamView.bounds), CGRectGetMidY(m_StreamView.bounds));
        m_DisplayLayer.videoGravity = AVLayerVideoGravityResizeAspect;
        m_DisplayLayer.opaque = YES;

        // This workaround prevents the image from going through processing that causes some
        // color artifacts in some cases. HDR seems to be okay without this, so we'll exclude
        // it out of caution. The artifacts seem to be far more significant on M1 Macs and
        // the workaround can cause performance regressions on Intel Macs, so only use this
        // on Apple silicon.
        //
        // https://github.com/moonlight-stream/moonlight-qt/issues/493
        // https://github.com/moonlight-stream/moonlight-qt/issues/722
        if (!(params->videoFormat & VIDEO_FORMAT_MASK_10BIT)) {
            int err;
            uint32_t cpuType;
            size_t size = sizeof(cpuType);

            // Apple Silicon Macs have CPU_ARCH_ABI64 set, so we need to mask that off.
            // For some reason, 64-bit Intel Macs don't seem to have CPU_ARCH_ABI64 set.
            err = sysctlbyname("hw.cputype", &cpuType, &size, NULL, 0);
            if (err == 0 && (cpuType & ~CPU_ARCH_MASK) == CPU_TYPE_ARM) {
                if (info.info.cocoa.window.screen != nullptr) {
                    m_DisplayLayer.shouldRasterize = YES;
                    m_DisplayLayer.rasterizationScale = info.info.cocoa.window.screen.backingScaleFactor;
                }
                else {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Unable to rasterize layer due to missing NSScreen");
                    SDL_assert(false);
                }
            }
            else if (err != 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "sysctlbyname(hw.cputype) failed: %d", err);
            }
        }

        // Create a layer-hosted view by setting the layer before wantsLayer
        // This avoids us having to add our AVSampleBufferDisplayLayer as a
        // sublayer of a layer-backed view which leaves a useless layer in
        // the middle.
        m_StreamView.layer = m_DisplayLayer;
        m_StreamView.wantsLayer = YES;

        [contentView addSubview: m_StreamView];

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

        if (params->enableFramePacing) {
            if (!initializeVsyncCallback(&info)) {
                return false;
            }
        }

        return true;
    }

    void updateOverlayOnMainThread(Overlay::OverlayType type)
    {
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
    }

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
                    "Using VideoToolbox accelerated renderer");

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
    AVBufferRef* m_HwContext;
    AVSampleBufferDisplayLayer* m_DisplayLayer;
    CMVideoFormatDescriptionRef m_FormatDesc;
    CFDataRef m_ContentLightLevelInfo;
    CFDataRef m_MasteringDisplayColorVolume;
    NSView* m_StreamView;
    dispatch_block_t m_OverlayUpdateBlocks[Overlay::OverlayMax];
    NSTextField* m_OverlayTextFields[Overlay::OverlayMax];
    CVDisplayLinkRef m_DisplayLink;
    int m_LastColorSpace;
    CGColorSpaceRef m_ColorSpace;
    SDL_mutex* m_VsyncMutex;
    SDL_cond* m_VsyncPassed;
};

IFFmpegRenderer* VTRendererFactory::createRenderer() {
    return new VTRenderer();
}
