#include "vaapi.h"
#include <streaming/streamutils.h>

#include <SDL_syswm.h>

VAAPIRenderer::VAAPIRenderer()
    : m_HwContext(nullptr)
{

}

VAAPIRenderer::~VAAPIRenderer()
{
    if (m_HwContext != nullptr) {
        AVHWDeviceContext* deviceContext = (AVHWDeviceContext*)m_HwContext->data;
        AVVAAPIDeviceContext* vaDeviceContext = (AVVAAPIDeviceContext*)deviceContext->hwctx;

        // Hold onto this VADisplay since we'll need it to uninitialize VAAPI
        VADisplay display = vaDeviceContext->display;

        av_buffer_unref(&m_HwContext);

        if (display) {
            vaTerminate(display);
        }
    }
}

void VAAPIRenderer::vaapiLogError(void*, const char *message)
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "VAAPI: %s", message);
}

void VAAPIRenderer::vaapiLogInfo(void*, const char *message)
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "VAAPI: %s", message);
}

bool
VAAPIRenderer::initialize(SDL_Window* window, int, int width, int height)
{
    int err;
    SDL_SysWMinfo info;

    m_VideoWidth = width;
    m_VideoHeight = height;

    SDL_GetWindowSize(window, &m_DisplayWidth, &m_DisplayHeight);

    SDL_VERSION(&info.version);

    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return false;
    }

    m_HwContext = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!m_HwContext) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to allocate VAAPI context");
        return false;
    }

    AVHWDeviceContext* deviceContext = (AVHWDeviceContext*)m_HwContext->data;
    AVVAAPIDeviceContext* vaDeviceContext = (AVVAAPIDeviceContext*)deviceContext->hwctx;

    m_WindowSystem = info.subsystem;
    if (info.subsystem == SDL_SYSWM_X11) {
#ifdef HAVE_LIBVA_X11
        m_XWindow = info.info.x11.window;
        vaDeviceContext->display = vaGetDisplay(info.info.x11.display);
        if (!vaDeviceContext->display) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to open X11 display for VAAPI");
            return false;
        }
#else
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Moonlight not compiled with VAAPI X11 support!");
        return false;
#endif
    }
    else if (info.subsystem == SDL_SYSWM_WAYLAND) {
#ifdef HAVE_LIBVA_WAYLAND
        m_WaylandSurface = info.info.wl.surface;
        m_WaylandDisplay = info.info.wl.display;
        vaDeviceContext->display = vaGetDisplayWl(info.info.wl.display);
        if (!vaDeviceContext->display) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to open Wayland display for VAAPI");
            return false;
        }
#else
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Moonlight not compiled with VAAPI Wayland support!");
        return false;
#endif
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unsupported VAAPI rendering subsystem: %d",
                     info.subsystem);
        return false;
    }

    vaSetErrorCallback(vaDeviceContext->display, &VAAPIRenderer::vaapiLogError, nullptr);
    vaSetInfoCallback(vaDeviceContext->display, &VAAPIRenderer::vaapiLogInfo, nullptr);

    int major, minor;
    VAStatus status;
    status = vaInitialize(vaDeviceContext->display, &major, &minor);
    if (status != VA_STATUS_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to initialize VAAPI: %d",
                     status);
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Initialized VAAPI %d.%d",
                major, minor);

    // This will populate the driver_quirks
    err = av_hwdevice_ctx_init(m_HwContext);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to initialize VAAPI context: %d",
                     err);
        return false;
    }

    // This quirk is set for the VDPAU wrapper which doesn't work with our VAAPI renderer
    if (vaDeviceContext->driver_quirks & AV_VAAPI_DRIVER_QUIRK_SURFACE_ATTRIBUTES) {
        // Fail and let our VDPAU renderer pick this up
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Avoiding VDPAU wrapper for VAAPI decoding");
        return false;
    }

    return true;
}

bool
VAAPIRenderer::prepareDecoderContext(AVCodecContext* context)
{
    context->hw_device_ctx = av_buffer_ref(m_HwContext);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using VAAPI accelerated renderer on %s",
                m_WindowSystem == SDL_SYSWM_X11 ? "X11" : "Wayland");

    return true;
}

void
VAAPIRenderer::renderFrame(AVFrame* frame)
{
    VASurfaceID surface = (VASurfaceID)(uintptr_t)frame->data[3];
    AVHWDeviceContext* deviceContext = (AVHWDeviceContext*)m_HwContext->data;
    AVVAAPIDeviceContext* vaDeviceContext = (AVVAAPIDeviceContext*)deviceContext->hwctx;

    SDL_Rect src, dst;
    src.x = src.y = 0;
    src.w = m_VideoWidth;
    src.h = m_VideoHeight;
    dst.x = dst.y = 0;
    dst.w = m_DisplayWidth;
    dst.h = m_DisplayHeight;

    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    if (m_WindowSystem == SDL_SYSWM_X11) {
#ifdef HAVE_LIBVA_X11
        vaPutSurface(vaDeviceContext->display,
                     surface,
                     m_XWindow,
                     0, 0,
                     m_VideoWidth, m_VideoHeight,
                     dst.x, dst.y,
                     dst.w, dst.h,
                     NULL, 0, 0);
#endif
    }
    else if (m_WindowSystem == SDL_SYSWM_WAYLAND) {
#ifdef HAVE_LIBVA_WAYLAND
        struct wl_buffer* buffer;
        VAStatus status;

        status = vaGetSurfaceBufferWl(vaDeviceContext->display,
                                      surface,
                                      VA_FRAME_PICTURE,
                                      &buffer);
        if (status == VA_STATUS_SUCCESS) {
            wl_surface_attach(m_WaylandSurface, buffer, 0, 0);
            wl_surface_damage(m_WaylandSurface, dst.x, dst.y, dst.w, dst.h);

            wl_display_flush(m_WaylandDisplay);
            wl_surface_commit(m_WaylandSurface);
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "vaGetSurfaceBufferWl failed(): %d",
                         status);
        }
#endif
    }
    else {
        // We don't accept anything else in initialize().
        SDL_assert(false);
    }

    av_frame_free(&frame);
}
