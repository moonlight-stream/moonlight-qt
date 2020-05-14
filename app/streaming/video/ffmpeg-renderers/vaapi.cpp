#include <QString>

#include "vaapi.h"
#include "utils.h"
#include <streaming/streamutils.h>

#include <SDL_syswm.h>
#ifdef HAVE_EGL
#include <SDL_egl.h>
#endif

#include <unistd.h>
#include <fcntl.h>

VAAPIRenderer::VAAPIRenderer()
    : m_HwContext(nullptr),
      m_DrmFd(-1),
      m_BlacklistedForDirectRendering(false)
{
#ifdef HAVE_EGL
    m_PrimeDescriptor.num_layers = 0;
    m_PrimeDescriptor.num_objects = 0;
    m_EGLExtDmaBuf = false;
#endif
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

    if (m_DrmFd != -1) {
        close(m_DrmFd);
    }
}

VADisplay
VAAPIRenderer::openDisplay(SDL_Window* window)
{
    SDL_SysWMinfo info;
    VADisplay display;

    SDL_VERSION(&info.version);

    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return nullptr;
    }

    m_WindowSystem = info.subsystem;
    if (info.subsystem == SDL_SYSWM_X11) {
        m_XWindow = info.info.x11.window;
#ifdef HAVE_LIBVA_X11
        display = vaGetDisplay(info.info.x11.display);
        if (display == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to open X11 display for VAAPI");
            return nullptr;
        }
#else
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Moonlight not compiled with VAAPI X11 support!");
        return nullptr;
#endif
    }
    else if (info.subsystem == SDL_SYSWM_WAYLAND) {
#ifdef HAVE_LIBVA_WAYLAND
        display = vaGetDisplayWl(info.info.wl.display);
        if (display == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to open Wayland display for VAAPI");
            return nullptr;
        }
#else
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Moonlight not compiled with VAAPI Wayland support!");
        return nullptr;
#endif
    }
    // TODO: Upstream a better solution for SDL_GetWindowWMInfo on KMSDRM
    else if (strcmp(SDL_GetCurrentVideoDriver(), "KMSDRM") == 0) {
#ifdef HAVE_LIBVA_DRM
        if (m_DrmFd < 0) {
            const char* device = SDL_getenv("DRM_DEV");

            if (device == nullptr) {
                device = "/dev/dri/card0";
            }

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Opening DRM device: %s",
                        device);

            m_DrmFd = open(device, O_RDWR | O_CLOEXEC);
            if (m_DrmFd < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to open DRM device: %d",
                             errno);
                return nullptr;
            }
        }

        display = vaGetDisplayDRM(m_DrmFd);
        if (display == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to open DRM display for VAAPI");
            return nullptr;
        }
#else
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Moonlight not compiled with VAAPI DRM support!");
        return nullptr;
#endif
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unsupported VAAPI rendering subsystem: %d",
                     info.subsystem);
        return nullptr;
    }

    return display;
}

