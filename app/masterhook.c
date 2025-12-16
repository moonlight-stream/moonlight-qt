// This file contains hooks for several functions that allow
// Qt and SDL to more or less share DRM master ownership.
//
// This technique requires Linux v5.8 or later, or for Moonlight
// to run as root (with CAP_SYS_ADMIN). Prior to Linux v5.8,
// DRM_IOCTL_DROP_MASTER required CAP_SYS_ADMIN, which prevents
// our trick from working (without root, that is).
//
// The specific kernel change required to run without root is:
// https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=45bc3d26c95a8fc63a7d8668ca9e57ef0883351c

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// NOTE: This file MUST NOT include fcntl.h due to open() -> open64()
// redirection that happens when _FILE_OFFSET_BITS=64!
// See masterhook_internal.c for details.

#include "SDL_compat.h"
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <pthread.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

// We require SDL 2.0.15+ to hook because it supports sharing
// the DRM FD with our code. This avoids having multiple DRM FDs
// in flight at the same time which would significantly complicate
// the logic here because we'd need to figure out exactly which FD
// should be the master at any given time. With the position of our
// hooks, that is definitely not trivial.
#if SDL_VERSION_ATLEAST(2, 0, 15)

// We don't include fcntl.h, so we have to define this ourselves
typedef int (*fn_open_t)(const char *pathname, int flags, ...);

// Pointers to the real implementations of these libdrm functions
static pthread_once_t s_InitDrmFunctions = PTHREAD_ONCE_INIT;
static typeof(drmModeSetCrtc)* fn_drmModeSetCrtc;
static typeof(drmModePageFlip)* fn_drmModePageFlip;
static typeof(drmModeAtomicCommit)* fn_drmModeAtomicCommit;

static void lookupRealDrmFunctions() {
    fn_drmModeSetCrtc = dlsym(RTLD_NEXT, "drmModeSetCrtc");
    fn_drmModePageFlip = dlsym(RTLD_NEXT, "drmModePageFlip");
    fn_drmModeAtomicCommit = dlsym(RTLD_NEXT, "drmModeAtomicCommit");
}

// Pointers to the real implementations of these libc functions
static pthread_once_t s_InitLibCFunctions = PTHREAD_ONCE_INIT;
static fn_open_t *fn_open;
static fn_open_t *fn_open64;
static typeof(close) *fn_close;

static void lookupRealLibCFunctions() {
    fn_open = dlsym(RTLD_NEXT, "open");
    fn_open64 = dlsym(RTLD_NEXT, "open64");
    fn_close = dlsym(RTLD_NEXT, "close");
}

// Qt's DRM master FD grabbed by our hook
int g_QtDrmMasterFd = -1;
struct stat g_DrmMasterStat;

// DRM lease FD for video renderer (if lease creation succeeds)
int g_DrmLeaseFd = -1;
bool g_LeaseAttempted = false;

// Last CRTC state for us to restore later
drmModeCrtcPtr g_QtCrtcState;
uint32_t* g_QtCrtcConnectors;
int g_QtCrtcConnectorCount;

bool removeSdlFd(int fd);
int takeMasterFromSdlFd(void);

