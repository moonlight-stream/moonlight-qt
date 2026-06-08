// This file contains the private implementation of the open() hook
// which must be in a separate compilation unit due to fcntl.h doing
// unwanted redirection of open() to open64().

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "SDL_compat.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <pthread.h>

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
extern bool g_DisableDrmHooks;

#define MAX_SDL_FD_COUNT 8
int g_SdlDrmMasterFds[MAX_SDL_FD_COUNT] = {-1, -1, -1, -1, -1, -1, -1, -1};
int g_SdlDrmMasterFdCount = 0;
pthread_mutex_t g_FdTableLock = PTHREAD_MUTEX_INITIALIZER;

// This lock protects sections of code that must run uninterrupted with DRM master
pthread_mutex_t g_MasterLock;
pthread_once_t g_MasterLockInit;

// Lock order: g_MasterLock -> g_FdTableLock

static void initializeMasterLock(void)
{
    pthread_mutexattr_t attr;

    // g_MasterLock must be a recursive mutex because we can't fully
    // predict how SDL and Vulkan/GL driver code will behave. They may
    // call functions that trigger reentrancy into our hooks again,
    // which can deadlock if the caller already holds the master lock.
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_MasterLock, &attr);
    pthread_mutexattr_destroy(&attr);
}

void lockDrmMaster()
{
    pthread_once(&g_MasterLockInit, initializeMasterLock);
    pthread_mutex_lock(&g_MasterLock);
}

void unlockDrmMaster()
{
    pthread_mutex_unlock(&g_MasterLock);
}

// Caller must hold g_FdTableLock
int getSdlFdEntryIndex(bool unused)
{
    for (int i = 0; i < MAX_SDL_FD_COUNT; i++) {
        if (unused && g_SdlDrmMasterFds[i] < 0) {
            return i;
        }
        else if (!unused && g_SdlDrmMasterFds[i] >= 0) {
            return i;
        }
    }

    return -1;
}

// Returns true if the final SDL FD was removed
bool removeSdlFd(int fd)
{
    // -1 will break our logic below that looks for matches
    // in the entire array (which we must do because it may
    // not be contiguously populated).
    if (fd == -1) {
        return false;
    }

    pthread_mutex_lock(&g_FdTableLock);
    if (g_SdlDrmMasterFdCount != 0) {
        // Clear the entry for this fd from the table
        for (int i = 0; i < MAX_SDL_FD_COUNT; i++) {
            if (fd == g_SdlDrmMasterFds[i]) {
                g_SdlDrmMasterFds[i] = -1;
                g_SdlDrmMasterFdCount--;
                break;
            }
        }

        if (g_SdlDrmMasterFdCount == 0) {
            pthread_mutex_unlock(&g_FdTableLock);
            return true;
        }
    }
    pthread_mutex_unlock(&g_FdTableLock);
    return false;
}

// Returns the previous master FD or -1 if none
int takeMasterFromSdlFd()
{
    int fd = -1;

    // Since all SDL FDs are actually dups of each other
    // we can take master from any one of them.
    pthread_mutex_lock(&g_FdTableLock);
    int fdIndex = getSdlFdEntryIndex(false);
    if (fdIndex != -1) {
        fd = g_SdlDrmMasterFds[fdIndex];
    }
    pthread_mutex_unlock(&g_FdTableLock);

    if (fd >= 0 && drmDropMaster(fd) == 0) {
        return fd;
    }
    else {
        return -1;
    }
}

int openHook(typeof(open) *real_open, typeof(close) *real_close, const char *pathname, int flags, va_list va)
{
    int fd;
    mode_t mode;

    // Call the real thing to do the open operation
    if (__OPEN_NEEDS_MODE(flags)) {
        mode = va_arg(va, mode_t);
        fd = real_open(pathname, flags, mode);
    }
    else {
        mode = 0;
        fd = real_open(pathname, flags);
    }

    if (g_DisableDrmHooks) {
        return fd;
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
                int freeFdIndex;
                int allocatedFdIndex;

                // Prevent other threads from stealing DRM master for now
                lockDrmMaster();

                // It is our device. Time to do the magic!
                pthread_mutex_lock(&g_FdTableLock);

                // Get a free index for us to put the new entry
                freeFdIndex = getSdlFdEntryIndex(true);
                if (freeFdIndex < 0) {
                    pthread_mutex_unlock(&g_FdTableLock);
                    unlockDrmMaster();
                    SDL_assert(freeFdIndex >= 0);
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "No unused SDL FD table entries!");
                    // Hope for the best
                    return fd;
                }

                // Check if we have an allocated entry already
                allocatedFdIndex = getSdlFdEntryIndex(false);
                if (allocatedFdIndex >= 0) {
                    // Close fd that we opened earlier (skipping our close() hook)
                    real_close(fd);

                    // dup() an existing FD into the unused slot
                    fd = dup(g_SdlDrmMasterFds[allocatedFdIndex]);
                }
                else {
                    // Drop master on Qt's FD so we can pick it up for SDL.
                    if (drmDropMaster(g_QtDrmMasterFd) < 0) {
                        pthread_mutex_unlock(&g_FdTableLock);
                        unlockDrmMaster();
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                     "Failed to drop master on Qt DRM FD: %d",
                                     errno);
                        // Hope for the best
                        return fd;
                    }

                    // Close fd that we opened earlier (skipping our close() hook)
                    real_close(fd);

                    // We are not allowed to call drmSetMaster() without CAP_SYS_ADMIN,
                    // but since we just dropped the master, we can become master by
                    // simply creating a new FD. Let's do it.
                    if (__OPEN_NEEDS_MODE(flags)) {
                        fd = real_open(pathname, flags, mode);
                    }
                    else {
                        fd = real_open(pathname, flags);
                    }
                }

                if (fd >= 0) {
                    // Start with DRM master on the new FD
                    drmSetMaster(fd);

                    // Insert the FD into the table
                    g_SdlDrmMasterFds[freeFdIndex] = fd;
                    g_SdlDrmMasterFdCount++;
                }

                pthread_mutex_unlock(&g_FdTableLock);

                unlockDrmMaster();
            }
        }
    }

    return fd;
}
#endif
