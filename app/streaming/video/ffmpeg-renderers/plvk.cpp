#include "plvk.h"

#include "streaming/session.h"
#include "streaming/streamutils.h"

// Implementation in plvk_c.c
#define PL_LIBAV_IMPLEMENTATION 0
#include <libplacebo/utils/libav.h>

#include <SDL_vulkan.h>

#include <libavutil/hwcontext_vulkan.h>

#include <vector>

// Keep these in sync with hwcontext_vulkan.c
static const char *k_OptionalDeviceExtensions[] = {
    /* Misc or required by other extensions */
    //VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
    VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME,
    VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
    VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME,

    /* Imports/exports */
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,
#ifdef _WIN32
    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
#endif

    /* Video encoding/decoding */
    VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
    "VK_MESA_video_decode_av1",
};

static void pl_log_cb(void*, enum pl_log_level level, const char *msg)
{
    switch (level) {
    case PL_LOG_FATAL:
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", msg);
        break;
    case PL_LOG_ERR:
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", msg);
        break;
    case PL_LOG_WARN:
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", msg);
        break;
    case PL_LOG_INFO:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", msg);
        break;
    case PL_LOG_DEBUG:
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", msg);
        break;
    case PL_LOG_NONE:
    case PL_LOG_TRACE:
        SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "%s", msg);
        break;
    }
}

void PlVkRenderer::lockQueue(struct AVHWDeviceContext *dev_ctx, uint32_t queue_family, uint32_t index)
{
    auto me = (PlVkRenderer*)dev_ctx->user_opaque;
    me->m_Vulkan->lock_queue(me->m_Vulkan, queue_family, index);
}

void PlVkRenderer::unlockQueue(struct AVHWDeviceContext *dev_ctx, uint32_t queue_family, uint32_t index)
{
    auto me = (PlVkRenderer*)dev_ctx->user_opaque;
    me->m_Vulkan->unlock_queue(me->m_Vulkan, queue_family, index);
}

void PlVkRenderer::overlayUploadComplete(void* opaque)
{
    SDL_FreeSurface((SDL_Surface*)opaque);
}

PlVkRenderer::PlVkRenderer(IFFmpegRenderer* backendRenderer) :
    m_Backend(backendRenderer)
{
    bool ok;

    pl_log_params logParams = pl_log_default_params;
    logParams.log_cb = pl_log_cb;
    logParams.log_level = (pl_log_level)qEnvironmentVariableIntValue("PLVK_LOG_LEVEL", &ok);
    if (!ok) {
#ifdef QT_DEBUG
        logParams.log_level = PL_LOG_DEBUG;
#else
        logParams.log_level = PL_LOG_WARN;
#endif
    }

    m_Log = pl_log_create(PL_API_VER, &logParams);
}

PlVkRenderer::~PlVkRenderer()
{
    if (m_Vulkan != nullptr) {
        for (int i = 0; i < (int)SDL_arraysize(m_Overlays); i++) {
            pl_tex_destroy(m_Vulkan->gpu, &m_Overlays[i].overlay.tex);
            pl_tex_destroy(m_Vulkan->gpu, &m_Overlays[i].stagingOverlay.tex);
        }

        for (int i = 0; i < (int)SDL_arraysize(m_Textures); i++) {
            pl_tex_destroy(m_Vulkan->gpu, &m_Textures[i]);
        }
    }

    pl_renderer_destroy(&m_Renderer);
    pl_swapchain_destroy(&m_Swapchain);
    pl_vulkan_destroy(&m_Vulkan);

    // This surface was created by SDL, so there's no libplacebo API to destroy it
    if (fn_vkDestroySurfaceKHR && m_VkSurface) {
        fn_vkDestroySurfaceKHR(m_PlVkInstance->instance, m_VkSurface, nullptr);
    }

    if (m_HwDeviceCtx != nullptr) {
        av_buffer_unref(&m_HwDeviceCtx);
    }

    pl_vk_inst_destroy(&m_PlVkInstance);

    // m_Log must always be the last object destroyed
    pl_log_destroy(&m_Log);
}

