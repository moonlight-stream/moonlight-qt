// mmap64() for 32-bit off_t systems
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif

#include "drm.h"
#include "string.h"

extern "C" {
    #include <libavutil/hwcontext_drm.h>
    #include <libavutil/pixdesc.h>
}

#include <libdrm/drm_fourcc.h>
#include <linux/dma-buf.h>

// Special Rockchip type
#ifndef DRM_FORMAT_NA12
#define DRM_FORMAT_NA12 fourcc_code('N', 'A', '1', '2')
#endif

// Same as NA12 but upstreamed
#ifndef DRM_FORMAT_NV15
#define DRM_FORMAT_NV15 fourcc_code('N', 'V', '1', '5')
#endif

// Same as NV15 but non-subsampled
#ifndef DRM_FORMAT_NV30
#define DRM_FORMAT_NV30	fourcc_code('N', 'V', '3', '0')
#endif

// Special Raspberry Pi type (upstreamed)
#ifndef DRM_FORMAT_P030
#define DRM_FORMAT_P030	fourcc_code('P', '0', '3', '0')
#endif

// Regular P010 (not present in some old libdrm headers)
#ifndef DRM_FORMAT_P010
#define DRM_FORMAT_P010	fourcc_code('P', '0', '1', '0')
#endif

// Upstreamed YUV444 10-bit
#ifndef DRM_FORMAT_Q410
#define DRM_FORMAT_Q410	fourcc_code('Q', '4', '1', '0')
#endif

// Values for "Colorspace" connector property
#ifndef DRM_MODE_COLORIMETRY_DEFAULT
#define DRM_MODE_COLORIMETRY_DEFAULT     0
#endif
#ifndef DRM_MODE_COLORIMETRY_BT2020_RGB
#define DRM_MODE_COLORIMETRY_BT2020_RGB  9
#endif

#include <unistd.h>
#include <fcntl.h>

#include <sys/mman.h>

#include "streaming/streamutils.h"
#include "streaming/session.h"

#include <Limelight.h>

// HACK: Avoid including X11 headers which conflict with QDir
#ifdef SDL_VIDEO_DRIVER_X11
#undef SDL_VIDEO_DRIVER_X11
#endif

#include <SDL_syswm.h>

#include <QDir>

DrmRenderer::DrmRenderer(AVHWDeviceType hwDeviceType, IFFmpegRenderer *backendRenderer)
    : m_BackendRenderer(backendRenderer),
      m_DrmPrimeBackend(backendRenderer && backendRenderer->canExportDrmPrime()),
      m_HwDeviceType(hwDeviceType),
      m_HwContext(nullptr),
      m_DrmFd(-1),
      m_SdlOwnsDrmFd(false),
      m_SupportsDirectRendering(false),
      m_VideoFormat(0),
      m_ConnectorId(0),
      m_EncoderId(0),
      m_CrtcId(0),
      m_PlaneId(0),
      m_CurrentFbId(0),
      m_LastFullRange(false),
      m_LastColorSpace(-1),
      m_Plane(nullptr),
      m_ColorEncodingProp(nullptr),
      m_ColorRangeProp(nullptr),
      m_HdrOutputMetadataProp(nullptr),
      m_ColorspaceProp(nullptr),
      m_Version(nullptr),
      m_HdrOutputMetadataBlobId(0),
      m_SwFrameMapper(this),
      m_CurrentSwFrameIdx(0)
#ifdef HAVE_EGL
    , m_EglImageFactory(this)
#endif
{
    SDL_zero(m_SwFrame);
}

DrmRenderer::~DrmRenderer()
{
    // Ensure we're out of HDR mode
    setHdrMode(false);

    for (int i = 0; i < k_SwFrameCount; i++) {
        if (m_SwFrame[i].primeFd) {
            close(m_SwFrame[i].primeFd);
        }

        if (m_SwFrame[i].mapping) {
            munmap(m_SwFrame[i].mapping, m_SwFrame[i].size);
        }

        if (m_SwFrame[i].handle) {
            struct drm_mode_destroy_dumb destroyBuf = {};
            destroyBuf.handle = m_SwFrame[i].handle;
            drmIoctl(m_DrmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyBuf);
        }
    }

    if (m_CurrentFbId != 0) {
        drmModeRmFB(m_DrmFd, m_CurrentFbId);
    }

    if (m_HdrOutputMetadataBlobId != 0) {
        drmModeDestroyPropertyBlob(m_DrmFd, m_HdrOutputMetadataBlobId);
    }

    if (m_ColorEncodingProp != nullptr) {
        drmModeFreeProperty(m_ColorEncodingProp);
    }

    if (m_ColorRangeProp != nullptr) {
        drmModeFreeProperty(m_ColorRangeProp);
    }

    if (m_HdrOutputMetadataProp != nullptr) {
        drmModeFreeProperty(m_HdrOutputMetadataProp);
    }

    if (m_ColorspaceProp != nullptr) {
        drmModeFreeProperty(m_ColorspaceProp);
    }

    if (m_Plane != nullptr) {
        drmModeFreePlane(m_Plane);
    }

    if (m_Version != nullptr) {
        drmFreeVersion(m_Version);
    }

    if (m_HwContext != nullptr) {
        av_buffer_unref(&m_HwContext);
    }

    if (!m_SdlOwnsDrmFd && m_DrmFd != -1) {
        close(m_DrmFd);
    }
}

