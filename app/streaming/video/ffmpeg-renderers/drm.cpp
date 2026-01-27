// mmap64() for 32-bit off_t systems
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif

#include "drm.h"
#include "utils.h"

extern "C" {
    #include <libavutil/hwcontext_drm.h>
    #include <libavutil/pixdesc.h>
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

// Upstreamed fully-planar YUV444 10-bit
#ifndef DRM_FORMAT_Q410
#define DRM_FORMAT_Q410	fourcc_code('Q', '4', '1', '0')
#endif

// Upstreamed packed YUV444 10-bit
#ifndef DRM_FORMAT_Y410
#define DRM_FORMAT_Y410 fourcc_code('Y', '4', '1', '0')
#endif

// Upstreamed packed YUV444 8-bit
#ifndef DRM_FORMAT_XYUV8888
#define DRM_FORMAT_XYUV8888 fourcc_code('X', 'Y', 'U', 'V')
#endif

// Upstreamed modifier macros (5.16+)
#ifndef fourcc_mod_get_vendor
#define fourcc_mod_get_vendor(modifier) (((modifier) >> 56) & 0xff)
#endif
#ifndef fourcc_mod_is_vendor
#define fourcc_mod_is_vendor(modifier, vendor) \
    (fourcc_mod_get_vendor(modifier) == DRM_FORMAT_MOD_VENDOR_## vendor)
#endif

#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <sys/mman.h>

#include "streaming/streamutils.h"
#include "streaming/session.h"

#include <Limelight.h>

#include <map>

// This map is used to lookup characteristics of a given DRM format
//
// All DRM formats that we want to try when selecting a plane must
// be listed here.
static const std::map<uint32_t, AVPixelFormat> k_DrmToAvFormatMap
{
    {DRM_FORMAT_NV12, AV_PIX_FMT_NV12},
    {DRM_FORMAT_NV21, AV_PIX_FMT_NV21},
    {DRM_FORMAT_P010, AV_PIX_FMT_P010LE},
    {DRM_FORMAT_YUV420, AV_PIX_FMT_YUV420P},
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 27, 100)
    {DRM_FORMAT_NV24, AV_PIX_FMT_NV24},
    {DRM_FORMAT_NV42, AV_PIX_FMT_NV42},
#endif
    {DRM_FORMAT_YUV444, AV_PIX_FMT_YUV444P},
    {DRM_FORMAT_Q410, AV_PIX_FMT_YUV444P10LE},
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 34, 100)
    {DRM_FORMAT_XYUV8888, AV_PIX_FMT_VUYX},
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 36, 100)
    {DRM_FORMAT_Y410, AV_PIX_FMT_XV30LE},
#endif

    // These mappings are lies, but they're close enough for our purposes.
    //
    // We don't support dumb buffers with these formats, so they just need
    // to have accurate bit depth and chroma subsampling values.
    {DRM_FORMAT_NA12, AV_PIX_FMT_P010LE},
    {DRM_FORMAT_NV15, AV_PIX_FMT_P010LE},
    {DRM_FORMAT_P030, AV_PIX_FMT_P010LE},
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 9, 100)
    {DRM_FORMAT_NV30, AV_PIX_FMT_P410LE},
#endif
};

// This map is used to determine the required DRM format for dumb buffer upload.
//
// AV pixel formats in this list must have exactly one valid linear DRM format.
static const std::map<AVPixelFormat, uint32_t> k_AvToDrmFormatMap
{
    {AV_PIX_FMT_NV12, DRM_FORMAT_NV12},
    {AV_PIX_FMT_NV21, DRM_FORMAT_NV21},
    {AV_PIX_FMT_P010LE, DRM_FORMAT_P010},
    {AV_PIX_FMT_YUV420P, DRM_FORMAT_YUV420},
    {AV_PIX_FMT_YUVJ420P, DRM_FORMAT_YUV420},
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 27, 100)
    {AV_PIX_FMT_NV24, DRM_FORMAT_NV24},
    {AV_PIX_FMT_NV42, DRM_FORMAT_NV42},
#endif
    {AV_PIX_FMT_YUV444P, DRM_FORMAT_YUV444},
    {AV_PIX_FMT_YUVJ444P, DRM_FORMAT_YUV444},
    {AV_PIX_FMT_YUV444P10LE, DRM_FORMAT_Q410},
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 34, 100)
    {AV_PIX_FMT_VUYX, DRM_FORMAT_XYUV8888},
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 36, 100)
    {AV_PIX_FMT_XV30LE, DRM_FORMAT_Y410},
#endif
};