#define POPULATE_FUNCTION(name) \
    fn_##name = (PFN_##name)vkInstParams.get_proc_addr(m_PlVkInstance->instance, #name); \
    if (fn_##name == nullptr) { \
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, \
                     "Missing required Vulkan function: " #name); \
        return false; \
    }

bool PlVkRenderer::initialize(PDECODER_PARAMETERS params)
{
    unsigned int instanceExtensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(params->window, &instanceExtensionCount, nullptr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_Vulkan_GetInstanceExtensions() #1 failed: %s",
                     SDL_GetError());
        return false;
    }

    std::vector<const char*> instanceExtensions(instanceExtensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(params->window, &instanceExtensionCount, instanceExtensions.data())) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_Vulkan_GetInstanceExtensions() #2 failed: %s",
                     SDL_GetError());
        return false;
    }

    pl_vk_inst_params vkInstParams = pl_vk_inst_default_params;
    {
        bool ok;
        vkInstParams.debug = !!qEnvironmentVariableIntValue("PLVK_DEBUG", &ok);
#ifdef QT_DEBUG
        if (!ok) {
            vkInstParams.debug = true;
        }
#endif
    }
    vkInstParams.get_proc_addr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    vkInstParams.extensions = instanceExtensions.data();
    vkInstParams.num_extensions = instanceExtensions.size();
    m_PlVkInstance = pl_vk_inst_create(m_Log, &vkInstParams);
    if (m_PlVkInstance == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "pl_vk_inst_create() failed");
        return false;
    }

    // Lookup all Vulkan functions we require
    POPULATE_FUNCTION(vkDestroySurfaceKHR);
    POPULATE_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);
    POPULATE_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR);
    POPULATE_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR);

    if (!SDL_Vulkan_CreateSurface(params->window, m_PlVkInstance->instance, &m_VkSurface)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_Vulkan_CreateSurface() failed: %s",
                     SDL_GetError());
        return false;
    }

    pl_vulkan_params vkParams = pl_vulkan_default_params;
    vkParams.instance = m_PlVkInstance->instance;
    vkParams.get_proc_addr = m_PlVkInstance->get_proc_addr;
    vkParams.surface = m_VkSurface;
    vkParams.allow_software = false;
    vkParams.opt_extensions = k_OptionalDeviceExtensions;
    vkParams.num_opt_extensions = SDL_arraysize(k_OptionalDeviceExtensions);
    vkParams.extra_queues = VK_QUEUE_VIDEO_DECODE_BIT_KHR;
    m_Vulkan = pl_vulkan_create(m_Log, &vkParams);
    if (m_Vulkan == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "pl_vulkan_create() failed");
        return false;
    }

    VkPresentModeKHR presentMode;
    if (params->enableVsync) {
        // We will use mailbox mode if present, otherwise libplacebo will fall back to FIFO
        presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    }
    else {
        // We want immediate mode for V-Sync disabled if possible
        if (isPresentModeSupported(VK_PRESENT_MODE_IMMEDIATE_KHR)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Using Immediate present mode with V-Sync disabled");
            presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Immediate present mode is not supported by the Vulkan driver. Latency may be higher than normal with V-Sync disabled.");

            // FIFO Relaxed can tear if the frame is running late
            if (isPresentModeSupported(VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Using FIFO Relaxed present mode with V-Sync disabled");
                presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            }
            // Mailbox at least provides non-blocking behavior
            else if (isPresentModeSupported(VK_PRESENT_MODE_MAILBOX_KHR)) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Using Mailbox present mode with V-Sync disabled");
                presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            }
            // FIFO is always supported
            else {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Using FIFO present mode with V-Sync disabled");
                presentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
        }
    }

    pl_vulkan_swapchain_params vkSwapchainParams = {};
    vkSwapchainParams.surface = m_VkSurface;
    vkSwapchainParams.present_mode = presentMode;
    vkSwapchainParams.swapchain_depth = 1; // No queued frames
#if PL_API_VER >= 338
    vkSwapchainParams.disable_10bit_sdr = true; // Some drivers don't dither 10-bit SDR output correctly
#endif
    m_Swapchain = pl_vulkan_create_swapchain(m_Vulkan, &vkSwapchainParams);
    if (m_Swapchain == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "pl_vulkan_create_swapchain() failed");
        return false;
    }

    int vkDrawableW, vkDrawableH;
    SDL_Vulkan_GetDrawableSize(params->window, &vkDrawableW, &vkDrawableH);
    pl_swapchain_resize(m_Swapchain, &vkDrawableW, &vkDrawableH);

    m_Renderer = pl_renderer_create(m_Log, m_Vulkan->gpu);
    if (m_Renderer == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "pl_renderer_create() failed");
        return false;
    }

    // We only need an hwaccel device context if we're going to act as the backend renderer too
    if (m_Backend == nullptr) {
        m_HwDeviceCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN);
        if (m_HwDeviceCtx == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN) failed");
            return false;
        }

        auto hwDeviceContext = ((AVHWDeviceContext *)m_HwDeviceCtx->data);
        hwDeviceContext->user_opaque = this; // Used by lockQueue()/unlockQueue()

        auto vkDeviceContext = (AVVulkanDeviceContext*)((AVHWDeviceContext *)m_HwDeviceCtx->data)->hwctx;
        vkDeviceContext->get_proc_addr = m_PlVkInstance->get_proc_addr;
        vkDeviceContext->inst = m_PlVkInstance->instance;
        vkDeviceContext->phys_dev = m_Vulkan->phys_device;
        vkDeviceContext->act_dev = m_Vulkan->device;
        vkDeviceContext->device_features = *m_Vulkan->features;
        vkDeviceContext->enabled_inst_extensions = m_PlVkInstance->extensions;
        vkDeviceContext->nb_enabled_inst_extensions = m_PlVkInstance->num_extensions;
        vkDeviceContext->enabled_dev_extensions = m_Vulkan->extensions;
        vkDeviceContext->nb_enabled_dev_extensions = m_Vulkan->num_extensions;
        vkDeviceContext->queue_family_index = m_Vulkan->queue_graphics.index;
        vkDeviceContext->nb_graphics_queues = m_Vulkan->queue_graphics.count;
        vkDeviceContext->queue_family_tx_index = m_Vulkan->queue_transfer.index;
        vkDeviceContext->nb_tx_queues = m_Vulkan->queue_transfer.count;
        vkDeviceContext->queue_family_comp_index = m_Vulkan->queue_compute.index;
        vkDeviceContext->nb_comp_queues = m_Vulkan->queue_compute.count;
