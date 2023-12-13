#pragma once

#include "renderer.h"

#include <libplacebo/log.h>
#include <libplacebo/renderer.h>
#include <libplacebo/vulkan.h>

class PlVkRenderer : public IFFmpegRenderer {
public:
    PlVkRenderer(IFFmpegRenderer* backendRenderer);
    virtual ~PlVkRenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual bool testRenderFrame(AVFrame* frame) override;
    virtual void notifyOverlayUpdated(Overlay::OverlayType) override;
    virtual void setHdrMode(bool enabled) override;
    virtual int getRendererAttributes() override;
    virtual int getDecoderCapabilities() override;
    virtual bool isPixelFormatSupported(int videoFormat, enum AVPixelFormat pixelFormat) override;
    virtual AVPixelFormat getPreferredPixelFormat(int videoFormat) override;
    virtual RendererType getRendererType() override;

private:
    static void lockQueue(AVHWDeviceContext *dev_ctx, uint32_t queue_family, uint32_t index);
    static void unlockQueue(AVHWDeviceContext *dev_ctx, uint32_t queue_family, uint32_t index);
    static void overlayUploadComplete(void* opaque);

    bool mapAvFrameToPlacebo(const AVFrame *frame, pl_frame* mappedFrame);
    bool getQueue(VkQueueFlags requiredFlags, uint32_t* queueIndex, uint32_t* queueCount);
    bool isPresentModeSupported(VkPresentModeKHR presentMode);
    bool isColorSpaceSupported(VkColorSpaceKHR colorSpace);

    // The backend renderer if we're frontend-only
    IFFmpegRenderer* m_Backend;

    // The libplacebo rendering state
    pl_log m_Log = nullptr;
    pl_vk_inst m_PlVkInstance = nullptr;
    VkSurfaceKHR m_VkSurface = nullptr;
    pl_vulkan m_Vulkan = nullptr;
    pl_swapchain m_Swapchain = nullptr;
    pl_renderer m_Renderer = nullptr;
    pl_tex m_Textures[4] = {};

    // Overlay state
    SDL_SpinLock m_OverlayLock = 0;
    struct {
        // The staging overlay state is copied here under the overlay lock in the render thread
        bool hasOverlay;
        pl_overlay overlay;

        // This state is written by the overlay update thread
        bool hasStagingOverlay;
        pl_overlay stagingOverlay;
    } m_Overlays[Overlay::OverlayMax] = {};

    // Device context used for hwaccel decoders
    AVBufferRef* m_HwDeviceCtx = nullptr;

    // Vulkan functions we call directly
    PFN_vkDestroySurfaceKHR fn_vkDestroySurfaceKHR = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties fn_vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fn_vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fn_vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
};