DrmRenderer::DrmRenderer(AVHWDeviceType hwDeviceType, IFFmpegRenderer *backendRenderer)
    : IFFmpegRenderer(RendererType::DRM),
      m_BackendRenderer(backendRenderer),
      m_DrmPrimeBackend(backendRenderer && backendRenderer->canExportDrmPrime()),
      m_HwDeviceType(hwDeviceType),
      m_HwContext(nullptr),
      m_DrmFd(-1),
      m_DrmIsMaster(false),
      m_DrmStateModified(false),
      m_DrmSupportsModifiers(false),
      m_MustCloseDrmFd(false),
      m_SupportsDirectRendering(false),
      m_VideoFormat(0),
      m_OverlayCompositionSurface(nullptr),
      m_OverlayRects{},
      m_Version(nullptr),
      m_HdrOutputMetadataBlobId(0),
      m_OutputRect{},
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
    // If we have a composition surface, unmap it before disabling planes
    if (m_OverlayCompositionSurface) {
        munmap(m_OverlayCompositionSurface->pixels, (uintptr_t)m_OverlayCompositionSurface->userdata);
        SDL_FreeSurface(m_OverlayCompositionSurface);
    }

    if (m_DrmStateModified) {
        // Ensure we're out of HDR mode
        setHdrMode(false);

        // Deactivate all planes
        m_PropSetter.disablePlane(m_VideoPlane);
        for (int i = 0; i < Overlay::OverlayMax; i++) {
            m_PropSetter.disablePlane(m_OverlayPlanes[i]);
        }

        // Revert our changes from prepareToRender()
        if (auto prop = m_Connector.property("content type")) {
            m_PropSetter.restorePropertyToInitial(*prop);
        }
        if (auto prop = m_Crtc.property("VRR_ENABLED")) {
            m_PropSetter.restorePropertyToInitial(*prop);
        }
        if (auto prop = m_Connector.property("max bpc")) {
            m_PropSetter.restorePropertyToInitial(*prop);
        }
        if (auto zpos = m_VideoPlane.property("zpos"); zpos && !zpos->isImmutable()) {
            m_PropSetter.restorePropertyToInitial(*zpos);
        }
        for (int i = 0; i < Overlay::OverlayMax; i++) {
            if (auto zpos = m_OverlayPlanes[i].property("zpos"); zpos && !zpos->isImmutable()) {
                m_PropSetter.restorePropertyToInitial(*zpos);
            }
        }
        for (auto &plane : m_UnusedActivePlanes) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Restoring previously active plane: %u",
                        plane.second.objectId());
            m_PropSetter.restoreToInitial(plane.second);
        }

        m_PropSetter.apply();
    }

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

    if (m_HdrOutputMetadataBlobId != 0) {
        drmModeDestroyPropertyBlob(m_DrmFd, m_HdrOutputMetadataBlobId);
    }

    if (m_Version != nullptr) {
        drmFreeVersion(m_Version);
    }

    if (m_HwContext != nullptr) {
        av_buffer_unref(&m_HwContext);
    }

    if (m_MustCloseDrmFd && m_DrmFd != -1) {
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

    // This option controls the pixel format for the h264_omx and hevc_omx decoders
    // used on the TH1520 running RevyOS. The decoder will give us software frames
    // by default but we can opt in for DRM_PRIME frames to dramatically improve
    // streaming performance.
    av_dict_set_int(options, "drm_prime", 1, 0);

    if (m_HwDeviceType != AV_HWDEVICE_TYPE_NONE) {
        context->hw_device_ctx = av_buffer_ref(m_HwContext);
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using DRM renderer");

    return true;
}

bool DrmRenderer::prepareDecoderContextInGetFormat(AVCodecContext*, AVPixelFormat)
{
#ifdef HAVE_EGL
    // The surface pool is being reset, so clear the cached EGLImages
    m_EglImageFactory.resetCache();
#endif
    return true;
}

void DrmRenderer::prepareToRender()
{
    // Retake DRM master if we dropped it earlier
    drmSetMaster(m_DrmFd);

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
        SDL_assert(Session::get());
        Session::get()->flushWindowEvents();

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

    // Set our DRM client caps again. SDL 3.4+ will disable these
    // when dropping master if it's using atomic itself.
    drmSetClientCap(m_DrmFd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (m_PropSetter.isAtomic()) {
        drmSetClientCap(m_DrmFd, DRM_CLIENT_CAP_ATOMIC, 1);
    }

    // Set the output rect to match the new CRTC size after modesetting
    m_OutputRect.x = m_OutputRect.y = 0;
    drmModeCrtc* crtc = drmModeGetCrtc(m_DrmFd, m_Crtc.objectId());
    if (crtc != nullptr) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "CRTC size after modesetting: %ux%u",
                    crtc->width,
                    crtc->height);
        m_OutputRect.w = crtc->width;
        m_OutputRect.h = crtc->height;
        drmModeFreeCrtc(crtc);
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmModeGetCrtc() failed: %d",
                     errno);

        SDL_GetWindowSize(m_Window, &m_OutputRect.w, &m_OutputRect.h);
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Guessing CRTC is window size: %dx%d",
                    m_OutputRect.w,
                    m_OutputRect.h);
    }

    // Set HDMI content type to hopefully enable ALLM
    if (auto prop = m_Connector.property("content type")) {
        QString contentType = qgetenv("DRM_CONTENT_TYPE");
        if (contentType.isEmpty()) {
            contentType = "Game";
        }
        m_PropSetter.set(*prop, contentType.toStdString());
    }

    // Enable VRR if V-sync is off by default
    if (auto prop = m_Crtc.property("VRR_ENABLED")) {
        bool enableVrr;
        if (!Utils::getEnvironmentVariableOverride("DRM_ENABLE_VRR", &enableVrr)) {
            enableVrr = !m_Vsync;
        }

        m_PropSetter.set(*prop, prop->clamp(enableVrr ? 1 : 0));
    }

    if (auto prop = m_Connector.property("max bpc")) {
        int maxBpc;

        // By default, set max bpc to 10 if we're streaming 10-bit content and it's currently
        // less than that value. If it's higher than 10 or we're not streaming 10-bit content,
        // we leave it alone.
        if (!Utils::getEnvironmentVariableOverride("DRM_MAX_BPC", &maxBpc)) {
            maxBpc = (prop->initialValue() < 10 && (m_VideoFormat & VIDEO_FORMAT_MASK_10BIT)) ? 10 : 0;
        }

        if (maxBpc > 0) {
            m_PropSetter.set(*prop, prop->clamp(maxBpc));
        }
    }

    // Adjust zpos values if needed
    if (auto zpos = m_VideoPlane.property("zpos"); zpos && m_VideoPlaneZpos != zpos->initialValue()) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Moving video plane to zpos: %" PRIu64,
                    m_VideoPlaneZpos);
        m_PropSetter.set(*zpos, m_VideoPlaneZpos, false);
    }
    for (int i = 0; i < Overlay::OverlayMax; i++) {
        if (auto zpos = m_OverlayPlanes[i].property("zpos")) {
            // This may result in multiple overlays having the same zpos, which
            // means undefined ordering between the planes, but that's fine.
            // The planes should never overlap anyway.
            if (!zpos->isImmutable() && zpos->initialValue() <= m_VideoPlaneZpos) {
                uint64_t newZpos = zpos->clamp(m_VideoPlaneZpos + 1);

                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Moving overlay plane %u to zpos: %" PRIu64,
                            i,
                            newZpos);
                m_PropSetter.set(*zpos, newZpos, false);
            }
        }
    }

    // Disable all other active planes in atomic mode
    for (auto &plane : m_UnusedActivePlanes) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Disabling unused plane: %u",
                    plane.second.objectId());
        m_PropSetter.disablePlane(plane.second);
    }

    // Enter overlay composition mode if we don't have enough planes to display all the
    // possible overlays we might need, but we have at least one available
    if (m_OverlayPlanes[0].isValid() && !m_OverlayPlanes[Overlay::OverlayMax - 1].isValid()) {
        enterOverlayCompositionMode();
    }

    m_PropSetter.apply();

    // We've now changed state that must be restored
    m_DrmStateModified = true;
}

