// Nasty hack to avoid conflict between AVFoundation and
// libavutil both defining AVMediaType
#define AVMediaType AVMediaType_FFmpeg
#include "vt.h"
#undef AVMediaType

#import <Cocoa/Cocoa.h>
#import <VideoToolbox/VideoToolbox.h>
#import <AVFoundation/AVFoundation.h>
#import <Metal/Metal.h>

bool VTBaseRenderer::checkDecoderCapabilities(id<MTLDevice> device, PDECODER_PARAMETERS params) {
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
            // Exclude all GPUs earlier than macOSGPUFamily2
            // https://developer.apple.com/documentation/metal/mtlfeatureset/mtlfeatureset_macos_gpufamily2_v1
            if ([device supportsFamily:MTLGPUFamilyMac2]) {
                if ([device.name containsString:@"Intel"]) {
                    // 500-series Intel GPUs are Skylake and don't support Main10 hardware decoding
                    if ([device.name containsString:@" 5"]) {
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                    "No HEVC Main10 support on Skylake iGPU");
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
                        return false;
                    }
                }
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "No HEVC Main10 support on macOS GPUFamily1 GPUs");
                return false;
            }
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