bool DrmRenderer::prepareDecoderContext(AVCodecContext* context, AVDictionary** options)
{
    // The out-of-tree LibreELEC patches use this option to control the type of the V4L2
    // buffers that we get back. We only support NV12 buffers now.
    if(strstr(context->codec->name, "_v4l2") != NULL)
        av_dict_set_int(options, "pixel_format", AV_PIX_FMT_NV12, 0);

    // This option controls the pixel format for the h264_omx and hevc_omx decoders
    // used by the JH7110 multimedia stack. This decoder gives us software frames,
    // so we need a format supported by our DRM dumb buffer code (NV12/NV21/P010).
    //
    // https://doc-en.rvspace.org/VisionFive2/DG_Multimedia/JH7110_SDK/h264_omx.html
    // https://doc-en.rvspace.org/VisionFive2/DG_Multimedia/JH7110_SDK/hevc_omx.html
    av_dict_set(options, "omx_pix_fmt", "nv12", 0);

    if (m_HwDeviceType != AV_HWDEVICE_TYPE_NONE) {
        context->hw_device_ctx = av_buffer_ref(m_HwContext);
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using DRM renderer");

    return true;
}

void DrmRenderer::prepareToRender()
{
    // Create a dummy renderer to force SDL to complete the modesetting
    // operation that the KMSDRM backend keeps pending until the next
    // time we swap buffers. We have to do this before we enumerate
    // CRTC modes below.
    SDL_Renderer* renderer = SDL_CreateRenderer(m_Window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer != nullptr) {
        // SDL_CreateRenderer() can end up having to recreate our window (SDL_RecreateWindow())
        // to ensure it's compatible with the renderer's OpenGL context. If that happens, we
        // can get spurious SDL_WINDOWEVENT events that will cause us to (again) recreate our
        // renderer. This can lead to an infinite to renderer recreation, so discard all
        // SDL_WINDOWEVENT events after SDL_CreateRenderer().
        Session* session = Session::get();
        if (session != nullptr) {
            // If we get here during a session, we need to synchronize with the event loop
            // to ensure we don't drop any important events.
            session->flushWindowEvents();
        }
        else {
            // If we get here prior to the start of a session, just pump and flush ourselves.
            SDL_PumpEvents();
            SDL_FlushEvent(SDL_WINDOWEVENT);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        SDL_DestroyRenderer(renderer);
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
    }
}

bool DrmRenderer::getPropertyByName(drmModeObjectPropertiesPtr props, const char* name, uint64_t *value) {
    for (uint32_t j = 0; j < props->count_props; j++) {
        drmModePropertyPtr prop = drmModeGetProperty(m_DrmFd, props->props[j]);
        if (prop != nullptr) {
            if (!strcmp(prop->name, name)) {
                *value = props->prop_values[j];
                drmModeFreeProperty(prop);
                return true;
            }
            else {
                drmModeFreeProperty(prop);
            }
        }
    }

    return false;
}

bool DrmRenderer::initialize(PDECODER_PARAMETERS params)
{
    int i;

    m_Window = params->window;
    m_VideoFormat = params->videoFormat;
    m_SwFrameMapper.setVideoFormat(params->videoFormat);

#if SDL_VERSION_ATLEAST(2, 0, 15)
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);

    if (!SDL_GetWindowWMInfo(params->window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return false;
    }

    if (info.subsystem == SDL_SYSWM_KMSDRM) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Sharing DRM FD with SDL");

        SDL_assert(info.info.kmsdrm.drm_fd >= 0);
        m_DrmFd = info.info.kmsdrm.drm_fd;
        m_SdlOwnsDrmFd = true;
    }
    else
#endif
    {
        const char* userDevice = SDL_getenv("DRM_DEV");
        if (userDevice != nullptr) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Opening user-specified DRM device: %s",
                        userDevice);

            m_DrmFd = open(userDevice, O_RDWR | O_CLOEXEC);
        }
        else {
            QDir driDir("/dev/dri");

            // We have to explicitly ask for devices to be returned
            driDir.setFilter(QDir::Files | QDir::System);

            // Try a render node first since we aren't using DRM for output in this codepath
            for (QFileInfo& node : driDir.entryInfoList(QStringList("renderD*"))) {
                QByteArray absolutePath = node.absoluteFilePath().toUtf8();
                m_DrmFd = open(absolutePath.constData(), O_RDWR | O_CLOEXEC);
                if (m_DrmFd >= 0) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Opened DRM render node: %s",
                                absolutePath.constData());
                    break;
                }
            }

            // If that fails, try to use a primary node and hope for the best
            if (m_DrmFd < 0) {
                for (QFileInfo& node : driDir.entryInfoList(QStringList("card*"))) {
                    QByteArray absolutePath = node.absoluteFilePath().toUtf8();
                    m_DrmFd = open(absolutePath.constData(), O_RDWR | O_CLOEXEC);
                    if (m_DrmFd >= 0) {
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                    "Opened DRM primary node: %s",
                                    absolutePath.constData());
                        break;
                    }
                }
            }
        }
    }

    // Create the device context first because it is needed whether we can
    // actually use direct rendering or not.
    if (m_HwDeviceType == AV_HWDEVICE_TYPE_DRM) {
        // A real DRM FD is required for DRM-backed hwaccels
        if (m_DrmFd < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to open DRM device: %d",
                         errno);
            return false;
        }

        m_HwContext = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_DRM);
        if (m_HwContext == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "av_hwdevice_ctx_alloc(DRM) failed");
            return false;
        }

        AVHWDeviceContext* deviceContext = (AVHWDeviceContext*)m_HwContext->data;
        AVDRMDeviceContext* drmDeviceContext = (AVDRMDeviceContext*)deviceContext->hwctx;

        drmDeviceContext->fd = m_DrmFd;

        int err = av_hwdevice_ctx_init(m_HwContext);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "av_hwdevice_ctx_init(DRM) failed: %d",
                         err);
            return false;
        }
    }
    else if (m_HwDeviceType != AV_HWDEVICE_TYPE_NONE) {
        // We got some other non-DRM hwaccel that outputs DRM_PRIME frames.
        // Create it with default parameters and hope for the best.
        int err = av_hwdevice_ctx_create(&m_HwContext, m_HwDeviceType, nullptr, nullptr, 0);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "av_hwdevice_ctx_create(%u) failed: %d",
                         m_HwDeviceType,
                         err);
            return false;
        }
    }

    // Still return true if we fail to initialize DRM direct rendering
    // stuff, since we have EGLRenderer and SDLRenderer that we can use
    // for indirect rendering. Our FFmpeg renderer selection code will
    // handle the case where those also fail to render the test frame.
    // If we are just acting as a frontend renderer (m_BackendRenderer
    // == nullptr), we want to fail if we can't render directly since
    // that's the whole point it's trying to use us for.
    const bool DIRECT_RENDERING_INIT_FAILED = (m_BackendRenderer == nullptr);

    if (m_DrmFd < 0) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Direct rendering via DRM is unavailable due to lack of DRM devices");
        return DIRECT_RENDERING_INIT_FAILED;
    }

    // Fetch version details about the DRM driver to use later
    m_Version = drmGetVersion(m_DrmFd);
    if (m_Version == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmGetVersion() failed: %d",
                     errno);
        return DIRECT_RENDERING_INIT_FAILED;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "GPU driver: %s", m_Version->name);

    // If we're not sharing the DRM FD with SDL, that means we don't
    // have DRM master, so we can't call drmModeSetPlane(). We can
    // use EGLRenderer or SDLRenderer to render in this situation.
    if (!m_SdlOwnsDrmFd) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Direct rendering via DRM is disabled");
        return DIRECT_RENDERING_INIT_FAILED;
    }

    drmModeRes* resources = drmModeGetResources(m_DrmFd);
    if (resources == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmModeGetResources() failed: %d",
                     errno);
        return DIRECT_RENDERING_INIT_FAILED;
    }

    // Look for a connected connector and get the associated encoder
    m_ConnectorId = 0;
    m_EncoderId = 0;
    for (i = 0; i < resources->count_connectors && m_EncoderId == 0; i++) {
        drmModeConnector* connector = drmModeGetConnector(m_DrmFd, resources->connectors[i]);
        if (connector != nullptr) {
            if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
                m_ConnectorId = resources->connectors[i];
                m_EncoderId = connector->encoder_id;
            }

            drmModeFreeConnector(connector);
        }
    }

    if (m_EncoderId == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "No connected displays found!");
        drmModeFreeResources(resources);
        return DIRECT_RENDERING_INIT_FAILED;
    }

    // Now find the CRTC from the encoder
    m_CrtcId = 0;
    for (i = 0; i < resources->count_encoders && m_CrtcId == 0; i++) {
        drmModeEncoder* encoder = drmModeGetEncoder(m_DrmFd, resources->encoders[i]);
        if (encoder != nullptr) {
            if (encoder->encoder_id == m_EncoderId) {
                m_CrtcId = encoder->crtc_id;
            }

            drmModeFreeEncoder(encoder);
        }
    }

    if (m_CrtcId == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DRM encoder not found!");
        drmModeFreeResources(resources);
        return DIRECT_RENDERING_INIT_FAILED;
    }

    int crtcIndex = -1;
    for (int i = 0; i < resources->count_crtcs; i++) {
        if (resources->crtcs[i] == m_CrtcId) {
            drmModeCrtc* crtc = drmModeGetCrtc(m_DrmFd, resources->crtcs[i]);
            crtcIndex = i;
            m_OutputRect.x = m_OutputRect.y = 0;
            m_OutputRect.w = crtc->width;
            m_OutputRect.h = crtc->height;
            drmModeFreeCrtc(crtc);
            break;
        }
    }

    drmModeFreeResources(resources);

    if (crtcIndex == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to get CRTC!");
        return DIRECT_RENDERING_INIT_FAILED;
    }

    drmSetClientCap(m_DrmFd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

    drmModePlaneRes* planeRes = drmModeGetPlaneResources(m_DrmFd);
    if (planeRes == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmGetPlaneResources() failed: %d",
                     errno);
        return DIRECT_RENDERING_INIT_FAILED;
    }

    // Find the active plane (if any) on this CRTC with the highest zpos.
    // We'll need to use a plane with a equal or greater zpos to be visible.
    uint64_t maxActiveZpos = qEnvironmentVariableIntValue("DRM_MIN_PLANE_ZPOS");
    for (uint32_t i = 0; i < planeRes->count_planes; i++) {
        drmModePlane* plane = drmModeGetPlane(m_DrmFd, planeRes->planes[i]);
        if (plane != nullptr) {
            if (plane->crtc_id == m_CrtcId) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Plane %u is active on CRTC %u",
                            plane->plane_id,
                            m_CrtcId);

                drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(m_DrmFd, planeRes->planes[i], DRM_MODE_OBJECT_PLANE);
                if (props != nullptr) {
                    // Don't consider cursor planes when searching for the highest active zpos
                    uint64_t type;
                    if (getPropertyByName(props, "type", &type) && (type == DRM_PLANE_TYPE_PRIMARY || type == DRM_PLANE_TYPE_OVERLAY)) {
                        uint64_t zPos;
                        if (getPropertyByName(props, "zpos", &zPos) && zPos > maxActiveZpos) {
                            maxActiveZpos = zPos;
                        }
                    }

                    drmModeFreeObjectProperties(props);
                }
            }

            drmModeFreePlane(plane);
        }
    }

    // The Spacemit K1 driver is broken and advertises support for NV12/P010
    // formats with the linear modifier on all planes, but doesn't actually
    // support raw YUV formats on the primary plane. Don't ever use primary
    // planes on Spacemit hardware to avoid triggering this bug.
    bool ok, allowPrimaryPlane = !!qEnvironmentVariableIntValue("DRM_ALLOW_PRIMARY_PLANE", &ok);
    if (!ok) {
        allowPrimaryPlane = strcmp(m_Version->name, "spacemit") != 0;
    }

    // Find a plane with the required format to render on
    //
    // FIXME: We should check the actual DRM format in a real AVFrame rather
    // than just assuming it will be a certain hardcoded type like NV12 based
    // on the chosen video format.
    for (uint32_t i = 0; i < planeRes->count_planes && !m_PlaneId; i++) {
        drmModePlane* plane = drmModeGetPlane(m_DrmFd, planeRes->planes[i]);
        if (plane != nullptr) {
            // If the plane can't be used on our CRTC, don't consider it further
            if (!(plane->possible_crtcs & (1 << crtcIndex))) {
                drmModeFreePlane(plane);
                continue;
            }

            // We don't check plane->crtc_id here because we want to be able to reuse the primary plane
            // that may owned by Qt and in use on a CRTC prior to us taking over DRM master. When we give
            // control back to Qt, it will repopulate the plane with the FB it owns and render as normal.

            // Validate that the candidate plane supports our pixel format
            bool matchingFormat = false;
            for (uint32_t j = 0; j < plane->count_formats && !matchingFormat; j++) {
                if (m_VideoFormat & VIDEO_FORMAT_MASK_10BIT) {
                    if (m_VideoFormat & VIDEO_FORMAT_MASK_YUV444) {
                        switch (plane->formats[j]) {
                        case DRM_FORMAT_Q410:
                        case DRM_FORMAT_NV30:
                            matchingFormat = true;
                            break;
                        }
                    }
                    else {
                        switch (plane->formats[j]) {
                        case DRM_FORMAT_P010:
                        case DRM_FORMAT_P030:
                        case DRM_FORMAT_NA12:
                        case DRM_FORMAT_NV15:
                            matchingFormat = true;
                            break;
                        }
                    }
                }
                else if (m_VideoFormat & VIDEO_FORMAT_MASK_YUV444) {
                    switch (plane->formats[j]) {
                    case DRM_FORMAT_NV24:
                    case DRM_FORMAT_NV42:
                    case DRM_FORMAT_YUV444:
                        matchingFormat = true;
                        break;
                    }
                }
                else {
                    switch (plane->formats[j]) {
                    case DRM_FORMAT_NV12:
                        matchingFormat = true;
                        break;
                    }
                }
            }

            if (!matchingFormat) {
                drmModeFreePlane(plane);
                continue;
            }

            drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(m_DrmFd, planeRes->planes[i], DRM_MODE_OBJECT_PLANE);
            if (props != nullptr) {
                uint64_t type;
                uint64_t zPos;

                // Only consider overlay and primary (if allowed) planes as valid render targets
                if (!getPropertyByName(props, "type", &type) ||
                        (type != DRM_PLANE_TYPE_OVERLAY && (type != DRM_PLANE_TYPE_PRIMARY || !allowPrimaryPlane))) {
                    drmModeFreePlane(plane);
                }
                // If this plane has a zpos property and it's lower (further from user) than
                // the highest active plane we found, avoid this plane. It won't be visible.
                //
                // Note: zpos is not a required property, but if any plane has it, all planes must.
                else if (getPropertyByName(props, "zpos", &zPos) && zPos < maxActiveZpos) {
                    drmModeFreePlane(plane);
                }
                else {
                    SDL_assert(!m_PlaneId);
                    SDL_assert(!m_Plane);

                    m_PlaneId = plane->plane_id;
                    m_Plane = plane;
                }

                drmModeFreeObjectProperties(props);
            }
        }
    }

    drmModeFreePlaneResources(planeRes);

    if (m_PlaneId == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to find suitable primary/overlay plane!");
        return DIRECT_RENDERING_INIT_FAILED;
    }

    // Populate plane properties
    {
        drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(m_DrmFd, m_PlaneId, DRM_MODE_OBJECT_PLANE);
        if (props != nullptr) {
            for (uint32_t j = 0; j < props->count_props; j++) {
                drmModePropertyPtr prop = drmModeGetProperty(m_DrmFd, props->props[j]);
                if (prop != nullptr) {
                    if (!strcmp(prop->name, "COLOR_ENCODING")) {
                        m_ColorEncodingProp = prop;
                    }
                    else if (!strcmp(prop->name, "COLOR_RANGE")) {
                        m_ColorRangeProp = prop;
                    }
                    else {
                        drmModeFreeProperty(prop);
                    }
                }
            }

            drmModeFreeObjectProperties(props);
        }
    }

    // Populate connector properties
    {
        drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(m_DrmFd, m_ConnectorId, DRM_MODE_OBJECT_CONNECTOR);
        if (props != nullptr) {
            for (uint32_t j = 0; j < props->count_props; j++) {
                drmModePropertyPtr prop = drmModeGetProperty(m_DrmFd, props->props[j]);
                if (prop != nullptr) {
                    if (!strcmp(prop->name, "HDR_OUTPUT_METADATA")) {
                        m_HdrOutputMetadataProp = prop;
                    }
                    else if (!strcmp(prop->name, "Colorspace")) {
                        m_ColorspaceProp = prop;
                    }
                    else if (!strcmp(prop->name, "max bpc") && (m_VideoFormat & VIDEO_FORMAT_MASK_10BIT)) {
                        if (drmModeObjectSetProperty(m_DrmFd, m_ConnectorId, DRM_MODE_OBJECT_CONNECTOR, prop->prop_id, 16) == 0) {
                            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                        "Enabled 48-bit HDMI Deep Color");
                        }
                        else if (drmModeObjectSetProperty(m_DrmFd, m_ConnectorId, DRM_MODE_OBJECT_CONNECTOR, prop->prop_id, 12) == 0) {
                            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                        "Enabled 36-bit HDMI Deep Color");
                        }
                        else if (drmModeObjectSetProperty(m_DrmFd, m_ConnectorId, DRM_MODE_OBJECT_CONNECTOR, prop->prop_id, 10) == 0) {
                            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                        "Enabled 30-bit HDMI Deep Color");
                        }
                        else {
                            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                         "drmModeObjectSetProperty(%s) failed: %d",
                                         prop->name,
                                         errno);
                            // Non-fatal
                        }

                        drmModeFreeProperty(prop);
                    }
                    else {
                        drmModeFreeProperty(prop);
                    }
                }
            }

            drmModeFreeObjectProperties(props);
        }
    }

    // If we got this far, we can do direct rendering via the DRM FD.
    m_SupportsDirectRendering = true;

    return true;
}