bool DrmRenderer::initialize(PDECODER_PARAMETERS params)
{
    int i;

    m_Window = params->window;
    m_VideoFormat = params->videoFormat;
    m_Vsync = params->enableVsync;
    m_SwFrameMapper.setVideoFormat(params->videoFormat);

    // Try to get the FD that we're sharing with SDL
    m_DrmFd = StreamUtils::getDrmFdForWindow(m_Window, &m_MustCloseDrmFd);
    if (m_DrmFd >= 0) {
        // If we got a DRM FD for the window, we can render to it
        m_DrmIsMaster = true;

        // If we just opened a new FD, let's drop master on it
        // so SDL can take master for Vulkan rendering. We'll
        // regrab master later if we end up direct rendering.
        if (m_MustCloseDrmFd) {
            drmDropMaster(m_DrmFd);
        }
    }
    else {
        // Try to open any DRM render node
        m_DrmFd = StreamUtils::getDrmFd(true);
        if (m_DrmFd >= 0) {
            // Drop master in case we somehow got a primary node
            drmDropMaster(m_DrmFd);

            // This is a new FD that we must close
            m_MustCloseDrmFd = true;
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
    if (!m_DrmIsMaster) {
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
    for (i = 0; i < resources->count_connectors && !m_Encoder.isValid(); i++) {
        drmModeConnector* connector = drmModeGetConnector(m_DrmFd, resources->connectors[i]);
        if (connector != nullptr) {
            if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
                m_Connector.load(m_DrmFd, resources->connectors[i], DRM_MODE_OBJECT_CONNECTOR);
                m_Encoder.load(m_DrmFd, connector->encoder_id, DRM_MODE_OBJECT_ENCODER);
            }

            drmModeFreeConnector(connector);
        }
    }

    if (!m_Encoder.isValid()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "No connected displays found!");
        drmModeFreeResources(resources);
        return DIRECT_RENDERING_INIT_FAILED;
    }

    // Now find the CRTC from the encoder
    for (i = 0; i < resources->count_encoders && !m_Crtc.isValid(); i++) {
        drmModeEncoder* encoder = drmModeGetEncoder(m_DrmFd, resources->encoders[i]);
        if (encoder != nullptr) {
            if (encoder->encoder_id == m_Encoder.objectId()) {
                m_Crtc.load(m_DrmFd, encoder->crtc_id, DRM_MODE_OBJECT_CRTC);
            }

            drmModeFreeEncoder(encoder);
        }
    }

    if (!m_Crtc.isValid()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DRM encoder not found!");
        drmModeFreeResources(resources);
        return DIRECT_RENDERING_INIT_FAILED;
    }

    int crtcIndex = -1;
    for (int i = 0; i < resources->count_crtcs; i++) {
        if (resources->crtcs[i] == m_Crtc.objectId()) {
            crtcIndex = i;
            break;
        }
    }

    drmModeFreeResources(resources);

    if (crtcIndex == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to get CRTC!");
        return DIRECT_RENDERING_INIT_FAILED;
    }

    if (drmSetClientCap(m_DrmFd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Universal planes are not supported!");
        return DIRECT_RENDERING_INIT_FAILED;
    }

    bool atomic;
    if (!Utils::getEnvironmentVariableOverride("DRM_ATOMIC", &atomic)) {
        // Use atomic by default if available
        atomic = true;
    }

    m_PropSetter.initialize(m_DrmFd, atomic, !params->enableVsync);

    drmModePlaneRes* planeRes = drmModeGetPlaneResources(m_DrmFd);
    if (planeRes == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmGetPlaneResources() failed: %d",
                     errno);
        return DIRECT_RENDERING_INIT_FAILED;
    }

    // Find the active plane (if any) on this CRTC with the highest zpos.
    // We'll need to use a plane with a equal or greater zpos to be visible,
    // or we'll disable the active planes if we're in atomic mode.
    std::set<uint64_t> activePlanesZpos;
    for (uint32_t i = 0; i < planeRes->count_planes; i++) {
        drmModePlane* plane = drmModeGetPlane(m_DrmFd, planeRes->planes[i]);
        if (plane != nullptr) {
            DrmPropertyMap props { m_DrmFd, planeRes->planes[i], DRM_MODE_OBJECT_PLANE };

            if (plane->crtc_id == m_Crtc.objectId()) {
                // Don't consider cursor planes when searching for the highest active zpos
                uint64_t type = props.property("type")->initialValue();
                if (type == DRM_PLANE_TYPE_PRIMARY || type == DRM_PLANE_TYPE_OVERLAY) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Plane %u is active on CRTC %u",
                                plane->plane_id,
                                plane->crtc_id);

                    // We can only restore state of planes on atomic
                    if (m_PropSetter.isAtomic()) {
                        m_UnusedActivePlanes.try_emplace(planeRes->planes[i], m_DrmFd, planeRes->planes[i], DRM_MODE_OBJECT_PLANE);
                    }
                    else if (auto zpos = props.property("zpos")) {
                        activePlanesZpos.emplace(zpos->initialValue());
                    }
                }
            }

            drmModeFreePlane(plane);
        }
    }

    // The Spacemit K1 driver is broken and advertises support for NV12/P010
    // formats with the linear modifier on all planes, but doesn't actually
    // support raw YUV formats on the primary plane. Don't ever use primary
    // planes on Spacemit hardware to avoid triggering this bug.
    bool allowPrimaryPlane;
    if (Utils::getEnvironmentVariableOverride("DRM_ALLOW_PRIMARY_PLANE", &allowPrimaryPlane)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using DRM_ALLOW_PRIMARY_PLANE to override default plane selection logic");
    }
    else {
        allowPrimaryPlane = strcmp(m_Version->name, "spacemit") != 0;
    }

    // Some Rockchip have a device tree that defines their only overlay plane
    // as a cursor plane, so we provide an override to allow rendering to a
    // cursor plane if requested.
    // https://github.com/moonlight-stream/moonlight-embedded/pull/882
    bool allowCursorPlane;
    if (Utils::getEnvironmentVariableOverride("DRM_ALLOW_CURSOR_PLANE", &allowCursorPlane)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using DRM_ALLOW_CURSOR_PLANE to override default plane selection logic");
    }
    else {
        allowCursorPlane = false;
    }

    // Find a video plane with the required format to render on
    //
    // FIXME: We should check the actual DRM format in a real AVFrame rather
    // than just assuming it will be a certain hardcoded type like NV12 based
    // on the chosen video format.
    for (uint32_t i = 0; i < planeRes->count_planes && !m_VideoPlane.isValid(); i++) {
        drmModePlane* plane = drmModeGetPlane(m_DrmFd, planeRes->planes[i]);
        if (plane != nullptr) {
            // If the plane can't be used on our CRTC, don't consider it further
            if (!(plane->possible_crtcs & (1 << crtcIndex))) {
                drmModeFreePlane(plane);
                continue;
            }

            {
                // Allow the user to override the plane selection logic
                uint32_t userPlane;
                if (Utils::getEnvironmentVariableOverride("DRM_VIDEO_PLANE", &userPlane) && userPlane != planeRes->planes[i]) {
                    drmModeFreePlane(plane);
                    continue;
                }
            }

            // We don't check plane->crtc_id here because we want to be able to reuse the primary plane
            // that may owned by Qt and in use on a CRTC prior to us taking over DRM master. When we give
            // control back to Qt, it will repopulate the plane with the FB it owns and render as normal.

            // Validate that the candidate plane supports our pixel format
            m_SupportedVideoPlaneFormats.clear();
            for (uint32_t j = 0; j < plane->count_formats; j++) {
                if (drmFormatMatchesVideoFormat(plane->formats[j], m_VideoFormat)) {
                    m_SupportedVideoPlaneFormats.emplace(plane->formats[j]);
                }
            }

            if (m_SupportedVideoPlaneFormats.empty()) {
                drmModeFreePlane(plane);
                continue;
            }

            // Check if the plane is one that we're allowed to use
            DrmPropertyMap props { m_DrmFd, planeRes->planes[i], DRM_MODE_OBJECT_PLANE };
            if (auto type = props.property("type");
                type->initialValue() != DRM_PLANE_TYPE_OVERLAY &&
                (type->initialValue() != DRM_PLANE_TYPE_PRIMARY || !allowPrimaryPlane) &&
                (type->initialValue() != DRM_PLANE_TYPE_CURSOR || !allowCursorPlane)) {
                drmModeFreePlane(plane);
                continue;
            }

            // If this plane has a zpos property and it's lower (further from user) than
            // the highest active plane we found, avoid this plane unless we can adjust
            // the zpos property to an acceptable value.
            //
            // Note: zpos is not a required property, but if any plane has it, all planes must.
            auto zpos = props.property("zpos");
            if (zpos) {
                // If the zpos property is immutable, then we're stuck with whatever it is
                if (zpos->isImmutable()) {
                    if (!activePlanesZpos.empty() && zpos->initialValue() < *activePlanesZpos.crbegin()) {
                        // This plane is too low to be visible
                        drmModeFreePlane(plane);
                        continue;
                    }
                    else {
                        m_VideoPlaneZpos = zpos->initialValue();
                    }
                }
                else {
                    auto zposRange = *zpos->range();

                    uint64_t lowestAcceptableZpos;

                    // No active planes, so we can use the minimum zpos
                    auto zposIt = activePlanesZpos.crbegin();
                    if (zposIt == activePlanesZpos.crend()) {
                        lowestAcceptableZpos = zposRange.first;
                    }
                    else if (*zposIt == zpos->initialValue()) {
                        // The highest active zpos is our current plane, so try the next one
                        if (++zposIt == activePlanesZpos.crend()) {
                            // Our plane is the only active, so we can use the minimum zpos
                            lowestAcceptableZpos = zposRange.first;
                        }
                        else {
                            lowestAcceptableZpos = *zposIt + 1;
                        }
                    }
                    else {
                        // The highest active zpos is some other plane that isn't ours
                        lowestAcceptableZpos = *zposIt + 1;
                    }

                    m_VideoPlaneZpos = zpos->clamp(lowestAcceptableZpos);
                }
            }
            else {
                m_VideoPlaneZpos = 0;
            }

            SDL_assert(!m_VideoPlane.isValid());
            m_VideoPlane.load(m_DrmFd, plane->plane_id, DRM_MODE_OBJECT_PLANE);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Selected plane %u for video", plane->plane_id);
            m_UnusedActivePlanes.erase(plane->plane_id);
            drmModeFreePlane(plane);
        }
    }

    if (!m_VideoPlane.isValid()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to find suitable video plane!");
        drmModeFreePlaneResources(planeRes);
        return DIRECT_RENDERING_INIT_FAILED;
    }

    // Find overlay planes when using the atomic API
    int overlayIndex = 0;
    for (uint32_t i = 0; i < planeRes->count_planes && overlayIndex < Overlay::OverlayMax && m_PropSetter.isAtomic(); i++) {
        drmModePlane* plane = drmModeGetPlane(m_DrmFd, planeRes->planes[i]);
        if (plane != nullptr) {
            // If the plane can't be used on our CRTC, don't consider it further
            if (!(plane->possible_crtcs & (1 << crtcIndex))) {
                drmModeFreePlane(plane);
                continue;
            }

            {
                // Allow the user to override the plane selection logic
                uint32_t userPlane;
                QString optionVarName = QString("DRM_OVERLAY_PLANE%1").arg(overlayIndex);
                if (Utils::getEnvironmentVariableOverride(optionVarName.toUtf8(), &userPlane) && userPlane != planeRes->planes[i]) {
                    drmModeFreePlane(plane);
                    continue;
                }
            }

            DrmPropertyMap props { m_DrmFd, planeRes->planes[i], DRM_MODE_OBJECT_PLANE };
            // Only consider overlay or primary planes as valid targets
            // The latter might seem strange, but some DRM devices use
            // underlays where the YUV-compatible overlay plane resides
            // underneath the primary plane. In this case, we will use
            // the primary plane as an overlay plane on top of the video.
            if (auto type = props.property("type")) {
                if (type->initialValue() != DRM_PLANE_TYPE_OVERLAY && type->initialValue() != DRM_PLANE_TYPE_PRIMARY) {
                    drmModeFreePlane(plane);
                    continue;
                }
            }

            // The overlay plane must support ARGB8888
            bool foundFormat = false;
            for (uint32_t j = 0; j < plane->count_formats; j++) {
                if (plane->formats[j] == DRM_FORMAT_ARGB8888) {
                    foundFormat = true;
                    break;
                }
            }
            if (!foundFormat) {
                drmModeFreePlane(plane);
                continue;
            }

            // If this plane has a zpos property and it's lower (further from user) than
            // the highest active plane we found, avoid this plane unless we can adjust
            // the zpos property to an acceptable value.
            //
            // Note: zpos is not a required property, but if any plane has it, all planes must.
            auto zpos = props.property("zpos");
            if (zpos) {
                // If the zpos property is immutable, then we're stuck with whatever it is
                if (zpos->isImmutable()) {
                    if (zpos->initialValue() <= m_VideoPlaneZpos) {
                        // This plane is too low to be visible
                        drmModeFreePlane(plane);
                        continue;
                    }
                }
                else if ((*zpos->range()).second <= m_VideoPlaneZpos) {
                    // This plane cannot be raised high enough to be visible
                    drmModeFreePlane(plane);
                    continue;
                }
            }

            // Allocate this overlay plane to the next unused overlay slot
            SDL_assert(!m_OverlayPlanes[overlayIndex].isValid());
            m_OverlayPlanes[overlayIndex++].load(m_DrmFd, plane->plane_id, DRM_MODE_OBJECT_PLANE);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Selected plane %u for overlay %d",
                        plane->plane_id, overlayIndex);
            m_UnusedActivePlanes.erase(plane->plane_id);
            drmModeFreePlane(plane);
        }
    }

    drmModeFreePlaneResources(planeRes);

    if (!m_PropSetter.isAtomic()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Overlays require DRM atomic support");
    }
    else if (overlayIndex == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Unable to find any suitable overlay planes");
    }
    else if (overlayIndex < Overlay::OverlayMax) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using overlay composition (%d of %d found)",
                    overlayIndex,
                    Overlay::OverlayMax);
    }

    {
        uint64_t val;
        if (drmGetCap(m_DrmFd, DRM_CAP_ADDFB2_MODIFIERS, &val) == 0 && val) {
            m_DrmSupportsModifiers = true;
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "FB modifiers are unsupported. Video or overlays may display incorrectly!");
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
        else {
            auto avToDrmTuple = k_AvToDrmFormatMap.find(pixelFormat);
            if (avToDrmTuple == k_AvToDrmFormatMap.end()) {
                return false;
            }

            // If we've been called after initialize(), use the actual supported plane formats
            if (!m_SupportedVideoPlaneFormats.empty()) {
                return m_SupportedVideoPlaneFormats.find(avToDrmTuple->second) != m_SupportedVideoPlaneFormats.end();
            }
            else {
                // If we've been called before initialize(), use any valid plane format for our video formats
                return drmFormatMatchesVideoFormat(avToDrmTuple->second, videoFormat);
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
    if (auto prop = m_Connector.property("Colorspace")) {
        if (enabled) {
            // Prefer BT2020_YCC to allow chroma subsampling
            if (prop->containsValue("BT2020_YCC")) {
                m_PropSetter.set(*prop, "BT2020_YCC");
            }
            else {
                m_PropSetter.set(*prop, "BT2020_RGB");
            }
        }
        else {
            m_PropSetter.set(*prop, "Default");
        }
    }

    if (auto prop = m_Connector.property("HDR_OUTPUT_METADATA")) {
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
                             err);
                // Non-fatal
            }
        }

        m_PropSetter.set(*prop, enabled ? m_HdrOutputMetadataBlobId : 0);
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

    auto drmFormatTuple = k_AvToDrmFormatMap.find((AVPixelFormat) frame->format);
    if (drmFormatTuple == k_AvToDrmFormatMap.end()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to map frame with unsupported format: %d",
                     frame->format);
        goto Exit;
    }

    // If the frame size or format changed, we need to recreate the buffer
    if (frame->width != drmFrame->width ||
        frame->height != drmFrame->height ||
        drmFormatTuple->second != drmFrame->format) {

        if (drmFrame->primeFd) {
            close(drmFrame->primeFd);
            drmFrame->primeFd = 0;
        }

        if (drmFrame->mapping) {
            munmap(drmFrame->mapping, drmFrame->size);
            drmFrame->mapping = nullptr;
        }

        if (drmFrame->handle) {
            struct drm_mode_destroy_dumb destroyBuf = {};
            destroyBuf.handle = drmFrame->handle;
            drmIoctl(m_DrmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyBuf);
            drmFrame->handle = 0;
        }
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
            createBuf.height += (2 * AV_CEIL_RSHIFT(frame->height,
                                                    formatDesc->log2_chroma_w +
                                                    formatDesc->log2_chroma_h));
        }

        int err = drmIoctl(m_DrmFd, DRM_IOCTL_MODE_CREATE_DUMB, &createBuf);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "DRM_IOCTL_MODE_CREATE_DUMB failed: %d",
                         errno);
            goto Exit;
        }

        drmFrame->width = frame->width;
        drmFrame->height = frame->height;
        drmFrame->format = drmFormatTuple->second;
        drmFrame->handle = createBuf.handle;
        drmFrame->pitch = createBuf.pitch;
        drmFrame->size = createBuf.size;
    }

    // Map the dumb buffer if needed
    if (!drmFrame->mapping && !mapDumbBuffer(drmFrame->handle, drmFrame->size, (void**)&drmFrame->mapping)) {
        goto Exit;
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
        mappedFrame->objects[0].size = drmFrame->size;

        // We use DRM_FORMAT_MOD_INVALID because we don't want modifiers to be passed
        // in drmModeAddFB2WithModifiers() when creating an FB for a dumb buffer.
        // Dumb buffers are already implicitly linear and don't require modifiers.
        mappedFrame->objects[0].format_modifier = DRM_FORMAT_MOD_INVALID;

        mappedFrame->nb_layers = 1;

        auto &layer = mappedFrame->layers[0];
        layer.format = drmFrame->format;

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
    }

    ret = true;
    m_CurrentSwFrameIdx = (m_CurrentSwFrameIdx + 1) % k_SwFrameCount;