#if LIBAVUTIL_VERSION_INT > AV_VERSION_INT(58, 9, 100)
        vkDeviceContext->lock_queue = lockQueue;
        vkDeviceContext->unlock_queue = unlockQueue;
#endif

        static_assert(sizeof(vkDeviceContext->queue_family_decode_index) == sizeof(uint32_t), "sizeof(int) != sizeof(uint32_t)");
        static_assert(sizeof(vkDeviceContext->nb_decode_queues) == sizeof(uint32_t), "sizeof(int) != sizeof(uint32_t)");
        if (!getQueue(VK_QUEUE_VIDEO_DECODE_BIT_KHR, (uint32_t*)&vkDeviceContext->queue_family_decode_index, (uint32_t*)&vkDeviceContext->nb_decode_queues)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Vulkan video decoding is not supported by the Vulkan device");
            return false;
        }

        int err = av_hwdevice_ctx_init(m_HwDeviceCtx);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "av_hwdevice_ctx_init() failed: %d",
                         err);
            return false;
        }
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "libplacebo Vulkan renderer initialized");

    return true;
}

bool PlVkRenderer::prepareDecoderContext(AVCodecContext *context, AVDictionary **)
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using Vulkan video decoding");

    if (m_Backend) {
        context->hw_device_ctx = av_buffer_ref(m_HwDeviceCtx);
    }
    return true;
}

