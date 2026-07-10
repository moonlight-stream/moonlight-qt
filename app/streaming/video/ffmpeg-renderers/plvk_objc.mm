#include "plvk.h"

#define PL_LIBAV_IMPLEMENTATION 0
#include <libplacebo/utils/libav.h>

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include <CoreVideo/CVPixelBuffer.h>
#include <CoreVideo/CVMetalTexture.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

static id<MTLDevice> getVulkanMetalDevice(pl_vulkan vulkan)
{
    VkExportMetalDeviceInfoEXT metalDeviceInfo = {};
    metalDeviceInfo.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT;

    VkExportMetalObjectsInfoEXT exportInfo = {};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT;
    exportInfo.pNext = &metalDeviceInfo;

    auto vkExportMetalObjectsEXT = (PFN_vkExportMetalObjectsEXT)(vulkan->get_proc_addr(vulkan->instance, "vkExportMetalObjectsEXT"));
    if (vkExportMetalObjectsEXT) {
        vkExportMetalObjectsEXT(vulkan->device, &exportInfo);
        return metalDeviceInfo.mtlDevice;
    }

    return nullptr;
}

MetalVulkanTextureFactory::MetalVulkanTextureFactory(pl_vulkan vulkan) :
    m_Vulkan(vulkan)
{
    id<MTLDevice> device = getVulkanMetalDevice(vulkan);
    if (!device) {
        return;
    }

    CFStringRef keys[1] = { kCVMetalTextureUsage };
    NSUInteger values[1] = { MTLTextureUsageShaderRead };
    auto cacheAttributes = CFDictionaryCreate(kCFAllocatorDefault, (const void**)keys, (const void**)values, 1, nullptr, nullptr);
    int err = CVMetalTextureCacheCreate(kCFAllocatorDefault, cacheAttributes, device, nullptr, (CVMetalTextureCacheRef*)&m_TextureCache);
    CFRelease(cacheAttributes);

    if (err != kCVReturnSuccess) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CVMetalTextureCacheCreate() failed: %d",
                     err);
    }
}

MetalVulkanTextureFactory::~MetalVulkanTextureFactory()
{
    if (m_TextureCache) {
        CFRelease((CVMetalTextureCacheRef)m_TextureCache);
    }
}

bool MetalVulkanTextureFactory::mapVideoToolboxToPlacebo(const AVFrame *frame, pl_frame* mappedFrame)
{
    SDL_assert(frame->format == AV_PIX_FMT_VIDEOTOOLBOX);

    if (!m_TextureCache) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to map VT frames without Metal texture cache");
        return false;
    }

    CVPixelBufferRef pixBuf = (CVPixelBufferRef)(frame->data[3]);
    size_t planes = CVPixelBufferGetPlaneCount(pixBuf);

    // Map the AVFrame metadata into the pl_frame
    pl_frame_from_avframe(mappedFrame, frame);

    // Create Metal textures for the planes of the CVPixelBuffer
    mappedFrame->num_planes = 0;
    for (size_t i = 0; i < planes; i++) {
        MTLPixelFormat fmt;

        pl_tex_params texParams = {};
        texParams.w = CVPixelBufferGetWidthOfPlane(pixBuf, i);
        texParams.h = CVPixelBufferGetHeightOfPlane(pixBuf, i);
        texParams.sampleable = true;

        switch (CVPixelBufferGetPixelFormatType(pixBuf)) {
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange:
        case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
        case kCVPixelFormatType_444YpCbCr8BiPlanarFullRange:
            fmt = (i == 0) ? MTLPixelFormatR8Unorm : MTLPixelFormatRG8Unorm;
            texParams.format = pl_find_named_fmt(m_Vulkan->gpu, (i == 0) ? "r8" : "rg8");
            break;

        case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
        case kCVPixelFormatType_444YpCbCr10BiPlanarFullRange:
        case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
            fmt = (i == 0) ? MTLPixelFormatR16Unorm : MTLPixelFormatRG16Unorm;
            texParams.format = pl_find_named_fmt(m_Vulkan->gpu, (i == 0) ? "r16" : "rg16");
            break;

        default:
            unmapVideoToolboxFromPlacebo(mappedFrame);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unknown pixel format: %x",
                         CVPixelBufferGetPixelFormatType(pixBuf));
            return false;
        }

        CVMetalTextureRef cvTexture;
        CVReturn err = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, (CVMetalTextureCacheRef)m_TextureCache,
                                                                 pixBuf, nullptr, fmt, texParams.w, texParams.h, i,
                                                                 &cvTexture);
        if (err != kCVReturnSuccess) {
            unmapVideoToolboxFromPlacebo(mappedFrame);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "CVMetalTextureCacheCreateTextureFromImage() failed: %d",
                         err);
            return false;
        }

        if (i == 0) {
            mappedFrame->planes[i].components = 1;
            mappedFrame->planes[i].component_mapping[0] = PL_CHANNEL_Y;
        }
        else {
            mappedFrame->planes[i].components = 2;
            mappedFrame->planes[i].component_mapping[0] = PL_CHANNEL_CB;
            mappedFrame->planes[i].component_mapping[1] = PL_CHANNEL_CR;
        }

        texParams.import_handle = PL_HANDLE_MTL_TEX;
        texParams.shared_mem.handle.handle = (__bridge void *)CVMetalTextureGetTexture(cvTexture);
        texParams.user_data = cvTexture;
        mappedFrame->planes[i].texture = pl_tex_create(m_Vulkan->gpu, &texParams);
        if (!mappedFrame->planes[i].texture) {
            CFRelease(cvTexture);
            unmapVideoToolboxFromPlacebo(mappedFrame);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "pl_tex_create() failed");
            return false;
        }

        mappedFrame->num_planes++;
    }

    return true;
}

void MetalVulkanTextureFactory::unmapVideoToolboxFromPlacebo(pl_frame* mappedFrame)
{
    for (int i = 0; i < mappedFrame->num_planes; i++) {
        auto cvTexture = (CVMetalTextureRef)mappedFrame->planes[i].texture->params.user_data;
        pl_tex_destroy(m_Vulkan->gpu, &mappedFrame->planes[i].texture);
        CFRelease(cvTexture);
    }
}