// Try to create a DRM lease for the video renderer
// This is called once we have identified the CRTC/connector/plane from Qt's atomic commit
void attemptDrmLease(int fd, drmModeAtomicReqPtr req)
{
    (void)req; // Unused - we query current config instead of inspecting the atomic request

    // Only try once
    if (g_LeaseAttempted || fd != g_QtDrmMasterFd) {
        return;
    }
    g_LeaseAttempted = true;

    // We need to extract the CRTC, connector, and plane IDs from the atomic request
    // Unfortunately libdrm doesn't expose a way to inspect the atomic request,
    // so we'll need to query the current configuration instead
    drmModeResPtr resources = drmModeGetResources(fd);
    if (!resources) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to get DRM resources for lease creation");
        return;
    }

    // Find the active CRTC (the one Qt is using)
    uint32_t crtcId = 0;
    uint32_t connectorId = 0;
    for (int i = 0; i < resources->count_crtcs; i++) {
        drmModeCrtcPtr crtc = drmModeGetCrtc(fd, resources->crtcs[i]);
        if (crtc && crtc->buffer_id != 0) {
            crtcId = crtc->crtc_id;
            // Find the connector for this CRTC
            for (int j = 0; j < resources->count_connectors; j++) {
                drmModeConnectorPtr connector = drmModeGetConnector(fd, resources->connectors[j]);
                if (connector && connector->encoder_id != 0) {
                    drmModeEncoderPtr encoder = drmModeGetEncoder(fd, connector->encoder_id);
                    if (encoder && encoder->crtc_id == crtcId) {
                        connectorId = connector->connector_id;
                        drmModeFreeEncoder(encoder);
                        drmModeFreeConnector(connector);
                        break;
                    }
                    if (encoder) drmModeFreeEncoder(encoder);
                }
                if (connector) drmModeFreeConnector(connector);
            }
            drmModeFreeCrtc(crtc);
            break;
        }
        if (crtc) drmModeFreeCrtc(crtc);
    }

    // Find the CRTC index for bitmask checks
    uint32_t crtcIndex = 0;
    for (int i = 0; i < resources->count_crtcs; i++) {
        if (resources->crtcs[i] == crtcId) {
            crtcIndex = i;
            break;
        }
    }

    // Find an overlay plane (we'll use the first overlay plane we find)
    drmModePlaneResPtr planeRes = drmModeGetPlaneResources(fd);
    uint32_t planeId = 0;
    if (planeRes) {
        for (uint32_t i = 0; i < planeRes->count_planes; i++) {
            drmModePlanePtr plane = drmModeGetPlane(fd, planeRes->planes[i]);
            if (plane) {
                // Check if this plane can be used with our CRTC
                if (plane->possible_crtcs & (1 << crtcIndex)) {
                    planeId = plane->plane_id;
                    drmModeFreePlane(plane);
                    break;
                }
                drmModeFreePlane(plane);
            }
        }
        drmModeFreePlaneResources(planeRes);
    }

    drmModeFreeResources(resources);

    if (crtcId == 0 || connectorId == 0 || planeId == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to identify CRTC/connector/plane for DRM lease (CRTC=%u, connector=%u, plane=%u)",
                    crtcId, connectorId, planeId);
        return;
    }

    // Try to create the lease
    uint32_t objects[] = {crtcId, connectorId, planeId};
    g_DrmLeaseFd = drmModeCreateLease(fd, objects, 3, 0, NULL);
    if (g_DrmLeaseFd >= 0) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Created DRM lease for video renderer: CRTC %u, connector %u, plane %u (lease FD %d)",
                    crtcId, connectorId, planeId, g_DrmLeaseFd);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to create DRM lease (errno=%d), video renderer will share DRM master",
                    errno);
    }
}

int drmIsMaster(int fd)
{
    // Detect master by attempting something that requires master.
    // This method is available in Mesa DRM since Feb 2019.
    return drmAuthMagic(fd, 0) != -EACCES;
}

// This hook will handle legacy DRM rendering
int drmModeSetCrtc(int fd, uint32_t crtcId, uint32_t bufferId,
                   uint32_t x, uint32_t y, uint32_t *connectors, int count,
                   drmModeModeInfoPtr mode)
{
    // Lookup the real libdrm function pointers if we haven't yet
    pthread_once(&s_InitDrmFunctions, lookupRealDrmFunctions);

    // Grab the first DRM Master FD that makes it in here. This will be the Qt
    // EGLFS backend's DRM FD, on which we will call drmDropMaster() later.
    if (g_QtDrmMasterFd == -1 && drmIsMaster(fd)) {
        g_QtDrmMasterFd = fd;
        fstat(g_QtDrmMasterFd, &g_DrmMasterStat);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Captured Qt EGLFS DRM master fd (legacy): %d",
                    g_QtDrmMasterFd);

        // Try to create a DRM lease for the video renderer
        attemptDrmLease(fd, NULL);
    }

    // Call into the real thing
    int err = fn_drmModeSetCrtc(fd, crtcId, bufferId, x, y, connectors, count, mode);
    if (err == 0 && fd == g_QtDrmMasterFd) {
        // Free old CRTC state (if any)
        if (g_QtCrtcState) {
            drmModeFreeCrtc(g_QtCrtcState);
        }
        if (g_QtCrtcConnectors) {
            free(g_QtCrtcConnectors);
        }

        // Store the CRTC configuration so we can restore it later
        g_QtCrtcState = drmModeGetCrtc(fd, crtcId);
        g_QtCrtcConnectors = calloc(count, sizeof(*g_QtCrtcConnectors));
        memcpy(g_QtCrtcConnectors, connectors, count * sizeof(*connectors));
        g_QtCrtcConnectorCount = count;
    }
    return err;
}