bool PlVkRenderer::mapAvFrameToPlacebo(const AVFrame *frame, pl_frame* mappedFrame)
{
    pl_avframe_params mapParams = {};
    mapParams.frame = frame;
    mapParams.tex = m_Textures;
    if (!pl_map_avframe_ex(m_Vulkan->gpu, mappedFrame, &mapParams)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "pl_map_avframe_ex() failed");
        return false;
    }

    return true;
}

bool PlVkRenderer::getQueue(VkQueueFlags requiredFlags, uint32_t *queueIndex, uint32_t *queueCount)
{
    uint32_t queueFamilyCount = 0;
    fn_vkGetPhysicalDeviceQueueFamilyProperties(m_Vulkan->phys_device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    fn_vkGetPhysicalDeviceQueueFamilyProperties(m_Vulkan->phys_device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if ((queueFamilies[i].queueFlags & requiredFlags) == requiredFlags) {
            *queueIndex = i;
            *queueCount = queueFamilies[i].queueCount;
            return true;
        }
    }

    return false;
}

bool PlVkRenderer::isPresentModeSupported(VkPresentModeKHR presentMode)
{
    uint32_t presentModeCount = 0;
    fn_vkGetPhysicalDeviceSurfacePresentModesKHR(m_Vulkan->phys_device, m_VkSurface, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    fn_vkGetPhysicalDeviceSurfacePresentModesKHR(m_Vulkan->phys_device, m_VkSurface, &presentModeCount, presentModes.data());

    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == presentMode) {
            return true;
        }
    }

    return false;
}