Exit:
    if (freeFrame) {
        av_frame_free(&frame);
    }

    return ret;
}

bool DrmRenderer::mapDumbBuffer(uint32_t handle, size_t size, void** mapping)
{
    struct drm_mode_map_dumb mapBuf = {};
    mapBuf.handle = handle;
    int err = drmIoctl(m_DrmFd, DRM_IOCTL_MODE_MAP_DUMB, &mapBuf);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DRM_IOCTL_MODE_MAP_DUMB failed: %d",
                     errno);
        return false;
    }

    // Raspberry Pi on kernel 6.1 defaults to an aarch64 kernel with a 32-bit userspace (and off_t).
    // This leads to issues when DRM_IOCTL_MODE_MAP_DUMB returns a > 4GB offset. The high bits are
    // chopped off when passed via the normal mmap() call using 32-bit off_t. We avoid this issue
    // by explicitly calling mmap64() to ensure the 64-bit offset is never truncated.
#if defined(__GLIBC__) && QT_POINTER_SIZE == 4
    *mapping = mmap64(nullptr, size, PROT_WRITE, MAP_SHARED, m_DrmFd, mapBuf.offset);
#else
    *mapping = mmap(nullptr, size, PROT_WRITE, MAP_SHARED, m_DrmFd, mapBuf.offset);
