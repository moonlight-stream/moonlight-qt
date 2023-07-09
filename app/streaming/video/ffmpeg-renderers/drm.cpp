#include "drm.h"

extern "C" {
    #include <libavutil/hwcontext_drm.h>
}

#include <libdrm/drm_fourcc.h>

// Special Rockchip type
#ifndef DRM_FORMAT_NA12
#define DRM_FORMAT_NA12 fourcc_code('N', 'A', '1', '2')
#endif

// Same as NA12 but upstreamed
#ifndef DRM_FORMAT_NV15
#define DRM_FORMAT_NV15 fourcc_code('N', 'V', '1', '5')
#endif

// Special Raspberry Pi type (upstreamed)
#ifndef DRM_FORMAT_P030
#define DRM_FORMAT_P030	fourcc_code('P', '0', '3', '0')
#endif

// Regular P010 (not present in some old libdrm headers)
#ifndef DRM_FORMAT_P010
#define DRM_FORMAT_P010	fourcc_code('P', '0', '1', '0')
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

DrmRenderer::DrmRenderer(bool hwaccel, IFFmpegRenderer *backendRenderer)
    : m_BackendRenderer(backendRenderer),
      m_DrmPrimeBackend(backendRenderer && backendRenderer->canExportDrmPrime()),
      m_HwAccelBackend(hwaccel),
      m_HwContext(nullptr),
      m_DrmFd(-1),
      m_SdlOwnsDrmFd(false),
      m_SupportsDirectRendering(false),
      m_Main10Hdr(false),
      m_ConnectorId(0),
      m_EncoderId(0),
      m_CrtcId(0),
      m_PlaneId(0),
      m_CurrentFbId(0),
      m_LastFullRange(false),
      m_LastColorSpace(-1),
      m_ColorEncodingProp(nullptr),
      m_ColorRangeProp(nullptr),
      m_HdrOutputMetadataProp(nullptr),
      m_HdrOutputMetadataBlobId(0),
      m_SwFrameMapper(this),
      m_CurrentSwFrameIdx(0)
{
#ifdef HAVE_EGL
    m_EGLExtDmaBuf = false;
    m_eglCreateImage = nullptr;
    m_eglCreateImageKHR = nullptr;
    m_eglDestroyImage = nullptr;
    m_eglDestroyImageKHR = nullptr;
#endif

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
    av_dict_set_int(options, "pixel_format", AV_PIX_FMT_NV12, 0);

    // This option controls the pixel format for the h264_omx and hevc_omx decoders
    // used by the JH7110 multimedia stack. This decoder gives us software frames,
    // so we need a format supported by our DRM dumb buffer code (NV12/NV21/P010).
    //
    // https://doc-en.rvspace.org/VisionFive2/DG_Multimedia/JH7110_SDK/h264_omx.html
    // https://doc-en.rvspace.org/VisionFive2/DG_Multimedia/JH7110_SDK/hevc_omx.html
    av_dict_set(options, "omx_pix_fmt", "nv12", 0);

    if (m_HwAccelBackend) {
        context->hw_device_ctx = av_buffer_ref(m_HwContext);
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using DRM renderer");

    return true;
}

bool DrmRenderer::initialize(PDECODER_PARAMETERS params)
{
    int i;

    m_Main10Hdr = (params->videoFormat & VIDEO_FORMAT_MASK_10BIT);
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

        if (m_DrmFd < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to open DRM device: %d",
                          errno);
            return false;
        }
    }

    // Create the device context first because it is needed whether we can
    // actually use direct rendering or not.
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

    // Still return true if we fail to initialize DRM direct rendering
    // stuff, since we have EGLRenderer and SDLRenderer that we can use
    // for indirect rendering. Our FFmpeg renderer selection code will
    // handle the case where those also fail to render the test frame.
    // If we are just acting as a frontend renderer (m_BackendRenderer
    // == nullptr), we want to fail if we can't render directly since
    // that's the whole point it's trying to use us for.
    const bool DIRECT_RENDERING_INIT_FAILED = (m_BackendRenderer == nullptr);

    // If we're not sharing the DRM FD with SDL, that means we don't
    // have DRM master, so we can't call drmModeSetPlane(). We can
    // use EGLRenderer or SDLRenderer to render in this situation.
    if (!m_SdlOwnsDrmFd) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Direct rendering via DRM is disabled");
        return DIRECT_RENDERING_INIT_FAILED;
    }

    if (!params->testOnly) {
        // Create a dummy renderer to force SDL to complete the modesetting
        // operation that the KMSDRM backend keeps pending until the next
        // time we swap buffers. We have to do this before we enumerate
        // CRTC modes below.
        SDL_Renderer* renderer = SDL_CreateRenderer(params->window, -1, SDL_RENDERER_SOFTWARE);
        if (renderer != nullptr) {
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

    // Find a plane with the required format to render on
    //
    // FIXME: We should check the actual DRM format in a real AVFrame rather
    // than just assuming it will be a certain hardcoded type like NV12 based
    // on the chosen video format.
    m_PlaneId = 0;
    for (uint32_t i = 0; i < planeRes->count_planes && m_PlaneId == 0; i++) {
        drmModePlane* plane = drmModeGetPlane(m_DrmFd, planeRes->planes[i]);
        if (plane != nullptr) {
            bool matchingFormat = false;
            for (uint32_t j = 0; j < plane->count_formats && !matchingFormat; j++) {
                if (m_Main10Hdr) {
                    switch (plane->formats[j]) {
                    case DRM_FORMAT_P010:
                    case DRM_FORMAT_P030:
                    case DRM_FORMAT_NA12:
                    case DRM_FORMAT_NV15:
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

            if (matchingFormat == false) {
                drmModeFreePlane(plane);
                continue;
            }

            if ((plane->possible_crtcs & (1 << crtcIndex)) && plane->crtc_id == 0) {
                drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(m_DrmFd, planeRes->planes[i], DRM_MODE_OBJECT_PLANE);
                if (props != nullptr) {
                    for (uint32_t j = 0; j < props->count_props; j++) {
                        drmModePropertyPtr prop = drmModeGetProperty(m_DrmFd, props->props[j]);
                        if (prop != nullptr) {
                            if (!strcmp(prop->name, "type") &&
                                    (props->prop_values[j] == DRM_PLANE_TYPE_PRIMARY || props->prop_values[j] == DRM_PLANE_TYPE_OVERLAY)) {
                                m_PlaneId = plane->plane_id;
                            }

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

            drmModeFreePlane(plane);
        }
    }

    drmModeFreePlaneResources(planeRes);

    if (m_PlaneId == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to find suitable overlay plane!");
        return DIRECT_RENDERING_INIT_FAILED;
    }

    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(m_DrmFd, m_ConnectorId, DRM_MODE_OBJECT_CONNECTOR);
    if (props != nullptr) {
        for (uint32_t j = 0; j < props->count_props; j++) {
            drmModePropertyPtr prop = drmModeGetProperty(m_DrmFd, props->props[j]);
            if (prop != nullptr) {
                if (!strcmp(prop->name, "HDR_OUTPUT_METADATA")) {
                    m_HdrOutputMetadataProp = prop;
                }
                else if (!strcmp(prop->name, "max bpc") && m_Main10Hdr) {
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
    if (m_HwAccelBackend) {
        return pixelFormat == AV_PIX_FMT_DRM_PRIME;
    }
    else if (m_DrmPrimeBackend) {
        return m_BackendRenderer->isPixelFormatSupported(videoFormat, pixelFormat);
    }
    else {
        // If we're going to need to map this as a software frame, check
        // against the set of formats we support in mapSoftwareFrame().
        switch (pixelFormat) {
        case AV_PIX_FMT_DRM_PRIME:
        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_NV21:
        case AV_PIX_FMT_P010:
            return true;
        default:
            return false;
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

    return attributes;
}

void DrmRenderer::setHdrMode(bool enabled)
{
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
    uint32_t drmFormat;

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

    int bpc;

    // NB: Keep this list updated with isPixelFormatSupported()
    switch (frame->format) {
    case AV_PIX_FMT_NV12:
        drmFormat = DRM_FORMAT_NV12;
        bpc = 8;
        break;
    case AV_PIX_FMT_NV21:
        drmFormat = DRM_FORMAT_NV21;
        bpc = 8;
        break;
    case AV_PIX_FMT_P010:
        drmFormat = DRM_FORMAT_P010;
        bpc = 16;
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
        createBuf.height = frame->height * 2; // Y + CbCr at 2x2 subsampling
        createBuf.bpp = bpc;

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

        drmFrame->mapping = (uint8_t*)mmap(nullptr, drmFrame->size, PROT_WRITE, MAP_SHARED, m_DrmFd, mapBuf.offset);
        if (drmFrame->mapping == nullptr) {
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

        int lastPlaneSize = 0;
        for (int i = 0; i < 4; i++) {
            if (frame->data[i] != nullptr) {
                auto &plane = layer.planes[layer.nb_planes];

                plane.object_index = 0;
                plane.pitch = drmFrame->pitch;
                plane.offset = i == 0 ? 0 : (layer.planes[layer.nb_planes - 1].offset + lastPlaneSize);

                int planeHeight;
                if (i == 0) {
                    // Y plane is not subsampled
                    planeHeight = frame->height;
                }
                else {
                    // UV/VU planes are 2x2 subsampled.
                    //
                    // NB: The pitch is the same between Y and UV/VU, because the 2x subsampling
                    // is cancelled out by the 2x plane size of UV/VU vs U/V alone.
                    planeHeight = frame->height / 2;
                }

                // Copy the plane data into the dumb buffer
                if (frame->linesize[i] == (int)drmFrame->pitch) {
                    // We can do a single memcpy() if the pitch is compatible
                    memcpy(drmFrame->mapping + plane.offset,
                           frame->data[i],
                           frame->linesize[i] * planeHeight);
                }
                else {
                    // The pitch is incompatible, so we must copy line-by-line
                    for (int j = 0; j < planeHeight; j++) {
                        memcpy(drmFrame->mapping + (j * drmFrame->pitch) + plane.offset,
                               frame->data[i] + (j * frame->linesize[i]),
                               qMin(frame->linesize[i], (int)drmFrame->pitch));
                    }
                }

                layer.nb_planes++;

                lastPlaneSize = drmFrame->pitch * planeHeight;
            }
        }
    }

    ret = true;
    m_CurrentSwFrameIdx = (m_CurrentSwFrameIdx + 1) % k_SwFrameCount;

Exit:
    if (freeFrame) {
        av_frame_free(&frame);
    }

    return ret;
}

bool DrmRenderer::addFbForFrame(AVFrame *frame, uint32_t* newFbId)
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
                     "drmModeAddFB2WithModifiers() failed: %d",
                     errno);
        return false;
    }

    return true;
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
    if (!addFbForFrame(frame, &m_CurrentFbId)) {
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

   // Ensure we can export DRM PRIME frames (if applicable) and
   // add a FB object with the provided DRM format.
   if (!addFbForFrame(frame, &fbId)) {
       return false;
   }

   drmModeRmFB(m_DrmFd, fbId);
   return true;
}

bool DrmRenderer::isDirectRenderingSupported()
{
    return m_SupportsDirectRendering;
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
    else if (m_SupportsDirectRendering && m_Main10Hdr) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Using direct rendering for HDR support");
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "DRM backend supports exporting EGLImage");
    return true;
}

AVPixelFormat DrmRenderer::getEGLImagePixelFormat() {
    // This tells EGLRenderer to treat the EGLImage as a single opaque texture
    return AV_PIX_FMT_DRM_PRIME;
}

bool DrmRenderer::initializeEGL(EGLDisplay,
                                const EGLExtensions &ext) {
    if (!ext.isSupported("EGL_EXT_image_dma_buf_import")) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DRM-EGL: DMABUF unsupported");
        return false;
    }

    m_EGLExtDmaBuf = ext.isSupported("EGL_EXT_image_dma_buf_import_modifiers");

    // NB: eglCreateImage() and eglCreateImageKHR() have slightly different definitions
    m_eglCreateImage = (typeof(m_eglCreateImage))eglGetProcAddress("eglCreateImage");
    m_eglCreateImageKHR = (typeof(m_eglCreateImageKHR))eglGetProcAddress("eglCreateImageKHR");
    m_eglDestroyImage = (typeof(m_eglDestroyImage))eglGetProcAddress("eglDestroyImage");
    m_eglDestroyImageKHR = (typeof(m_eglDestroyImageKHR))eglGetProcAddress("eglDestroyImageKHR");

    if (!(m_eglCreateImage && m_eglDestroyImage) &&
            !(m_eglCreateImageKHR && m_eglDestroyImageKHR)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Missing eglCreateImage()/eglDestroyImage() in EGL driver");
        return false;
    }

    return true;
}

ssize_t DrmRenderer::exportEGLImages(AVFrame *frame, EGLDisplay dpy,
                                     EGLImage images[EGL_MAX_PLANES]) {
    AVDRMFrameDescriptor* drmFrame = (AVDRMFrameDescriptor*)frame->data[0];

    memset(images, 0, sizeof(EGLImage) * EGL_MAX_PLANES);

    // DRM requires composed layers rather than separate layers per plane
    SDL_assert(drmFrame->nb_layers == 1);

    // Max 30 attributes (1 key + 1 value for each)
    const int MAX_ATTRIB_COUNT = 30 * 2;
    EGLAttrib attribs[MAX_ATTRIB_COUNT] = {
        EGL_LINUX_DRM_FOURCC_EXT, (EGLAttrib)drmFrame->layers[0].format,
        EGL_WIDTH, frame->width,
        EGL_HEIGHT, frame->height,
    };
    int attribIndex = 6;

    for (int i = 0; i < drmFrame->layers[0].nb_planes; ++i) {
        const auto &plane = drmFrame->layers[0].planes[i];
        const auto &object = drmFrame->objects[plane.object_index];

        switch (i) {
        case 0:
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_FD_EXT;
            attribs[attribIndex++] = object.fd;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
            attribs[attribIndex++] = plane.offset;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
            attribs[attribIndex++] = plane.pitch;
            if (m_EGLExtDmaBuf && object.format_modifier != DRM_FORMAT_MOD_INVALID) {
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier & 0xFFFFFFFF);
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier >> 32);
            }
            break;

        case 1:
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_FD_EXT;
            attribs[attribIndex++] = object.fd;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
            attribs[attribIndex++] = plane.offset;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
            attribs[attribIndex++] = plane.pitch;
            if (m_EGLExtDmaBuf && object.format_modifier != DRM_FORMAT_MOD_INVALID) {
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier & 0xFFFFFFFF);
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier >> 32);
            }
            break;

        case 2:
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_FD_EXT;
            attribs[attribIndex++] = object.fd;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
            attribs[attribIndex++] = plane.offset;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
            attribs[attribIndex++] = plane.pitch;
            if (m_EGLExtDmaBuf && object.format_modifier != DRM_FORMAT_MOD_INVALID) {
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier & 0xFFFFFFFF);
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier >> 32);
            }
            break;

        case 3:
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_FD_EXT;
            attribs[attribIndex++] = object.fd;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
            attribs[attribIndex++] = plane.offset;
            attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
            attribs[attribIndex++] = plane.pitch;
            if (m_EGLExtDmaBuf && object.format_modifier != DRM_FORMAT_MOD_INVALID) {
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier & 0xFFFFFFFF);
                attribs[attribIndex++] = EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT;
                attribs[attribIndex++] = (EGLint)(object.format_modifier >> 32);
            }
            break;

        default:
            Q_UNREACHABLE();
        }
    }

    // Add colorspace metadata
    switch (getFrameColorspace(frame)) {
    case COLORSPACE_REC_601:
        attribs[attribIndex++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
        attribs[attribIndex++] = EGL_ITU_REC601_EXT;
        break;
    case COLORSPACE_REC_709:
        attribs[attribIndex++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
        attribs[attribIndex++] = EGL_ITU_REC709_EXT;
        break;
    case COLORSPACE_REC_2020:
        attribs[attribIndex++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
        attribs[attribIndex++] = EGL_ITU_REC2020_EXT;
        break;
    }

    // Add color range metadata
    attribs[attribIndex++] = EGL_SAMPLE_RANGE_HINT_EXT;
    attribs[attribIndex++] = isFrameFullRange(frame) ? EGL_YUV_FULL_RANGE_EXT : EGL_YUV_NARROW_RANGE_EXT;

    // Terminate the attribute list
    attribs[attribIndex++] = EGL_NONE;
    SDL_assert(attribIndex <= MAX_ATTRIB_COUNT);

    // Our EGLImages are non-planar, so we only populate the first entry
    if (m_eglCreateImage) {
        images[0] = m_eglCreateImage(dpy, EGL_NO_CONTEXT,
                                     EGL_LINUX_DMA_BUF_EXT,
                                     nullptr, attribs);
        if (!images[0]) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "eglCreateImage() Failed: %d", eglGetError());
            goto fail;
        }
    }
    else {
        // Cast the EGLAttrib array elements to EGLint for the KHR extension
        EGLint intAttribs[MAX_ATTRIB_COUNT];
        for (int i = 0; i < MAX_ATTRIB_COUNT; i++) {
            intAttribs[i] = (EGLint)attribs[i];
        }

        images[0] = m_eglCreateImageKHR(dpy, EGL_NO_CONTEXT,
                                        EGL_LINUX_DMA_BUF_EXT,
                                        nullptr, intAttribs);
        if (!images[0]) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "eglCreateImageKHR() Failed: %d", eglGetError());
            goto fail;
        }
    }

    return 1;

fail:
    freeEGLImages(dpy, images);
    return -1;
}

void DrmRenderer::freeEGLImages(EGLDisplay dpy, EGLImage images[EGL_MAX_PLANES]) {
    if (m_eglDestroyImage) {
        m_eglDestroyImage(dpy, images[0]);
    }
    else {
        m_eglDestroyImageKHR(dpy, images[0]);
    }

    // Our EGLImages are non-planar
    SDL_assert(images[1] == 0);
    SDL_assert(images[2] == 0);
}

#endif