bool PlVkRenderer::isColorSpaceSupported(VkColorSpaceKHR colorSpace)
{
    uint32_t formatCount = 0;
    fn_vkGetPhysicalDeviceSurfaceFormatsKHR(m_Vulkan->phys_device, m_VkSurface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    fn_vkGetPhysicalDeviceSurfaceFormatsKHR(m_Vulkan->phys_device, m_VkSurface, &formatCount, formats.data());

    for (uint32_t i = 0; i < formatCount; i++) {
        if (formats[i].colorSpace == colorSpace) {
            return true;
        }
    }

    return false;
}

void PlVkRenderer::renderFrame(AVFrame *frame)
{
    pl_frame mappedFrame, targetFrame;
    pl_swapchain_frame swapchainFrame;

    if (!mapAvFrameToPlacebo(frame, &mappedFrame)) {
        // This function logs internally
        return;
    }

    // Reserve enough space to avoid allocating under the overlay lock
    pl_overlay_part overlayParts[Overlay::OverlayMax] = {};
    std::vector<pl_tex> texturesToDestroy;
    std::vector<pl_overlay> overlays;
    texturesToDestroy.reserve(Overlay::OverlayMax);
    overlays.reserve(Overlay::OverlayMax);

    // Get the next swapchain buffer for rendering
    //
    // NB: After calling this successfully, we *MUST* call pl_swapchain_submit_frame()!
    if (!pl_swapchain_start_frame(m_Swapchain, &swapchainFrame)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "pl_swapchain_start_frame() failed");

        // Recreate the renderer
        SDL_Event event;
        event.type = SDL_RENDER_TARGETS_RESET;
        SDL_PushEvent(&event);
        goto UnmapExit;
    }

    pl_frame_from_swapchain(&targetFrame, &swapchainFrame);

    // We perform minimal processing under the overlay lock to avoid blocking threads updating the overlay
    SDL_AtomicLock(&m_OverlayLock);
    for (int i = 0; i < Overlay::OverlayMax; i++) {
        // If we have a staging overlay, we need to transfer ownership to us
        if (m_Overlays[i].hasStagingOverlay) {
            if (m_Overlays[i].hasOverlay) {
                texturesToDestroy.push_back(m_Overlays[i].overlay.tex);
            }

            // Copy the overlay fields from the staging area
            m_Overlays[i].overlay = m_Overlays[i].stagingOverlay;

            // We now own the staging overlay
            m_Overlays[i].hasStagingOverlay = false;
            SDL_zero(m_Overlays[i].stagingOverlay);
            m_Overlays[i].hasOverlay = true;
        }

        // If we have an overlay but it's been disabled, free the overlay texture
        if (m_Overlays[i].hasOverlay && !Session::get()->getOverlayManager().isOverlayEnabled((Overlay::OverlayType)i)) {
            texturesToDestroy.push_back(m_Overlays[i].overlay.tex);
            m_Overlays[i].hasOverlay = false;
        }

        // We have an overlay to draw
        if (m_Overlays[i].hasOverlay) {
            // Position the overlay
            overlayParts[i].src = { 0, 0, (float)m_Overlays[i].overlay.tex->params.w, (float)m_Overlays[i].overlay.tex->params.h };
            if (i == Overlay::OverlayStatusUpdate) {
                // Bottom Left
                overlayParts[i].dst.x0 = 0;
                overlayParts[i].dst.y0 = SDL_min(0, targetFrame.crop.y1 - overlayParts[i].src.y1);
            }
            else if (i == Overlay::OverlayDebug) {
                // Top left
                overlayParts[i].dst.x0 = 0;
                overlayParts[i].dst.y0 = 0;
            }
            overlayParts[i].dst.x1 = overlayParts[i].dst.x0 + overlayParts[i].src.x1;
            overlayParts[i].dst.y1 = overlayParts[i].dst.y0 + overlayParts[i].src.y1;

            m_Overlays[i].overlay.parts = &overlayParts[i];
            m_Overlays[i].overlay.num_parts = 1;

            overlays.push_back(m_Overlays[i].overlay);
        }
    }
    SDL_AtomicUnlock(&m_OverlayLock);

    SDL_Rect src;
    src.x = 0;
    src.y = 0;
    src.w = frame->width;
    src.h = frame->height;

    SDL_Rect dst;
    dst.x = targetFrame.crop.x0;
    dst.y = targetFrame.crop.y0;
    dst.w = targetFrame.crop.x1 - targetFrame.crop.x0;
    dst.h = targetFrame.crop.y1 - targetFrame.crop.y0;

    // Scale the video to the surface size while preserving the aspect ratio
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    targetFrame.crop.x0 = dst.x;
    targetFrame.crop.y0 = dst.y;
    targetFrame.crop.x1 = dst.x + dst.w;
    targetFrame.crop.y1 = dst.y + dst.h;

    // Render the video image and overlays into the swapchain buffer
    targetFrame.num_overlays = overlays.size();
    targetFrame.overlays = overlays.data();
    if (!pl_render_image(m_Renderer, &mappedFrame, &targetFrame, &pl_render_fast_params)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "pl_render_image() failed");
        // NB: We must fallthrough to call pl_swapchain_submit_frame()
    }

    // Submit the frame for display and swap buffers
    if (!pl_swapchain_submit_frame(m_Swapchain)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "pl_swapchain_submit_frame() failed");

        // Recreate the renderer
        SDL_Event event;
        event.type = SDL_RENDER_TARGETS_RESET;
        SDL_PushEvent(&event);
        goto UnmapExit;
    }
    pl_swapchain_swap_buffers(m_Swapchain);

UnmapExit:
    // Delete any textures that need to be destroyed
    for (pl_tex texture : texturesToDestroy) {
        pl_tex_destroy(m_Vulkan->gpu, &texture);
    }

    pl_unmap_avframe(m_Vulkan->gpu, &mappedFrame);
}