enum AVPixelFormat DrmRenderer::getPreferredPixelFormat(int videoFormat)
{
    // DRM PRIME buffers, or whatever the backend renderer wants
    if (m_BackendRenderer != nullptr) {
        return m_BackendRenderer->getPreferredPixelFormat(videoFormat);
    }
    else {
        // We must return this pixel format to ensure it's used with
        // v4l2m2m decoders that go through non-hwaccel format selection.
        //
        // For non-hwaccel decoders that don't support DRM PRIME, ffGetFormat()
        // will call isPixelFormatSupported() and pick a supported swformat.
        return AV_PIX_FMT_DRM_PRIME;
    }
}

bool DrmRenderer::isPixelFormatSupported(int videoFormat, AVPixelFormat pixelFormat) {
    if (m_HwDeviceType != AV_HWDEVICE_TYPE_NONE) {
        return pixelFormat == AV_PIX_FMT_DRM_PRIME;
    }
    else if (m_DrmPrimeBackend) {
        return m_BackendRenderer->isPixelFormatSupported(videoFormat, pixelFormat);
    }
    else {
        // If we're going to need to map this as a software frame, check
        // against the set of formats we support in mapSoftwareFrame().
        if (pixelFormat == AV_PIX_FMT_DRM_PRIME) {
            // AV_PIX_FMT_DRM_PRIME is always supported
            return true;
        }
        else if (videoFormat & VIDEO_FORMAT_MASK_10BIT) {
            if (videoFormat & VIDEO_FORMAT_MASK_YUV444) {
                switch (pixelFormat) {
                case AV_PIX_FMT_YUV444P10:
                    return true;
                default:
                    return false;
                }
            }
            else {
                switch (pixelFormat) {
                case AV_PIX_FMT_P010:
                    return true;
                default:
                    return false;
                }
            }
        }
        else if (videoFormat & VIDEO_FORMAT_MASK_YUV444) {
            switch (pixelFormat) {
            case AV_PIX_FMT_NV24:
            case AV_PIX_FMT_NV42:
            case AV_PIX_FMT_YUV444P:
            case AV_PIX_FMT_YUVJ444P:
                return true;
            default:
                return false;
            }
        }
        else {
            switch (pixelFormat) {
            case AV_PIX_FMT_NV12:
            case AV_PIX_FMT_NV21:
            case AV_PIX_FMT_YUV420P:
            case AV_PIX_FMT_YUVJ420P:
                return true;
            default:
                return false;
            }
        }
    }
}