#endif
    if (mapping == MAP_FAILED) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "mmap() failed for dumb buffer: %d",
                     errno);
        *mapping = nullptr;
        return false;
    }

    return true;
}

bool DrmRenderer::createFbForDumbBuffer(struct drm_mode_create_dumb* createBuf, uint32_t* fbId)
{
    uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};

    // Create a FB backed by the dumb buffer
    handles[0] = createBuf->handle;
    pitches[0] = createBuf->pitch;
    int err = drmModeAddFB2(m_DrmFd, createBuf->width, createBuf->height,
                            DRM_FORMAT_ARGB8888,
                            handles, pitches, offsets, fbId, 0);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmModeAddFB2() failed: %d",
                     err);
        return false;
    }

    return true;
}

bool DrmRenderer::uploadSurfaceToFb(SDL_Surface *surface, uint32_t* handle, uint32_t* fbId)
{
    struct drm_mode_create_dumb createBuf = {};
    void* mapping;

    createBuf.width = surface->w;
    createBuf.height = surface->h;
    createBuf.bpp = 32;
    int err = drmIoctl(m_DrmFd, DRM_IOCTL_MODE_CREATE_DUMB, &createBuf);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DRM_IOCTL_MODE_CREATE_DUMB failed: %d",
                     errno);
        return false;
    }

    // Map the buffer, so we can copy our data into it
    if (!mapDumbBuffer(createBuf.handle, createBuf.size, &mapping)) {
        goto Fail;
    }

    // Convert and copy the surface pixels into the dumb buffer with premultiplied alpha
    SDL_PremultiplyAlpha(surface->w, surface->h, surface->format->format, surface->pixels, surface->pitch,
                         SDL_PIXELFORMAT_ARGB8888, mapping, createBuf.pitch);

    munmap(mapping, createBuf.size);

    if (!createFbForDumbBuffer(&createBuf, fbId)) {
        goto Fail;
    }

    *handle = createBuf.handle;
    return true;