bool
VAAPIRenderer::initialize(PDECODER_PARAMETERS params)
{
    int err;

    m_VideoWidth = params->width;
    m_VideoHeight = params->height;

    SDL_GetWindowSize(params->window, &m_DisplayWidth, &m_DisplayHeight);

    m_HwContext = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!m_HwContext) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to allocate VAAPI context");
        return false;
    }

    AVHWDeviceContext* deviceContext = (AVHWDeviceContext*)m_HwContext->data;
    AVVAAPIDeviceContext* vaDeviceContext = (AVVAAPIDeviceContext*)deviceContext->hwctx;

    vaDeviceContext->display = openDisplay(params->window);
    if (vaDeviceContext->display == nullptr) {
        // openDisplay() logs the error
        return false;
    }

    int major, minor;
    VAStatus status;
    bool setPathVar = false;

    for (;;) {
        status = vaInitialize(vaDeviceContext->display, &major, &minor);
        if (status != VA_STATUS_SUCCESS && qEnvironmentVariableIsEmpty("LIBVA_DRIVER_NAME")) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Trying fallback VAAPI driver names");

            // It would be nice to use vaSetDriverName() here, but there's no way to unset
            // it and get back to the default driver selection logic once we've overridden
            // the driver name using that API. As a result, we must use LIBVA_DRIVER_NAME.

            if (status != VA_STATUS_SUCCESS) {
                // The iHD driver supports newer hardware like Ice Lake and Comet Lake.
                // It should be picked by default on those platforms, but that doesn't
                // always seem to be the case for some reason.
                qputenv("LIBVA_DRIVER_NAME", "iHD");
                status = vaInitialize(vaDeviceContext->display, &major, &minor);
            }

            if (status != VA_STATUS_SUCCESS) {
                // The Iris driver in Mesa 20.0 returns a bogus VA driver (iris_drv_video.so)
                // even though the correct driver is still i965. If we hit this path, we'll
                // explicitly try i965 to handle this case.
                qputenv("LIBVA_DRIVER_NAME", "i965");
                status = vaInitialize(vaDeviceContext->display, &major, &minor);
            }

            if (status != VA_STATUS_SUCCESS) {
                // The RadeonSI driver is compatible with XWayland but can't be detected by libva
                // so try it too if all else fails.
                qputenv("LIBVA_DRIVER_NAME", "radeonsi");
                status = vaInitialize(vaDeviceContext->display, &major, &minor);
            }

            if (status != VA_STATUS_SUCCESS) {
                // Unset LIBVA_DRIVER_NAME if none of the drivers we tried worked. This ensures
                // we will get a fresh start using the default driver selection behavior after
                // setting LIBVA_DRIVERS_PATH in the code below.
                qunsetenv("LIBVA_DRIVER_NAME");
            }
        }

        if (status == VA_STATUS_SUCCESS) {
            // Success!
            break;
        }

        if (qEnvironmentVariableIsEmpty("LIBVA_DRIVERS_PATH")) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Trying fallback VAAPI driver paths");

            qputenv("LIBVA_DRIVERS_PATH",
        #if Q_PROCESSOR_WORDSIZE == 8
                    "/usr/lib64/dri:" // Fedora x86_64
                    "/usr/lib64/va/drivers:" // Gentoo x86_64
        #endif
                    "/usr/lib/dri:" // Arch i386 & x86_64, Fedora i386
                    "/usr/lib/va/drivers:" // Gentoo i386
        #if defined(Q_PROCESSOR_X86_64)
                    "/usr/lib/x86_64-linux-gnu/dri:" // Ubuntu/Debian x86_64
        #elif defined(Q_PROCESSOR_X86_32)
                    "/usr/lib/i386-linux-gnu/dri:" // Ubuntu/Debian i386
        #endif
                    );
           setPathVar = true;
        }
        else {
            if (setPathVar) {
                // Unset LIBVA_DRIVERS_PATH if we set it ourselves
                // and we didn't find any working VAAPI drivers.
                qunsetenv("LIBVA_DRIVERS_PATH");
            }

            // Give up
            break;
        }
    }

    if (status != VA_STATUS_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to initialize VAAPI: %d",
                     status);
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Initialized VAAPI %d.%d",
                major, minor);

    const char* vendorString = vaQueryVendorString(vaDeviceContext->display);
    QString vendorStr(vendorString);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Driver: %s",
                vendorString ? vendorString : "<unknown>");

    // AMD's Gallium VAAPI driver has a nasty memory leak that causes memory
    // to be leaked for each submitted frame. The Flatpak runtime has a VDPAU
    // driver in place that works well, so use that instead on AMD systems. If
    // we're using Wayland, we have no choice but to use VAAPI because VDPAU
    // is only supported under X11 or XWayland.
    if (qgetenv("FORCE_VAAPI") != "1" && !WMUtils::isRunningWayland()) {
        if (vendorStr.contains("AMD", Qt::CaseInsensitive) ||
                vendorStr.contains("Radeon", Qt::CaseInsensitive)) {
            // Fail and let VDPAU pick this up
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Avoiding VAAPI on AMD driver");
            return false;
        }
    }

    if (WMUtils::isRunningWayland()) {
        // The iHD VAAPI driver can initialize on XWayland but it crashes in
        // vaPutSurface() so we must also not directly render on XWayland.
        m_BlacklistedForDirectRendering = vendorStr.contains("iHD");
    }

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
VAAPIRenderer::prepareDecoderContext(AVCodecContext* context, AVDictionary**)
{
    context->hw_device_ctx = av_buffer_ref(m_HwContext);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using VAAPI accelerated renderer on %s",
                SDL_GetCurrentVideoDriver());

    return true;
}

