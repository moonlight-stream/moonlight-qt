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

#define _GNU_SOURCE

// NOTE: This file MUST NOT include fcntl.h due to open() -> open64()
// redirection that happens when _FILE_OFFSET_BITS=64!
// See masterhook_internal.c for details.

#if HAVE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

// We require SDL 2.0.15+ to hook because it supports sharing
// the DRM FD with our code. This avoids having multiple DRM FDs
// in flight at the same time which would significantly complicate
// the logic here because we'd need to figure out exactly which FD
// should be the master at any given time. With the position of our
// hooks, that is definitely not trivial.
#if SDL_VERSION_ATLEAST(2, 0, 15)

// Qt's DRM master FD grabbed by our hook
int g_QtDrmMasterFd = -1;
struct stat g_DrmMasterStat;

// The DRM master FD created for SDL
int g_SdlDrmMasterFd = -1;

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
    // Grab the first DRM Master FD that makes it in here. This will be the Qt
    // EGLFS backend's DRM FD, on which we will call drmDropMaster() later.
    if (g_QtDrmMasterFd == -1 && drmIsMaster(fd)) {
        g_QtDrmMasterFd = fd;
        fstat(g_QtDrmMasterFd, &g_DrmMasterStat);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Captured Qt EGLFS DRM master fd (legacy): %d",
                    g_QtDrmMasterFd);
    }

    // Call into the real thing
    return ((typeof(drmModeSetCrtc)*)dlsym(RTLD_NEXT, __FUNCTION__))(fd, crtcId, bufferId, x, y, connectors, count, mode);
}

// This hook will handle atomic DRM rendering
int drmModeAtomicCommit(int fd, drmModeAtomicReqPtr req,
                        uint32_t flags, void *user_data)
{
    // Grab the first DRM Master FD that makes it in here. This will be the Qt
    // EGLFS backend's DRM FD, on which we will call drmDropMaster() later.
    if (g_QtDrmMasterFd == -1 && drmIsMaster(fd)) {
        g_QtDrmMasterFd = fd;
        fstat(g_QtDrmMasterFd, &g_DrmMasterStat);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Captured Qt EGLFS DRM master fd (atomic): %d",
                    g_QtDrmMasterFd);
    }

    // Call into the real thing
    return ((typeof(drmModeAtomicCommit)*)dlsym(RTLD_NEXT, __FUNCTION__))(fd, req, flags, user_data);
}

// This hook will handle SDL's open() on the DRM device. We just need to
// hook this variant of open(), since that's what SDL uses. When we see
// the open a FD for the same card as the Qt DRM master FD, we'll drop
// master on the Qt FD to allow the new FD to have master.
int openHook(const char *funcname, const char *pathname, int flags, va_list va);

int open(const char *pathname, int flags, ...)
{
    va_list va;
    va_start(va, flags);
    int fd = openHook(__FUNCTION__, pathname, flags, va);
    va_end(va);
    return fd;
}

int open64(const char *pathname, int flags, ...)
{
    va_list va;
    va_start(va, flags);
    int fd = openHook(__FUNCTION__, pathname, flags, va);
    va_end(va);
    return fd;
}

// Our close() hook handles restoring DRM master to the Qt FD
// after SDL closes its DRM FD.
int close(int fd)
{
    // Call the real thing
    int ret = ((typeof(close)*)dlsym(RTLD_NEXT, __FUNCTION__))(fd);
    if (ret == 0) {
        // If we just closed the SDL DRM master FD, restore master
        // to the Qt DRM FD. This works because the Qt DRM master FD
        // was master once before, so we can set it as master again
        // using drmSetMaster() without CAP_SYS_ADMIN.
        if (g_SdlDrmMasterFd != -1 && fd == g_SdlDrmMasterFd) {
            if (drmSetMaster(g_QtDrmMasterFd) < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to restore master to Qt DRM FD: %d",
                             errno);
            }

            g_SdlDrmMasterFd = -1;
        }
    }
    return ret;
}

#endif
