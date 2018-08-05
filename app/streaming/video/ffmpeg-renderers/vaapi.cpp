#include "vaapi.h"
#include <streaming/streamutils.h>

#include <dlfcn.h>

#include <SDL_syswm.h>

VAAPIRenderer::VAAPIRenderer()
    : m_HwContext(nullptr),
      m_X11VaLibHandle(nullptr),
      m_vaPutSurface(nullptr)
{

}

VAAPIRenderer::~VAAPIRenderer()
{
    if (m_HwContext != nullptr) {
        av_buffer_unref(&m_HwContext);
    }

    if (m_X11VaLibHandle != nullptr) {
        dlclose(m_X11VaLibHandle);
    }
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
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL_GetWindowWMInfo() failed: %s",
                    SDL_GetError());
        return false;
    }

    SDL_assert(info.subsystem == SDL_SYSWM_X11);

    if (info.subsystem == SDL_SYSWM_X11) {
        m_XWindow = info.info.x11.window;

        m_X11VaLibHandle = dlopen("libva-x11.so", RTLD_LAZY);
        if (!m_X11VaLibHandle) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "dlopen(libva.so) failed: %s",
                        dlerror());
            return false;
        }

        m_vaPutSurface = (vaPutSurface_t)dlsym(m_X11VaLibHandle, "vaPutSurface");
    }
    else if (info.subsystem == SDL_SYSWM_WAYLAND) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "VAAPI backend does not currently support Wayland");
        return false;
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unsupported VAAPI rendering subsystem: %d",
                     info.subsystem);
        return false;
    }

    err = av_hwdevice_ctx_create(&m_HwContext,
                                 AV_HWDEVICE_TYPE_VAAPI,
                                 nullptr, nullptr, 0);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create VAAPI context: %d",
                     err);
        return false;
    }

    // This quirk is set for the VDPAU wrapper which doesn't work with our VAAPI renderer
    if (((AVVAAPIDeviceContext*)((AVHWDeviceContext*)(m_HwContext->data))->hwctx)->driver_quirks & AV_VAAPI_DRIVER_QUIRK_SURFACE_ATTRIBUTES) {
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
                "Using VAAPI accelerated renderer");

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

    m_vaPutSurface(vaDeviceContext->display,
                   surface,
                   m_XWindow,
                   0, 0,
                   m_VideoWidth, m_VideoHeight,
                   dst.x, dst.y,
                   dst.w, dst.h,
                   NULL, 0, 0);

    av_frame_free(&frame);
}