int DrmRenderer::getRendererAttributes()
{
    int attributes = 0;

    // This renderer can only draw in full-screen
    attributes |= RENDERER_ATTRIBUTE_FULLSCREEN_ONLY;

    // This renderer supports HDR
    attributes |= RENDERER_ATTRIBUTE_HDR_SUPPORT;

    // This renderer does not buffer any frames in the graphics pipeline
    attributes |= RENDERER_ATTRIBUTE_NO_BUFFERING;

#ifdef GL_IS_SLOW
    // Restrict streaming resolution to 1080p on the Pi 4 while in the desktop environment.
    // EGL performance is extremely poor and just barely hits 1080p60 on Bookworm. This also
    // covers the MMAL H.264 case which maxes out at 1080p60 too.
    if (!m_SupportsDirectRendering && m_Version &&
            (strcmp(m_Version->name, "vc4") == 0 || strcmp(m_Version->name, "v3d") == 0) &&
            qgetenv("RPI_ALLOW_EGL_4K") != "1") {
        drmDevicePtr device;

        if (drmGetDevice(m_DrmFd, &device) == 0) {
            if (device->bustype == DRM_BUS_PLATFORM) {
                for (int i = 0; device->deviceinfo.platform->compatible[i]; i++) {
                    QString compatibleId(device->deviceinfo.platform->compatible[i]);
                    if (compatibleId == "brcm,bcm2835-vc4" || compatibleId == "brcm,bcm2711-vc5" || compatibleId == "brcm,2711-v3d") {
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                    "Streaming resolution is limited to 1080p on the Pi 4 inside the desktop environment!");
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                    "Run Moonlight directly from the console to stream above 1080p resolution!");
                        attributes |= RENDERER_ATTRIBUTE_1080P_MAX;
                        break;
                    }
                }
            }

            drmFreeDevice(&device);
        }
    }