Fail:
    struct drm_mode_destroy_dumb destroyBuf = {};
    destroyBuf.handle = createBuf.handle;
    drmIoctl(m_DrmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyBuf);
    return false;
}

void DrmRenderer::enterOverlayCompositionMode()
{
    if (m_OverlayCompositionSurface) {
        return;
    }

    // Turn off all existing overlay planes
    for (auto &overlay : m_OverlayPlanes) {
        if (overlay.isValid()) {
            m_PropSetter.disablePlane(overlay);
        }
    }

    struct drm_mode_create_dumb createBuf = {};
    uint32_t fbId;
    void* mapping = nullptr;

    createBuf.width = m_OutputRect.w;
    createBuf.height = m_OutputRect.h;
    createBuf.bpp = 32;
    int err = drmIoctl(m_DrmFd, DRM_IOCTL_MODE_CREATE_DUMB, &createBuf);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DRM_IOCTL_MODE_CREATE_DUMB failed: %d",
                     errno);
        return;
    }

    if (!mapDumbBuffer(createBuf.handle, createBuf.size, &mapping)) {
        goto Fail;
    }

    if (!createFbForDumbBuffer(&createBuf, &fbId)) {
        goto Fail;
    }

    // Configure the overlay plane to cover the entire display
    m_PropSetter.configurePlane(m_OverlayPlanes[0], m_Crtc.objectId(),
                                0, 0, m_OutputRect.w, m_OutputRect.h,
                                0, 0,
                                m_OutputRect.w << 16,
                                m_OutputRect.h << 16);

    // Flip the surface onto the overlay
    //
    // NB: This will take ownership of both the FB and the dumb buffer,
    // but it won't free them until we stop streaming since we don't
    // flip this plane anymore after this.
    m_PropSetter.flipPlane(m_OverlayPlanes[0], fbId, createBuf.handle);

    // Create an SDL surface that wraps our dumb buffer mapping
    m_OverlayCompositionSurface = SDL_CreateRGBSurfaceWithFormatFrom(mapping,
                                                                     m_OutputRect.w,
                                                                     m_OutputRect.h,
                                                                     32,
                                                                     createBuf.pitch,
                                                                     SDL_PIXELFORMAT_ARGB8888);
    m_OverlayCompositionSurface->userdata = (void*)createBuf.size;

    // Disable blending to avoid costly reads of possibly WC/UC data
    SDL_SetSurfaceBlendMode(m_OverlayCompositionSurface, SDL_BLENDMODE_NONE);
    return;

Fail:
    if (mapping) {
        munmap(mapping, createBuf.size);
    }

    struct drm_mode_destroy_dumb destroyBuf = {};
    destroyBuf.handle = createBuf.handle;
    drmIoctl(m_DrmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyBuf);
}

void DrmRenderer::blitOverlayToCompositionSurface(Overlay::OverlayType type, SDL_Surface* newSurface, SDL_Rect* overlayRect)
{
    SDL_assert(m_OverlayCompositionSurface);

    if (newSurface && overlayRect) {
        // Disable blending of the source surface when blitting
        SDL_SetSurfaceBlendMode(newSurface, SDL_BLENDMODE_NONE);

        // Premultiply alpha in place, so we can blit directly into the composition surface
        // without having to read anything (which may be very costly due to UC/WC memory)
        SDL_PremultiplyAlpha(newSurface->w, newSurface->h,
                             newSurface->format->format, newSurface->pixels, newSurface->pitch,
                             newSurface->format->format, newSurface->pixels, newSurface->pitch);

        // Compute the union of the current and previous overlay rects. Our draw operation
        // will need to cover this entire area to ensure the old dirty area is covered.
        SDL_Rect overlayUnionRect;
        SDL_UnionRect(overlayRect, &m_OverlayRects[type], &overlayUnionRect);

        // If the new overlay completely covers the old overlay, blit it all at once
        if (SDL_RectEquals(&overlayUnionRect, overlayRect)) {
            SDL_BlitSurface(newSurface, nullptr, m_OverlayCompositionSurface, overlayRect);
        }
        else {
            SDL_assert(newSurface->format->format == m_OverlayCompositionSurface->format->format);

            // Draw the surface row-by-row to ensure we clear the dirty area from the previous surface
            // without causing flickering, which would be noticeable if we cleared the whole area first.
            for (int y = overlayUnionRect.y; y < overlayUnionRect.y + overlayUnionRect.h; y++) {
                auto dstPixelRow =
                    (uint8_t*)m_OverlayCompositionSurface->pixels +
                    (y * m_OverlayCompositionSurface->pitch);
                auto bpp = m_OverlayCompositionSurface->format->BytesPerPixel;

                if (y < overlayRect->y || y > overlayRect->y + overlayRect->h) {
                    // Clear the whole row if the overlay doesn't intersect this row
                    memset(dstPixelRow + (overlayUnionRect.x * bpp),
                           0,
                           overlayUnionRect.w * bpp);
                }
                else {
                    auto srcPixelRow = (uint8_t*)newSurface->pixels + ((y - overlayRect->y) * newSurface->pitch);

                    // Clear columns prior to the intersection
                    SDL_assert(overlayRect->x >= overlayUnionRect.x);
                    memset(dstPixelRow + (overlayUnionRect.x * bpp),
                           0,
                           (overlayRect->x - overlayUnionRect.x) * bpp);

                    // Copy the overlay into the intersection
                    memcpy(dstPixelRow + (overlayRect->x * bpp),
                           srcPixelRow,
                           overlayRect->w * bpp);

                    // Clear columns after the intersection
                    SDL_assert(overlayUnionRect.w >= overlayRect->w);
                    memset(dstPixelRow + ((overlayRect->x + overlayRect->w) * bpp),
                           0,
                           (overlayUnionRect.w - overlayRect->w) * bpp);
                }
            }
        }

        // Dirty the modified portion of the plane
        m_PropSetter.damagePlane(m_OverlayPlanes[0], overlayUnionRect);
    }
    else {
        // Clear the pixels where this overlay was drawn before
        SDL_FillRect(m_OverlayCompositionSurface, &m_OverlayRects[type], 0);

        // Dirty the modified portion of the plane
        m_PropSetter.damagePlane(m_OverlayPlanes[0], m_OverlayRects[type]);
    }
}