bool
VAAPIRenderer::needsTestFrame()
{
    // We need a test frame to see if this VAAPI driver
    // supports the profile used for streaming
    return true;
}

bool
VAAPIRenderer::isDirectRenderingSupported()
{
    // We only support direct rendering on X11 with VAEntrypointVideoProc support
    if (m_WindowSystem != SDL_SYSWM_X11 || m_BlacklistedForDirectRendering) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Using indirect rendering due to WM or blacklist");
        return false;
    }

    AVHWDeviceContext* deviceContext = (AVHWDeviceContext*)m_HwContext->data;
    AVVAAPIDeviceContext* vaDeviceContext = (AVVAAPIDeviceContext*)deviceContext->hwctx;
    VAEntrypoint entrypoints[vaMaxNumEntrypoints(vaDeviceContext->display)];
    int entrypointCount;
    VAStatus status = vaQueryConfigEntrypoints(vaDeviceContext->display, VAProfileNone, entrypoints, &entrypointCount);
    if (status == VA_STATUS_SUCCESS) {
        for (int i = 0; i < entrypointCount; i++) {
            // Without VAEntrypointVideoProc support, the driver will crash inside vaPutSurface()
            if (entrypoints[i] == VAEntrypointVideoProc) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Using direct rendering with VAEntrypointVideoProc");
                return true;
            }
        }
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using indirect rendering due to lack of VAEntrypointVideoProc");
    return false;
}

int VAAPIRenderer::getDecoderColorspace()
{
    // Gallium drivers don't support Rec 709 yet - https://gitlab.freedesktop.org/mesa/mesa/issues/1915
    // Intel-vaapi-driver defaults to Rec 601 - https://github.com/intel/intel-vaapi-driver/blob/021bcb79d1bd873bbd9fbca55f40320344bab866/src/i965_output_dri.c#L186
    return COLORSPACE_REC_601;
}

void
VAAPIRenderer::renderFrame(AVFrame* frame)
{
    if (frame == nullptr) {
        // End of stream - nothing to do for us
        return;
    }

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
        unsigned int flags = 0;

        // NB: Not all VAAPI drivers respect these flags. Many drivers
        // just ignore them and do the color conversion as Rec 601.
        switch (frame->colorspace) {
        case AVCOL_SPC_BT709:
            flags |= VA_SRC_BT709;
            break;
        case AVCOL_SPC_BT470BG:
        case AVCOL_SPC_SMPTE170M:
            flags |= VA_SRC_BT601;
            break;
        case AVCOL_SPC_SMPTE240M:
            flags |= VA_SRC_SMPTE_240;
            break;
        default:
            // Unknown colorspace
            SDL_assert(false);
            break;
        }

        vaPutSurface(vaDeviceContext->display,
                     surface,
                     m_XWindow,
                     0, 0,
                     m_VideoWidth, m_VideoHeight,
                     dst.x, dst.y,
                     dst.w, dst.h,
                     NULL, 0, flags);
#endif
    }
    else if (m_WindowSystem == SDL_SYSWM_WAYLAND) {
        // We don't support direct rendering on Wayland, so we should
        // never get called there. Many common Wayland compositors don't
        // support YUV surfaces, so direct rendering would fail.
        SDL_assert(false);
    }
    else {
        // We don't accept anything else in initialize().
        SDL_assert(false);
    }
}