#endif

    return attributes;
}

void DrmRenderer::setHdrMode(bool enabled)
{
    if (m_ColorspaceProp != nullptr) {
        int err = drmModeObjectSetProperty(m_DrmFd, m_ConnectorId, DRM_MODE_OBJECT_CONNECTOR,
                                           m_ColorspaceProp->prop_id,
                                           enabled ? DRM_MODE_COLORIMETRY_BT2020_RGB : DRM_MODE_COLORIMETRY_DEFAULT);
        if (err == 0) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Set HDMI Colorspace: %s",
                        enabled ? "BT.2020 RGB" : "Default");
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "drmModeObjectSetProperty(%s) failed: %d",
                         m_ColorspaceProp->name,
                         errno);
            // Non-fatal
        }
    }

    if (m_HdrOutputMetadataProp != nullptr) {
        if (m_HdrOutputMetadataBlobId != 0) {
            drmModeDestroyPropertyBlob(m_DrmFd, m_HdrOutputMetadataBlobId);
            m_HdrOutputMetadataBlobId = 0;
        }

        if (enabled) {
            DrmDefs::hdr_output_metadata outputMetadata;
            SS_HDR_METADATA sunshineHdrMetadata;

            // Sunshine will have HDR metadata but GFE will not
            if (!LiGetHdrMetadata(&sunshineHdrMetadata)) {
                memset(&sunshineHdrMetadata, 0, sizeof(sunshineHdrMetadata));
            }

            outputMetadata.metadata_type = 0; // HDMI_STATIC_METADATA_TYPE1
            outputMetadata.hdmi_metadata_type1.eotf = 2; // SMPTE ST 2084
            outputMetadata.hdmi_metadata_type1.metadata_type = 0; // Static Metadata Type 1
            for (int i = 0; i < 3; i++) {
                outputMetadata.hdmi_metadata_type1.display_primaries[i].x = sunshineHdrMetadata.displayPrimaries[i].x;
                outputMetadata.hdmi_metadata_type1.display_primaries[i].y = sunshineHdrMetadata.displayPrimaries[i].y;
            }
            outputMetadata.hdmi_metadata_type1.white_point.x = sunshineHdrMetadata.whitePoint.x;
            outputMetadata.hdmi_metadata_type1.white_point.y = sunshineHdrMetadata.whitePoint.y;
            outputMetadata.hdmi_metadata_type1.max_display_mastering_luminance = sunshineHdrMetadata.maxDisplayLuminance;
            outputMetadata.hdmi_metadata_type1.min_display_mastering_luminance = sunshineHdrMetadata.minDisplayLuminance;
            outputMetadata.hdmi_metadata_type1.max_cll = sunshineHdrMetadata.maxContentLightLevel;
            outputMetadata.hdmi_metadata_type1.max_fall = sunshineHdrMetadata.maxFrameAverageLightLevel;

            int err = drmModeCreatePropertyBlob(m_DrmFd, &outputMetadata, sizeof(outputMetadata), &m_HdrOutputMetadataBlobId);
            if (err < 0) {
                m_HdrOutputMetadataBlobId = 0;
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "drmModeCreatePropertyBlob() failed: %d",
                             errno);
                // Non-fatal
            }
        }

        int err = drmModeObjectSetProperty(m_DrmFd, m_ConnectorId, DRM_MODE_OBJECT_CONNECTOR,
                                           m_HdrOutputMetadataProp->prop_id,
                                           enabled ? m_HdrOutputMetadataBlobId : 0);
        if (err == 0) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Set display HDR mode: %s", enabled ? "enabled" : "disabled");
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "drmModeObjectSetProperty(%s) failed: %d",
                         m_HdrOutputMetadataProp->name,
                         errno);
            // Non-fatal
        }
    }
    else if (enabled) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "HDR_OUTPUT_METADATA is unavailable on this display. Unable to enter HDR mode!");
    }
}