bool PlVkRenderer::testRenderFrame(AVFrame *frame)
{
    // Test if the frame can be mapped to libplacebo
    pl_frame mappedFrame;
    if (!mapAvFrameToPlacebo(frame, &mappedFrame)) {
        return false;
    }

    pl_unmap_avframe(m_Vulkan->gpu, &mappedFrame);
    return true;
}

void PlVkRenderer::notifyOverlayUpdated(Overlay::OverlayType type)
{
    SDL_Surface* newSurface = Session::get()->getOverlayManager().getUpdatedOverlaySurface(type);
    if (newSurface == nullptr && Session::get()->getOverlayManager().isOverlayEnabled(type)) {
        // The overlay is enabled and there is no new surface. Leave the old texture alone.
        return;
    }

    SDL_AtomicLock(&m_OverlayLock);
    // We want to clear the staging overlay flag even if a staging overlay is still present,
    // since this ensures the render thread will not read from a partially initialized pl_tex
    // as we modify or recreate the staging overlay texture outside the overlay lock.
    m_Overlays[type].hasStagingOverlay = false;
    SDL_AtomicUnlock(&m_OverlayLock);

    // If there's no new staging overlay, free the old staging overlay texture.
    // NB: This is safe to do outside the overlay lock because we're guaranteed
    // to not have racing readers/writers if hasStagingOverlay is false.
    if (newSurface == nullptr) {
        pl_tex_destroy(m_Vulkan->gpu, &m_Overlays[type].stagingOverlay.tex);
        SDL_zero(m_Overlays[type].stagingOverlay);
        return;
    }

    // Find a compatible texture format
    SDL_assert(newSurface->format->format == SDL_PIXELFORMAT_ARGB8888);
    pl_fmt texFormat = pl_find_named_fmt(m_Vulkan->gpu, "bgra8");
    if (!texFormat) {
        SDL_FreeSurface(newSurface);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "pl_find_named_fmt(bgra8) failed");
        return;
    }

    // Create a new texture for this overlay if necessary, otherwise reuse the existing texture.
    // NB: We're guaranteed that the render thread won't be reading this concurrently because
    // we set hasStagingOverlay to false above.
    pl_tex_params texParams = {};
    texParams.w = newSurface->w;
    texParams.h = newSurface->h;
    texParams.format = texFormat;
    texParams.sampleable = true;
    texParams.host_writable = true;
    texParams.blit_src = !!(texFormat->caps & PL_FMT_CAP_BLITTABLE);
    texParams.debug_tag = PL_DEBUG_TAG;
    if (!pl_tex_recreate(m_Vulkan->gpu, &m_Overlays[type].stagingOverlay.tex, &texParams)) {
        SDL_FreeSurface(newSurface);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "pl_tex_recreate() failed");
        return;
    }

    // Upload the surface data to the new texture
    SDL_assert(!SDL_MUSTLOCK(newSurface));
    pl_tex_transfer_params xferParams = {};
    xferParams.tex = m_Overlays[type].stagingOverlay.tex;
    xferParams.row_pitch = (size_t)newSurface->pitch;
    xferParams.ptr = newSurface->pixels;
    xferParams.callback = overlayUploadComplete;
    xferParams.priv = newSurface;
    if (!pl_tex_upload(m_Vulkan->gpu, &xferParams)) {
        SDL_FreeSurface(newSurface);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "pl_tex_upload() failed");
        return;
    }

    // newSurface is now owned by the texture upload process. It will be freed in overlayUploadComplete()
    newSurface = nullptr;

    // Initialize the rest of the overlay params
    m_Overlays[type].stagingOverlay.mode = PL_OVERLAY_NORMAL;
    m_Overlays[type].stagingOverlay.coords = PL_OVERLAY_COORDS_DST_FRAME;
    m_Overlays[type].stagingOverlay.repr = pl_color_repr_rgb;
    m_Overlays[type].stagingOverlay.color = pl_color_space_srgb;

    // Make this staging overlay visible to the render thread
    SDL_AtomicLock(&m_OverlayLock);
    SDL_assert(!m_Overlays[type].hasStagingOverlay);
    m_Overlays[type].hasStagingOverlay = true;
    SDL_AtomicUnlock(&m_OverlayLock);
}