#ifdef HAVE_EGL

bool
VAAPIRenderer::canExportEGL() {
    return true;
}

bool
VAAPIRenderer::initializeEGL(EGLDisplay,
                             const EGLExtensions &ext) {
    if (!ext.isSupported("EGL_EXT_image_dma_buf_import")) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "VAAPI-EGL: DMABUF unsupported");
        return false;
    }
    m_EGLExtDmaBuf = ext.isSupported("EGL_EXT_image_dma_buf_import_modifiers");
    return true;
}

ssize_t
VAAPIRenderer::exportEGLImages(AVFrame *frame, EGLDisplay dpy,
                               EGLImage images[EGL_MAX_PLANES]) {
    ssize_t count = 0;
    auto hwFrameCtx = (AVHWFramesContext*)frame->hw_frames_ctx->data;
    AVVAAPIDeviceContext* vaDeviceContext = (AVVAAPIDeviceContext*)hwFrameCtx->device_ctx->hwctx;

    VASurfaceID surface_id = (VASurfaceID)(uintptr_t)frame->data[3];
    VAStatus st = vaExportSurfaceHandle(vaDeviceContext->display,
                                        surface_id,
                                        VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                        VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                        &m_PrimeDescriptor);
    if (st != VA_STATUS_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vaExportSurfaceHandle failed: %d", st);
        return -1;
    }

    SDL_assert(m_PrimeDescriptor.num_layers <= EGL_MAX_PLANES);

    st = vaSyncSurface(vaDeviceContext->display, surface_id);
    if (st != VA_STATUS_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "vaSyncSurface failed: %d", st);
        goto sync_fail;
    }

    for (size_t i = 0; i < m_PrimeDescriptor.num_layers; ++i) {
        const auto &layer = m_PrimeDescriptor.layers[i];
        const auto &object = m_PrimeDescriptor.objects[layer.object_index[0]];

        EGLAttrib attribs[17] = {
            EGL_LINUX_DRM_FOURCC_EXT, (EGLint)layer.drm_format,
            EGL_WIDTH, i == 0 ? frame->width : frame->width / 2,
            EGL_HEIGHT, i == 0 ? frame->height : frame->height / 2,
            EGL_DMA_BUF_PLANE0_FD_EXT, object.fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, (EGLint)layer.offset[0],
            EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint)layer.pitch[0],
            EGL_NONE,
        };
        if (m_EGLExtDmaBuf) {
            const EGLAttrib extra[] = {
                EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
                (EGLint)object.drm_format_modifier,
                EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
                (EGLint)(object.drm_format_modifier >> 32),
                EGL_NONE,
            };
            memcpy((void *)(&attribs[12]), (void *)extra, sizeof (extra));
        }
        images[i] = eglCreateImage(dpy, EGL_NO_CONTEXT,
                                   EGL_LINUX_DMA_BUF_EXT,
                                   nullptr, attribs);
        if (!images[i]) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "eglCreateImage() Failed: %d", eglGetError());
            goto create_image_fail;
        }
        ++count;
    }
    return count;

create_image_fail:
    m_PrimeDescriptor.num_layers = count;
sync_fail:
    freeEGLImages(dpy, images);
    return -1;
}

void
VAAPIRenderer::freeEGLImages(EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES]) {
    for (size_t i = 0; i < m_PrimeDescriptor.num_layers; ++i) {
        eglDestroyImage(dpy, images[i]);
    }
    for (size_t i = 0; i < m_PrimeDescriptor.num_objects; ++i) {
        close(m_PrimeDescriptor.objects[i].fd);
    }
    m_PrimeDescriptor.num_layers = 0;
    m_PrimeDescriptor.num_objects = 0;
}

#endif