bool DrmRenderer::mapSoftwareFrame(AVFrame *frame, AVDRMFrameDescriptor *mappedFrame)
{
    bool ret = false;
    bool freeFrame;
    auto drmFrame = &m_SwFrame[m_CurrentSwFrameIdx];

    SDL_assert(frame->format != AV_PIX_FMT_DRM_PRIME);
    SDL_assert(!m_DrmPrimeBackend);

    // If this is a non-DRM hwframe that cannot be exported to DRM format, we must
    // use the SwFrameMapper to map it to a swframe before we can copy it to dumb buffers.
    if (frame->hw_frames_ctx != nullptr) {
        frame = m_SwFrameMapper.getSwFrameFromHwFrame(frame);
        if (frame == nullptr) {
            return false;
        }

        freeFrame = true;
    }
    else {
        freeFrame = false;
    }

    const AVPixFmtDescriptor* formatDesc = av_pix_fmt_desc_get((AVPixelFormat) frame->format);
    int planes = av_pix_fmt_count_planes((AVPixelFormat) frame->format);

    uint32_t drmFormat;

    // NB: Keep this list updated with isPixelFormatSupported()
    switch (frame->format) {
    case AV_PIX_FMT_NV12:
        drmFormat = DRM_FORMAT_NV12;
        break;
    case AV_PIX_FMT_NV21:
        drmFormat = DRM_FORMAT_NV21;
        break;
    case AV_PIX_FMT_P010:
        drmFormat = DRM_FORMAT_P010;
        break;
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
        drmFormat = DRM_FORMAT_YUV420;
        break;
    case AV_PIX_FMT_NV24:
        drmFormat = DRM_FORMAT_NV24;
        break;
    case AV_PIX_FMT_NV42:
        drmFormat = DRM_FORMAT_NV42;
        break;
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
        drmFormat = DRM_FORMAT_YUV444;
        break;
    case AV_PIX_FMT_YUV444P10:
        drmFormat = DRM_FORMAT_Q410;
        break;
    default:
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to map frame with unsupported format: %d",
                     frame->format);
        goto Exit;
    }

    // Create a new dumb buffer if needed
    if (!drmFrame->handle) {
        struct drm_mode_create_dumb createBuf = {};

        createBuf.width = frame->width;
        createBuf.height = frame->height;
        createBuf.bpp = formatDesc->comp[0].step * 8;

        // For planar formats, we need to add additional space to the "height"
        // of the dumb buffer to account for the chroma plane(s). Chroma for
        // packed formats is already covered by the bpp value since the step
        // value of the Y component will also include the space for chroma
        // since it's all packed into a single plane.
        if (planes > 1) {
            createBuf.height += (2 * AV_CEIL_RSHIFT(frame->height, formatDesc->log2_chroma_h));
        }

        int err = drmIoctl(m_DrmFd, DRM_IOCTL_MODE_CREATE_DUMB, &createBuf);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "DRM_IOCTL_MODE_CREATE_DUMB failed: %d",
                         errno);
            goto Exit;
        }

        drmFrame->handle = createBuf.handle;
        drmFrame->pitch = createBuf.pitch;
        drmFrame->size = createBuf.size;
    }

    // Map the dumb buffer if needed
    if (!drmFrame->mapping) {
        struct drm_mode_map_dumb mapBuf = {};
        mapBuf.handle = drmFrame->handle;

        int err = drmIoctl(m_DrmFd, DRM_IOCTL_MODE_MAP_DUMB, &mapBuf);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "DRM_IOCTL_MODE_MAP_DUMB failed: %d",
                         errno);
            goto Exit;
        }

        // Raspberry Pi on kernel 6.1 defaults to an aarch64 kernel with a 32-bit userspace (and off_t).
        // This leads to issues when DRM_IOCTL_MODE_MAP_DUMB returns a > 4GB offset. The high bits are
        // chopped off when passed via the normal mmap() call using 32-bit off_t. We avoid this issue
        // by explicitly calling mmap64() to ensure the 64-bit offset is never truncated.
#if defined(__GLIBC__) && QT_POINTER_SIZE == 4
        drmFrame->mapping = (uint8_t*)mmap64(nullptr, drmFrame->size, PROT_WRITE, MAP_SHARED, m_DrmFd, mapBuf.offset);
#else
        drmFrame->mapping = (uint8_t*)mmap(nullptr, drmFrame->size, PROT_WRITE, MAP_SHARED, m_DrmFd, mapBuf.offset);
#endif
        if (drmFrame->mapping == MAP_FAILED) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "mmap() failed for dumb buffer: %d",
                         errno);
            goto Exit;
        }
    }

    // Convert this buffer handle to a FD if needed
    if (!drmFrame->primeFd) {
        int err = drmPrimeHandleToFD(m_DrmFd, drmFrame->handle, O_CLOEXEC, &drmFrame->primeFd);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "drmPrimeHandleToFD() failed: %d",
                         errno);
            goto Exit;
        }
    }

    {
        // Construct the AVDRMFrameDescriptor and copy our frame data into the dumb buffer
        SDL_zerop(mappedFrame);

        // We use a single dumb buffer for semi/fully planar formats because some DRM
        // drivers (i915, at least) don't support multi-buffer FBs.
        mappedFrame->nb_objects = 1;
        mappedFrame->objects[0].fd = drmFrame->primeFd;
        mappedFrame->objects[0].format_modifier = DRM_FORMAT_MOD_LINEAR;
        mappedFrame->objects[0].size = drmFrame->size;

        mappedFrame->nb_layers = 1;

        auto &layer = mappedFrame->layers[0];
        layer.format = drmFormat;

        // Prepare to write to the dumb buffer from the CPU
        struct dma_buf_sync sync;
        sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE;
        drmIoctl(drmFrame->primeFd, DMA_BUF_IOCTL_SYNC, &sync);

        int lastPlaneSize = 0;
        for (int i = 0; i < 4; i++) {
            if (frame->data[i] != nullptr) {
                auto &plane = layer.planes[layer.nb_planes];

                plane.object_index = 0;
                plane.offset = i == 0 ? 0 : (layer.planes[layer.nb_planes - 1].offset + lastPlaneSize);

                int planeHeight;
                if (i == 0) {
                    // Y plane is not subsampled
                    planeHeight = frame->height;
                    plane.pitch = drmFrame->pitch;
                }
                else {
                    planeHeight = AV_CEIL_RSHIFT(frame->height, formatDesc->log2_chroma_h);

                    // First argument to AV_CEIL_RSHIFT() *must* be signed for correct behavior!
                    plane.pitch = AV_CEIL_RSHIFT((ptrdiff_t)drmFrame->pitch, formatDesc->log2_chroma_w);

                    // If UV planes are interleaved, double the pitch to count both U+V together
                    if (planes == 2) {
                        plane.pitch <<= 1;
                    }
                }

                // Copy the plane data into the dumb buffer
                if (frame->linesize[i] == (int)plane.pitch) {
                    // We can do a single memcpy() if the pitch is compatible
                    memcpy(drmFrame->mapping + plane.offset,
                           frame->data[i],
                           frame->linesize[i] * planeHeight);
                }
                else {
                    // The pitch is incompatible, so we must copy line-by-line
                    for (int j = 0; j < planeHeight; j++) {
                        memcpy(drmFrame->mapping + (j * plane.pitch) + plane.offset,
                               frame->data[i] + (j * frame->linesize[i]),
                               qMin(frame->linesize[i], (int)plane.pitch));
                    }
                }

                layer.nb_planes++;

                lastPlaneSize = plane.pitch * planeHeight;
            }
        }

        // End the CPU write to the dumb buffer
        sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE;
        drmIoctl(drmFrame->primeFd, DMA_BUF_IOCTL_SYNC, &sync);
    }

    ret = true;
    m_CurrentSwFrameIdx = (m_CurrentSwFrameIdx + 1) % k_SwFrameCount;

