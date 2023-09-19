// This file contains the private implementation of the open() hook
// which must be in a separate compilation unit due to fcntl.h doing
// unwanted redirection of open() to open64().

#define _GNU_SOURCE

#include <SDL.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

// __OPEN_NEEDS_MODE is a glibc-ism, so define it ourselves for other libc
#ifndef __OPEN_NEEDS_MODE
#ifdef __O_TMPFILE
# define __OPEN_NEEDS_MODE(oflag) \
  (((oflag) & O_CREAT) != 0 || ((oflag) & __O_TMPFILE) == __O_TMPFILE)
#else
# define __OPEN_NEEDS_MODE(oflag) (((oflag) & O_CREAT) != 0)
#endif
#endif

#if SDL_VERSION_ATLEAST(2, 0, 15)

extern int g_QtDrmMasterFd;
extern struct stat g_DrmMasterStat;
extern int g_SdlDrmMasterFd;

int openHook(const char *funcname, const char *pathname, int flags, va_list va)
{
    int fd;
    mode_t mode;

    // Call the real thing to do the open operation
    if (__OPEN_NEEDS_MODE(flags)) {
        mode = va_arg(va, mode_t);
        fd = ((typeof(open)*)dlsym(RTLD_NEXT, funcname))(pathname, flags, mode);
    }
    else {
        mode = 0;
        fd = ((typeof(open)*)dlsym(RTLD_NEXT, funcname))(pathname, flags);
    }

    // If the file was successfully opened and we have a DRM master FD,
    // check if the FD we just opened is a DRM device.
    if (fd >= 0 && g_QtDrmMasterFd != -1) {
        if (strncmp(pathname, "/dev/dri/card", 13) == 0) {
            // It's a DRM device, but is it _our_ DRM device?
            struct stat fdstat;

            fstat(fd, &fdstat);
            if (g_DrmMasterStat.st_dev == fdstat.st_dev &&
                    g_DrmMasterStat.st_ino == fdstat.st_ino) {
                // It is our device. Time to do the magic!

                // This code assumes SDL only ever opens a single FD
                // for a given DRM device.
                SDL_assert(g_SdlDrmMasterFd == -1);

                // Drop master on Qt's FD so we can pick it up for SDL.
                if (drmDropMaster(g_QtDrmMasterFd) < 0) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "Failed to drop master on Qt DRM FD: %d",
                                 errno);
                    // Hope for the best
                    return fd;
                }

                // We are not allowed to call drmSetMaster() without CAP_SYS_ADMIN,
                // but since we just dropped the master, we can become master by
                // simply creating a new FD. Let's do it.
                close(fd);
                if (__OPEN_NEEDS_MODE(flags)) {
                    fd = ((typeof(open)*)dlsym(RTLD_NEXT, funcname))(pathname, flags, mode);
                }
                else {
                    fd = ((typeof(open)*)dlsym(RTLD_NEXT, funcname))(pathname, flags);
                }
                g_SdlDrmMasterFd = fd;
            }
        }
    }

    return fd;
}
#endif