// This hook will temporarily retake DRM master to allow Qt to render while SDL has a DRM FD open
int drmModePageFlip(int fd, uint32_t crtc_id, uint32_t fb_id, uint32_t flags, void *user_data)
{
    // Lookup the real libdrm function pointers if we haven't yet
    pthread_once(&s_InitDrmFunctions, lookupRealDrmFunctions);

    // Call into the real thing
    int err = fn_drmModePageFlip(fd, crtc_id, fb_id, flags, user_data);
    if (err == -EACCES && fd == g_QtDrmMasterFd) {
        // If SDL took master from us, try to grab it back temporarily
        int oldMasterFd = takeMasterFromSdlFd();
        drmSetMaster(fd);
        err = fn_drmModePageFlip(fd, crtc_id, fb_id, flags, user_data);
        drmDropMaster(fd);
        if (oldMasterFd != -1) {
            drmSetMaster(oldMasterFd);
        }
    }
    return err;
}

// This hook will handle atomic DRM rendering
int drmModeAtomicCommit(int fd, drmModeAtomicReqPtr req,
                        uint32_t flags, void *user_data)
{
    // Lookup the real libdrm function pointers if we haven't yet
    pthread_once(&s_InitDrmFunctions, lookupRealDrmFunctions);

    // Grab the first DRM Master FD that makes it in here. This will be the Qt
    // EGLFS backend's DRM FD, on which we will call drmDropMaster() later.
    if (g_QtDrmMasterFd == -1 && drmIsMaster(fd)) {
        g_QtDrmMasterFd = fd;
        fstat(g_QtDrmMasterFd, &g_DrmMasterStat);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Captured Qt EGLFS DRM master fd (atomic): %d",
                    g_QtDrmMasterFd);

        // Try to create a DRM lease for the video renderer
        attemptDrmLease(fd, req);
    }

    // Call into the real thing
    int err = fn_drmModeAtomicCommit(fd, req, flags, user_data);
    if (err == -EACCES && fd == g_QtDrmMasterFd) {
        // If SDL took master from us, try to grab it back temporarily
        int oldMasterFd = takeMasterFromSdlFd();
        drmSetMaster(fd);
        err = fn_drmModeAtomicCommit(fd, req, flags, user_data);
        drmDropMaster(fd);
        if (oldMasterFd != -1) {
            drmSetMaster(oldMasterFd);
        }
    }
    return err;
}

// This hook will handle SDL's open() on the DRM device. We just need to
// hook this variant of open(), since that's what SDL uses. When we see
// the open a FD for the same card as the Qt DRM master FD, we'll drop
// master on the Qt FD to allow the new FD to have master.
int openHook(fn_open_t *real_open, typeof(close) *real_close, const char *pathname, int flags, va_list va);

int open(const char *pathname, int flags, ...)
{
    // Lookup the real libc functions if we haven't yet
    pthread_once(&s_InitLibCFunctions, lookupRealLibCFunctions);

    va_list va;
    va_start(va, flags);
    int fd = openHook(fn_open, fn_close, pathname, flags, va);
    va_end(va);
    return fd;
}

int open64(const char *pathname, int flags, ...)
{
    // Lookup the real libc functions if we haven't yet
    pthread_once(&s_InitLibCFunctions, lookupRealLibCFunctions);

    va_list va;
    va_start(va, flags);
    int fd = openHook(fn_open64, fn_close, pathname, flags, va);
    va_end(va);
    return fd;
}

// Our close() hook handles restoring DRM master to the Qt FD
// after SDL closes its DRM FD.
int close(int fd)
{
    // Lookup the real libc functions if we haven't yet
    pthread_once(&s_InitLibCFunctions, lookupRealLibCFunctions);

    // Remove this entry from the SDL FD table
    bool lastSdlFd = removeSdlFd(fd);

    // Call the real thing
    int ret = fn_close(fd);

    // If we closed the last SDL FD, restore master to the Qt FD
    if (ret == 0 && lastSdlFd) {
        if (drmSetMaster(g_QtDrmMasterFd) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to restore master to Qt DRM FD: %d",
                         errno);
        }

        // Reset the CRTC state to how Qt configured it
        if (g_QtCrtcState) {
            SDL_assert(fn_drmModeSetCrtc != NULL);
            int err = fn_drmModeSetCrtc(g_QtDrmMasterFd,
                                        g_QtCrtcState->crtc_id,
                                        g_QtCrtcState->buffer_id,
                                        g_QtCrtcState->x,
                                        g_QtCrtcState->y,
                                        g_QtCrtcConnectors,
                                        g_QtCrtcConnectorCount,
                                        g_QtCrtcState->mode_valid ?
                                              &g_QtCrtcState->mode : NULL);
            if (err < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to restore CRTC state to Qt DRM FD: %d",
                             errno);
            }
        }
    }

    return ret;
}

#endif