void DrmRenderer::notifyOverlayUpdated(Overlay::OverlayType type)
{
    std::lock_guard lg { m_OverlayLock };

    // If we are not using atomic KMS, we can't support overlays
    if (!m_PropSetter.isAtomic()) {
        return;
    }

    // If we don't have any overlays, we can't do anything
    if (!m_OverlayPlanes[0].isValid()) {
        return;
    }

    // Don't upload if the overlay is disabled
    if (!Session::get()->getOverlayManager().isOverlayEnabled(type)) {
        // Turn the overlay plane off when transitioning from enabled to disabled
        if (m_OverlayRects[type].w || m_OverlayRects[type].h) {
            if (m_OverlayCompositionSurface) {
                blitOverlayToCompositionSurface(type, nullptr, nullptr);
            }
            else if (m_OverlayPlanes[type].isValid()) {
                m_PropSetter.disablePlane(m_OverlayPlanes[type]);
            }
            memset(&m_OverlayRects[type], 0, sizeof(m_OverlayRects[type]));
        }

        return;
    }

    // Upload a new overlay surface if needed
    SDL_Surface* newSurface = Session::get()->getOverlayManager().getUpdatedOverlaySurface(type);
    if (newSurface != nullptr) {
        uint32_t dumbBuffer, fbId;
        SDL_Rect overlayRect;

        if (type == Overlay::OverlayStatusUpdate) {
            // Bottom Left
            overlayRect.x = 0;
            overlayRect.y = m_OutputRect.h - newSurface->h;
        }
        else if (type == Overlay::OverlayDebug) {
            // Top left
            overlayRect.x = 0;
            overlayRect.y = 0;
        }

        overlayRect.w = newSurface->w;
        overlayRect.h = newSurface->h;

        // Try to let the display controller composite for us
        if (!m_OverlayCompositionSurface) {
            if (!uploadSurfaceToFb(newSurface, &dumbBuffer, &fbId)) {
                SDL_FreeSurface(newSurface);
                return;
            }

            // If we changed our overlay rect, we need to reconfigure the plane
            if (memcmp(&m_OverlayRects[type], &overlayRect, sizeof(overlayRect)) != 0) {
                if (m_PropSetter.testPlane(m_OverlayPlanes[type], m_Crtc.objectId(), fbId,
                                           overlayRect.x, overlayRect.y, overlayRect.w, overlayRect.h,
                                           0, 0,
                                           newSurface->w << 16,
                                           newSurface->h << 16)) {
                    m_PropSetter.configurePlane(m_OverlayPlanes[type], m_Crtc.objectId(),
                                                overlayRect.x, overlayRect.y, overlayRect.w, overlayRect.h,
                                                0, 0,
                                                newSurface->w << 16,
                                                newSurface->h << 16);
                }
                else {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Switching to overlay composition mode after commit failure");
                    enterOverlayCompositionMode();

                    // Free the original uploaded FB and dumb buffer
                    drmModeRmFB(m_DrmFd, fbId);
                    struct drm_mode_destroy_dumb destroyBuf = {};
                    destroyBuf.handle = dumbBuffer;
                    drmIoctl(m_DrmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyBuf);
                }
            }
        }

        // If we're in overlay composition mode, blit this overlay into the composition surface
        if (m_OverlayCompositionSurface) {
            blitOverlayToCompositionSurface(type, newSurface, &overlayRect);
        }
        else {
            // Otherwise queue the plane flip with the new FB
            //
            // NB: This takes ownership of the FB and dumb buffer, even on failure
            m_PropSetter.flipPlane(m_OverlayCompositionSurface ? m_OverlayPlanes[0] : m_OverlayPlanes[type],
                                   fbId, dumbBuffer);
        }

        memcpy(&m_OverlayRects[type], &overlayRect, sizeof(overlayRect));

        SDL_FreeSurface(newSurface);
    }
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

    // If we're testing, check the IN_FORMATS property or legacy plane formats
    if (testMode) {
        bool formatMatch = false;

        uint64_t maskedModifier = drmFrame->objects[0].format_modifier;
        if (fourcc_mod_is_vendor(maskedModifier, BROADCOM)) {
            // Broadcom has modifiers that contain variable data, so we need to mask
            // off the variable data because the IN_FORMATS blob contains the just
            // the base modifier alone
            maskedModifier = fourcc_mod_broadcom_mod(maskedModifier);
        }

        // If we have an IN_FORMATS property and the frame has DRM modifiers, use that since it supports modifiers too
        if (auto prop = m_VideoPlane.property("IN_FORMATS"); prop && maskedModifier != DRM_FORMAT_MOD_INVALID) {
            drmModePropertyBlobPtr blob = drmModeGetPropertyBlob(m_DrmFd, prop->initialValue());
            if (blob) {
                auto *header = (struct drm_format_modifier_blob *)blob->data;
                auto *modifiers = (struct drm_format_modifier *)((char *)header + header->modifiers_offset);
                uint32_t *formats = (uint32_t *)((char *)header + header->formats_offset);

                for (uint32_t i = 0; i < header->count_modifiers; i++) {
                    if (modifiers[i].modifier == maskedModifier) {
                        for (uint32_t j = 0; j < header->count_formats && j < sizeof(modifiers[i].formats) * 8; j++) {
                            if (modifiers[i].formats & (1ULL << j)) {
                                if (formats[modifiers[i].offset + j] == drmFrame->layers[0].format) {
                                    formatMatch = true;
                                    break;
                                }
                            }
                        }

                        if (formatMatch) {
                            break;
                        }
                        else {
                            // Do not break for this case even though we got a modifier
                            // match that did not appear to have our format in it.
                            // To handle greater than 64 formats, the same modifier may
                            // appear in the list more than once.
                        }
                    }
                }

                drmModeFreePropertyBlob(blob);
            }
            else {
                // This should never happen since IN_FORMATS is an immutable property
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "drmModeGetPropertyBlob(IN_FORMATS) failed: %d",
                             errno);
            }
        }
        else {
            drmModePlanePtr videoPlane = drmModeGetPlane(m_DrmFd, m_VideoPlane.objectId());
            if (videoPlane) {
                for (uint32_t i = 0; i < videoPlane->count_formats; i++) {
                    if (drmFrame->layers[0].format == videoPlane->formats[i]) {
                        formatMatch = true;
                        break;
                    }
                }

                drmModeFreePlane(videoPlane);
            }
            else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "drmModeGetPlane() failed: %d",
                             errno);
            }
        }

        if (formatMatch) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Selected DRM plane supports chosen decoding format and modifier: " FOURCC_FMT " %016" PRIx64,
                        FOURCC_FMT_ARGS(drmFrame->layers[0].format),
                        drmFrame->objects[0].format_modifier);
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Selected DRM plane doesn't support chosen decoding format and modifier: " FOURCC_FMT " %016" PRIx64,
                         FOURCC_FMT_ARGS(drmFrame->layers[0].format),
                         drmFrame->objects[0].format_modifier);
            return false;
        }
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
            return false;
        }

        pitches[i] = layer.planes[i].pitch;
        offsets[i] = layer.planes[i].offset;
        modifiers[i] = object.format_modifier;

        // Pass along the modifiers to DRM if there are some in the descriptor
        // and the driver supports receiving modifiers on FBs
        if (modifiers[i] != DRM_FORMAT_MOD_INVALID && m_DrmSupportsModifiers) {
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
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmModeAddFB2[WithModifiers]() failed: %d",
                     err);
        return false;
    }

    return true;
}

