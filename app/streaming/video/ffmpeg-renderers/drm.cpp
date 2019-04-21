#include "drm.h"

extern "C" {
    #include <libavutil/hwcontext_drm.h>
}

#include <libdrm/drm_fourcc.h>

#include <unistd.h>
#include <fcntl.h>

#include "streaming/streamutils.h"
#include "streaming/session.h"

#include <Limelight.h>

DrmRenderer::DrmRenderer()
    : m_DrmFd(-1),
      m_CrtcId(0),
      m_PlaneId(0),
      m_CurrentFbId(0)
{
}

DrmRenderer::~DrmRenderer()
{
    if (m_CurrentFbId != 0) {
        drmModeRmFB(m_DrmFd, m_CurrentFbId);
    }

    if (m_DrmFd != -1) {
        close(m_DrmFd);
    }
}

bool DrmRenderer::prepareDecoderContext(AVCodecContext*)
{
    /* Nothing to do */

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using DRM renderer");

    return true;
}

bool DrmRenderer::initialize(PDECODER_PARAMETERS)
{
    const char* device = SDL_getenv("DRM_DEV");
    int i;

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
        return false;
    }

    drmModeRes* resources = drmModeGetResources(m_DrmFd);
    if (resources == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "drmModeGetResources() failed: %d",
                 errno);
        return false;
    }

    // Look for a connected connector and get the associated encoder
    uint32_t encoderId = 0;
    for (i = 0; i < resources->count_connectors && encoderId == 0; i++) {
        drmModeConnector* connector = drmModeGetConnector(m_DrmFd, resources->connectors[i]);
        if (connector != nullptr) {
            if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
                encoderId = connector->encoder_id;
            }

            drmModeFreeConnector(connector);
        }
    }

    if (encoderId == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "No connected displays found!");
        drmModeFreeResources(resources);
        return false;
    }

    // Now find the CRTC from the encoder
    m_CrtcId = 0;
    for (i = 0; i < resources->count_encoders && m_CrtcId == 0; i++) {
        drmModeEncoder* encoder = drmModeGetEncoder(m_DrmFd, resources->encoders[i]);
        if (encoder != nullptr) {
            if (encoder->encoder_id == encoderId) {
                m_CrtcId = encoder->crtc_id;
            }

            drmModeFreeEncoder(encoder);
        }
    }

    if (m_CrtcId == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DRM encoder not found!");
        drmModeFreeResources(resources);
        return false;
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
        return false;
    }

    drmSetClientCap(m_DrmFd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

    drmModePlaneRes* planeRes = drmModeGetPlaneResources(m_DrmFd);
    if (planeRes == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmGetPlaneResources() failed: %d",
                     errno);
        return false;
    }

    // Find an NV12 overlay plane to render on
    m_PlaneId = 0;
    for (uint32_t i = 0; i < planeRes->count_planes && m_PlaneId == 0; i++) {
        drmModePlane* plane = drmModeGetPlane(m_DrmFd, planeRes->planes[i]);
        if (plane != nullptr) {
            bool matchingFormat = false;
            for (uint32_t j = 0; j < plane->count_formats; j++) {
                if (plane->formats[j] == DRM_FORMAT_NV12) {
                    matchingFormat = true;
                    break;
                }
            }

            if (matchingFormat == false) {
                drmModeFreePlane(plane);
                continue;
            }

            if ((plane->possible_crtcs & (1 << crtcIndex)) && plane->crtc_id == 0) {
                drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(m_DrmFd, planeRes->planes[i], DRM_MODE_OBJECT_PLANE);
                if (props != nullptr) {
                    for (uint32_t j = 0; j < props->count_props && m_PlaneId == 0; j++) {
                        drmModePropertyPtr prop = drmModeGetProperty(m_DrmFd, props->props[j]);
                        if (prop != nullptr) {
                            if (!strcmp(prop->name, "type") && props->prop_values[j] == DRM_PLANE_TYPE_OVERLAY) {
                                m_PlaneId = plane->plane_id;
                            }

                            drmModeFreeProperty(prop);
                        }
                    }

                    drmModeFreeObjectProperties(props);
                }
            }

            drmModeFreePlane(plane);
        }
    }

    drmModeFreePlaneResources(planeRes);

    return true;
}

enum AVPixelFormat DrmRenderer::getPreferredPixelFormat(int)
{
    // DRM PRIME buffers
    return AV_PIX_FMT_DRM_PRIME;
}

void DrmRenderer::renderFrame(AVFrame* frame)
{
    AVDRMFrameDescriptor* drmFrame = (AVDRMFrameDescriptor*)frame->data[0];
    int err;
    uint32_t primeHandle;
    uint32_t handles[4] = {};
    uint32_t pitches[4] = {};
    uint32_t offsets[4] = {};

    SDL_Rect src, dst;

    src.x = src.y = 0;
    src.w = frame->width;
    src.h = frame->height;
    dst = m_OutputRect;

    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    // Convert the FD in the AVDRMFrameDescriptor to a PRIME handle
    // that can be used in drmModeAddFB2()
    SDL_assert(drmFrame->nb_objects == 1);
    err = drmPrimeFDToHandle(m_DrmFd, drmFrame->objects[0].fd, &primeHandle);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmPrimeFDToHandle() failed: %d",
                     errno);
        return;
    }

    SDL_assert(drmFrame->nb_layers == 1);
    SDL_assert(drmFrame->layers[0].nb_planes == 2);
    for (int i = 0; i < drmFrame->layers[0].nb_planes; i++) {
        handles[i] = primeHandle;
        pitches[i] = drmFrame->layers[0].planes[i].pitch;
        offsets[i] = drmFrame->layers[0].planes[i].offset;
    }

    // Remember the last FB object we created so we can free it
    // when we are finished rendering this one (if successful).
    uint32_t lastFbId = m_CurrentFbId;

    // Create a frame buffer object from the PRIME buffer
    err = drmModeAddFB2(m_DrmFd, frame->width, frame->height,
                        drmFrame->layers[0].format,
                        handles, pitches, offsets, &m_CurrentFbId, 0);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "drmModeAddFB2() failed: %d",
                     errno);
        m_CurrentFbId = lastFbId;
        return;
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