void PlVkRenderer::setHdrMode(bool enabled)
{
    pl_color_space csp = {};

    if (enabled) {
        csp.primaries = PL_COLOR_PRIM_BT_2020;
        csp.transfer = PL_COLOR_TRC_PQ;

        // Use the host's provided HDR metadata if present
        SS_HDR_METADATA hdrMetadata;
        if (LiGetHdrMetadata(&hdrMetadata)) {
            csp.hdr.prim.red.x = hdrMetadata.displayPrimaries[0].x / 50000.f;
            csp.hdr.prim.red.y = hdrMetadata.displayPrimaries[0].y / 50000.f;
            csp.hdr.prim.green.x = hdrMetadata.displayPrimaries[1].x / 50000.f;
            csp.hdr.prim.green.y = hdrMetadata.displayPrimaries[1].y / 50000.f;
            csp.hdr.prim.blue.x = hdrMetadata.displayPrimaries[2].x / 50000.f;
            csp.hdr.prim.blue.y = hdrMetadata.displayPrimaries[2].y / 50000.f;
            csp.hdr.prim.white.x = hdrMetadata.whitePoint.x / 50000.f;
            csp.hdr.prim.white.y = hdrMetadata.whitePoint.y / 50000.f;
            csp.hdr.min_luma = hdrMetadata.minDisplayLuminance / 10000.f;
            csp.hdr.max_luma = hdrMetadata.maxDisplayLuminance;
            csp.hdr.max_cll = hdrMetadata.maxContentLightLevel;
            csp.hdr.max_fall = hdrMetadata.maxFrameAverageLightLevel;
        }
        else {
            // Use the generic HDR10 metadata if the host doesn't provide HDR metadata
            csp.hdr = pl_hdr_metadata_hdr10;
        }
    }
    else {
        csp.primaries = PL_COLOR_PRIM_UNKNOWN;
        csp.transfer = PL_COLOR_TRC_UNKNOWN;
    }

    pl_swapchain_colorspace_hint(m_Swapchain, &csp);
}

int PlVkRenderer::getRendererAttributes()
{
    int attributes = 0;

    if (isColorSpaceSupported(VK_COLOR_SPACE_HDR10_ST2084_EXT)) {
        attributes |= RENDERER_ATTRIBUTE_HDR_SUPPORT;
    }

    return attributes;
}

int PlVkRenderer::getDecoderCapabilities()
{
    return CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC |
           CAPABILITY_REFERENCE_FRAME_INVALIDATION_AV1;
}

bool PlVkRenderer::needsTestFrame()
{
    // We need a test frame to verify that Vulkan video decoding is working
    return true;
}

bool PlVkRenderer::isPixelFormatSupported(int videoFormat, AVPixelFormat pixelFormat)
{
    if (m_Backend) {
        return m_Backend->isPixelFormatSupported(videoFormat, pixelFormat);
    }
    else {
        return IFFmpegRenderer::isPixelFormatSupported(videoFormat, pixelFormat);
    }
}

AVPixelFormat PlVkRenderer::getPreferredPixelFormat(int videoFormat)
{
    if (m_Backend) {
        return m_Backend->getPreferredPixelFormat(videoFormat);
    }
    else {
        return AV_PIX_FMT_VULKAN;
    }
}

IFFmpegRenderer::RendererType PlVkRenderer::getRendererType()
{
    return IFFmpegRenderer::RendererType::Vulkan;
}