bool DrmRenderer::drmFormatMatchesVideoFormat(uint32_t drmFormat, int videoFormat)
{
    auto drmToAvTuple = k_DrmToAvFormatMap.find(drmFormat);
    if (drmToAvTuple == k_DrmToAvFormatMap.end()) {
        return false;
    }

    const int expectedPixelDepth = (videoFormat & VIDEO_FORMAT_MASK_10BIT) ? 10 : 8;
    const int expectedLog2ChromaW = (videoFormat & VIDEO_FORMAT_MASK_YUV444) ? 0 : 1;
    const int expectedLog2ChromaH = (videoFormat & VIDEO_FORMAT_MASK_YUV444) ? 0 : 1;

    const AVPixFmtDescriptor* formatDesc = av_pix_fmt_desc_get(drmToAvTuple->second);
    if (!formatDesc) {
        // This shouldn't be possible but handle it anyway
        SDL_assert(formatDesc);
        return false;
    }

    return formatDesc->comp[0].depth == expectedPixelDepth &&
           formatDesc->log2_chroma_w == expectedLog2ChromaW &&
           formatDesc->log2_chroma_h == expectedLog2ChromaH;
}

void DrmRenderer::renderFrame(AVFrame* frame)
{
    SDL_assert(m_OutputRect.w > 0 && m_OutputRect.h > 0);

    // Register a frame buffer object for this frame
    uint32_t fbId;
    if (!addFbForFrame(frame, &fbId, false)) {
        return;
    }

    if (hasFrameFormatChanged(frame)) {
        SDL_Rect src, dst;
        src.x = src.y = 0;
        src.w = frame->width;
        src.h = frame->height;
        dst = m_OutputRect;

        StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

        // Set the video plane size and location
        m_PropSetter.configurePlane(m_VideoPlane, m_Crtc.objectId(),
                                    dst.x, dst.y,
                                    dst.w, dst.h,
                                    0, 0,
                                    frame->width << 16,
                                    frame->height << 16);

        // Set COLOR_RANGE property for the plane
        if (auto prop = m_VideoPlane.property("COLOR_RANGE")) {
            const char* desiredValue = getDrmColorRangeValue(frame);

            if (prop->containsValue(desiredValue)) {
                m_PropSetter.set(*prop, desiredValue);
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Unable to find matching COLOR_RANGE value for '%s'. Colors may be inaccurate!",
                            desiredValue);
            }
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "COLOR_RANGE property does not exist on video plane. Colors may be inaccurate!");
        }


        // Set COLOR_ENCODING property for the plane
        if (auto prop = m_VideoPlane.property("COLOR_ENCODING")) {
            const char* desiredValue = getDrmColorEncodingValue(frame);

            if (prop->containsValue(desiredValue)) {
                m_PropSetter.set(*prop, desiredValue);
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Unable to find matching COLOR_ENCODING value for '%s'. Colors may be inaccurate!",
                            desiredValue);
            }
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "COLOR_ENCODING property does not exist on video plane. Colors may be inaccurate!");
        }
    }

    // Update the video plane
    //
    // NB: This takes ownership of fbId, even on failure
    //
    // NB2: Pacer references the AVFrame (which also references the AVBuffers backing the frame
    // and the opaque_ref which may store our DRM-PRIME mapping) for frames backed by DMA-BUFs in
    // order to keep those from being reused by the decoder while they're still being scanned out.
    m_PropSetter.flipPlane(m_VideoPlane, fbId, 0);

    // Apply pending atomic transaction (if in atomic mode)
    m_PropSetter.apply();
}

bool DrmRenderer::testRenderFrame(AVFrame* frame) {
    uint32_t fbId;

    // If we don't even have a plane, we certainly can't render
    if (!m_VideoPlane.isValid()) {
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
    if (auto prop = m_VideoPlane.property("COLOR_ENCODING")) {
        if (prop->containsValue("ITU-R BT.601 YCbCr")) {
            return COLORSPACE_REC_601;
        }
        else if (prop->containsValue("ITU-R BT.709 YCbCr")) {
            return COLORSPACE_REC_709;
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

    return m_EglImageFactory.exportDRMImages(frame, dpy, images);
}

#endif