Exit:
    if (freeFrame) {
        av_frame_free(&frame);
    }

    return ret;
}

bool DrmRenderer::addFbForFrame(AVFrame *frame, uint32_t* newFbId, bool testMode)
{
    AVDRMFrameDescriptor mappedFrame;
    AVDRMFrameDescriptor* drmFrame;
    int err;

    // If we don't have a DRM PRIME frame here, we'll need to map into one
    if (frame->format != AV_PIX_FMT_DRM_PRIME) {
        if (m_DrmPrimeBackend) {
            // If the backend supports DRM PRIME directly, use that.
            if (!m_BackendRenderer->mapDrmPrimeFrame(frame, &mappedFrame)) {
                return false;
            }
        }
        else {
            // Otherwise, we'll map it to a software format and use dumb buffers
            if (!mapSoftwareFrame(frame, &mappedFrame)) {
                return false;
            }
        }

        drmFrame = &mappedFrame;
    }
    else {
        SDL_assert(frame->format == AV_PIX_FMT_DRM_PRIME);
        drmFrame = (AVDRMFrameDescriptor*)frame->data[0];
    }

    uint32_t handles[4] = {};
    uint32_t pitches[4] = {};
    uint32_t offsets[4] = {};
    uint64_t modifiers[4] = {};
    uint32_t flags = 0;

    // DRM requires composed layers rather than separate layers per plane
    SDL_assert(drmFrame->nb_layers == 1);

    const auto &layer = drmFrame->layers[0];
    for (int i = 0; i < layer.nb_planes; i++) {
        const auto &object = drmFrame->objects[layer.planes[i].object_index];

        err = drmPrimeFDToHandle(m_DrmFd, object.fd, &handles[i]);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "drmPrimeFDToHandle() failed: %d",
                         errno);
            if (m_DrmPrimeBackend) {
                SDL_assert(drmFrame == &mappedFrame);
                m_BackendRenderer->unmapDrmPrimeFrame(drmFrame);
            }
            return false;
        }

        pitches[i] = layer.planes[i].pitch;
        offsets[i] = layer.planes[i].offset;
        modifiers[i] = object.format_modifier;

        // Pass along the modifiers to DRM if there are some in the descriptor
        if (modifiers[i] != DRM_FORMAT_MOD_INVALID) {
            flags |= DRM_MODE_FB_MODIFIERS;
        }
    }

    // Create a frame buffer object from the PRIME buffer
    // NB: It is an error to pass modifiers without DRM_MODE_FB_MODIFIERS set.
    err = drmModeAddFB2WithModifiers(m_DrmFd, frame->width, frame->height,
                                     drmFrame->layers[0].format,
                                     handles, pitches, offsets,
                                     (flags & DRM_MODE_FB_MODIFIERS) ? modifiers : NULL,
                                     newFbId, flags);

    if (m_DrmPrimeBackend) {
        SDL_assert(drmFrame == &mappedFrame);
        m_BackendRenderer->unmapDrmPrimeFrame(drmFrame);
    }

    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmModeAddFB2[WithModifiers]() failed: %d",
                     errno);
        return false;
    }

    if (testMode) {
        // Check if plane can actually be imported
        for (uint32_t i = 0; i < m_Plane->count_formats; i++) {
            if (drmFrame->layers[0].format == m_Plane->formats[i]) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Selected DRM plane supports chosen decoding format: %08x",
                            drmFrame->layers[0].format);
                return true;
            }
        }

        // TODO: We can also check the modifier support using the IN_FORMATS property,
        // but checking format alone is probably enough for real world cases since we're
        // either getting linear buffers from software mapping or DMA-BUFs from the
        // hardware decoder.
        //
        // Hopefully no actual hardware vendors are dumb enough to ship display hardware
        // or drivers that lack support for the format modifiers required by their own
        // video decoders.

        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Selected DRM plane doesn't support chosen decoding format: %08x",
                     drmFrame->layers[0].format);
        drmModeRmFB(m_DrmFd, *newFbId);
        return false;
    }
    else {
        return true;
    }
}

void DrmRenderer::renderFrame(AVFrame* frame)
{
    int err;
    SDL_Rect src, dst;

    src.x = src.y = 0;
    src.w = frame->width;
    src.h = frame->height;
    dst = m_OutputRect;

    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    // Remember the last FB object we created so we can free it
    // when we are finished rendering this one (if successful).
    uint32_t lastFbId = m_CurrentFbId;

    // Register a frame buffer object for this frame
    if (!addFbForFrame(frame, &m_CurrentFbId, false)) {
        m_CurrentFbId = lastFbId;
        return;
    }

    int colorspace = getFrameColorspace(frame);
    bool fullRange = isFrameFullRange(frame);

    // We also update the color range when the colorspace changes in order to handle initialization
    // where the last color range value may not actual be applied to the plane.
    if (fullRange != m_LastFullRange || colorspace != m_LastColorSpace) {
        const char* desiredValue = getDrmColorRangeValue(frame);

        if (m_ColorRangeProp != nullptr && desiredValue != nullptr) {
            int i;

            for (i = 0; i < m_ColorRangeProp->count_enums; i++) {
                if (!strcmp(desiredValue, m_ColorRangeProp->enums[i].name)) {
                    err = drmModeObjectSetProperty(m_DrmFd, m_PlaneId, DRM_MODE_OBJECT_PLANE,
                                                   m_ColorRangeProp->prop_id, m_ColorRangeProp->enums[i].value);
                    if (err == 0) {
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                    "%s: %s",
                                    m_ColorRangeProp->name,
                                    desiredValue);
                    }
                    else {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                     "drmModeObjectSetProperty(%s) failed: %d",
                                     m_ColorRangeProp->name,
                                     errno);
                        // Non-fatal
                    }

                    break;
                }
            }

            if (i == m_ColorRangeProp->count_enums) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Unable to find matching COLOR_RANGE value for '%s'. Colors may be inaccurate!",
                            desiredValue);
            }
        }
        else if (desiredValue != nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "COLOR_RANGE property does not exist on output plane. Colors may be inaccurate!");
        }

        m_LastFullRange = fullRange;
    }

    if (colorspace != m_LastColorSpace) {
        const char* desiredValue = getDrmColorEncodingValue(frame);

        if (m_ColorEncodingProp != nullptr && desiredValue != nullptr) {
            int i;

            for (i = 0; i < m_ColorEncodingProp->count_enums; i++) {
                if (!strcmp(desiredValue, m_ColorEncodingProp->enums[i].name)) {
                    err = drmModeObjectSetProperty(m_DrmFd, m_PlaneId, DRM_MODE_OBJECT_PLANE,
                                                   m_ColorEncodingProp->prop_id, m_ColorEncodingProp->enums[i].value);
                    if (err == 0) {
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                    "%s: %s",
                                    m_ColorEncodingProp->name,
                                    desiredValue);
                    }
                    else {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                     "drmModeObjectSetProperty(%s) failed: %d",
                                     m_ColorEncodingProp->name,
                                     errno);
                        // Non-fatal
                    }

                    break;
                }
            }

            if (i == m_ColorEncodingProp->count_enums) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Unable to find matching COLOR_ENCODING value for '%s'. Colors may be inaccurate!",
                            desiredValue);
            }
        }
        else if (desiredValue != nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "COLOR_ENCODING property does not exist on output plane. Colors may be inaccurate!");
        }

        m_LastColorSpace = colorspace;
    }

    // Update the overlay
    err = drmModeSetPlane(m_DrmFd, m_PlaneId, m_CrtcId, m_CurrentFbId, 0,
                          dst.x, dst.y,
                          dst.w, dst.h,
                          0, 0,
                          frame->width << 16,
                          frame->height << 16);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmModeSetPlane() failed: %d",
                     errno);
        drmModeRmFB(m_DrmFd, m_CurrentFbId);
        m_CurrentFbId = lastFbId;
        return;
    }

    // Free the previous FB object which has now been superseded
    drmModeRmFB(m_DrmFd, lastFbId);
}

bool DrmRenderer::needsTestFrame()
{
    return true;
}

bool DrmRenderer::testRenderFrame(AVFrame* frame) {
    uint32_t fbId;

    // If we don't even have a plane, we certainly can't render
    if (!m_Plane) {
        return false;
    }

    // Ensure we can export DRM PRIME frames (if applicable) and
    // add a FB object with the provided DRM format. Ask for the
    // extended validation to ensure the chosen plane supports
    // the format too.
    if (!addFbForFrame(frame, &fbId, true)) {
        return false;
    }

    drmModeRmFB(m_DrmFd, fbId);
    return true;
}

bool DrmRenderer::isDirectRenderingSupported()
{
    return m_SupportsDirectRendering;
}

int DrmRenderer::getDecoderColorspace()
{
    if (m_ColorEncodingProp != nullptr) {
        // Search for a COLOR_ENCODING property that fits a value we support
        for (int i = 0; i < m_ColorEncodingProp->count_enums; i++) {
            if (!strcmp(m_ColorEncodingProp->enums[i].name, "ITU-R BT.601 YCbCr")) {
                return COLORSPACE_REC_601;
            }
            else if (!strcmp(m_ColorEncodingProp->enums[i].name, "ITU-R BT.709 YCbCr")) {
                return COLORSPACE_REC_709;
            }
        }
    }

    // Default to BT.601 if we couldn't find a valid COLOR_ENCODING property
    return COLORSPACE_REC_601;
}

const char* DrmRenderer::getDrmColorEncodingValue(AVFrame* frame)
{
    switch (getFrameColorspace(frame)) {
    case COLORSPACE_REC_601:
        return "ITU-R BT.601 YCbCr";
    case COLORSPACE_REC_709:
        return "ITU-R BT.709 YCbCr";
    case COLORSPACE_REC_2020:
        return "ITU-R BT.2020 YCbCr";
    default:
        return NULL;
    }
}

const char* DrmRenderer::getDrmColorRangeValue(AVFrame* frame)
{
    return isFrameFullRange(frame) ? "YCbCr full range" : "YCbCr limited range";
}

#ifdef HAVE_EGL

bool DrmRenderer::canExportEGL() {
    if (qgetenv("DRM_FORCE_DIRECT") == "1") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Using direct rendering due to environment variable");
        return false;
    }
    else if (qgetenv("DRM_FORCE_EGL") == "1") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Using EGL rendering due to environment variable");
        return true;
    }
    else if (m_SupportsDirectRendering && (m_VideoFormat & VIDEO_FORMAT_MASK_10BIT)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Using direct rendering for HDR support");
        return false;
    }

#if defined(HAVE_MMAL) && !defined(ALLOW_EGL_WITH_MMAL)
    // EGL rendering is so slow on the Raspberry Pi 4 that we should basically
    // never use it. It is suitable for 1080p 30 FPS on a good day, and much
    // much less than that if you decide to do something crazy like stream
    // in full-screen. MMAL is the ideal rendering API for Buster and Bullseye,
    // but it's gone in Bookworm. Fortunately, Bookworm has a more efficient
    // rendering pipeline that makes EGL mostly usable as long as we stick
    // to a 1080p 60 FPS maximum.
    if (qgetenv("RPI_ALLOW_EGL_RENDER") != "1") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Disabling EGL rendering due to low performance on Raspberry Pi 4");
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Set RPI_ALLOW_EGL_RENDER=1 to override");
        return false;
    }
#endif

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "DRM backend supports exporting EGLImage");
    return true;
}

AVPixelFormat DrmRenderer::getEGLImagePixelFormat() {
    // This tells EGLRenderer to treat the EGLImage as a single opaque texture
    return AV_PIX_FMT_DRM_PRIME;
}

bool DrmRenderer::initializeEGL(EGLDisplay display,
                                const EGLExtensions &ext) {
    return m_EglImageFactory.initializeEGL(display, ext);
}

ssize_t DrmRenderer::exportEGLImages(AVFrame *frame, EGLDisplay dpy,
                                     EGLImage images[EGL_MAX_PLANES]) {
    if (frame->format != AV_PIX_FMT_DRM_PRIME) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "EGLImage export requires hardware-backed frames");
        return -1;
    }

    AVDRMFrameDescriptor* drmFrame = (AVDRMFrameDescriptor*)frame->data[0];
    return m_EglImageFactory.exportDRMImages(frame, drmFrame, dpy, images);
}

void DrmRenderer::freeEGLImages(EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES]) {
    m_EglImageFactory.freeEGLImages(dpy, images);
}

#endif
