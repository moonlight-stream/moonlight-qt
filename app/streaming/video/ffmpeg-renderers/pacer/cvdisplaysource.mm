#include "cvdisplaysource.h"

#ifndef __APPLE__
#warning Unable to use CVDisplaySource outside of macOS
#else

#import <AppKit/AppKit.h>
#include <SDL_syswm.h>

#include <mach/machine.h>
#include <sys/sysctl.h>

// Same check as VTBaseRenderer::isAppleSilicon(), which we can't reach from here.
// Display link presentation is unreliable on Intel (see ccaca685).
static bool isAppleSilicon() {
    static uint32_t cpuType = 0;
    if (cpuType == 0) {
        size_t size = sizeof(cpuType);
        int err = sysctlbyname("hw.cputype", &cpuType, &size, NULL, 0);
        if (err != 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "CVDisplaySource: sysctlbyname(hw.cputype) failed: %d", err);
            return false;
        }
    }

    // Apple Silicon Macs have CPU_ARCH_ABI64 set, so we need to mask that off.
    // For some reason, 64-bit Intel Macs don't seem to have CPU_ARCH_ABI64 set.
    return (cpuType & ~CPU_ARCH_MASK) == CPU_TYPE_ARM;
}

CVDisplaySource::CVDisplaySource(Pacer* pacer) :
    m_Pacer(pacer),
    m_DisplayLink(nullptr) {
}

CVDisplaySource::~CVDisplaySource() {
    if (m_DisplayLink != nullptr) {
        // Stop() waits for the IO thread, so the callback can't be running after it.
        CVDisplayLinkStop(m_DisplayLink);
        CVDisplayLinkRelease(m_DisplayLink);
        m_DisplayLink = nullptr;
    }
}

CVReturn CVDisplaySource::displayLinkCallback(CVDisplayLinkRef displayLink,
                                              const CVTimeStamp* now,
                                              const CVTimeStamp* vsyncTime,
                                              CVOptionFlags flags,
                                              CVOptionFlags* newFlags,
                                              void* context) {
    auto self = reinterpret_cast<CVDisplaySource*>(context);
    self->m_Pacer->signalVsync();
    return kCVReturnSuccess;
}

bool CVDisplaySource::initialize(SDL_Window* window, int displayFps) {
    if (!isAppleSilicon()) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "CVDisplaySource: frame pacing unsupported on Intel Macs");
        return false;
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_COCOA) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CVDisplaySource: SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return false;
    }

    CVDisplayLinkRef displayLink = nullptr;
    CVReturn status;

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    NSWindow* nsWindow = (NSWindow*)info.info.cocoa.window;
    NSScreen* screen = (nsWindow != nil) ? [nsWindow screen] : nil;
    if (screen != nil) {
        CGDirectDisplayID displayId =
            [[screen deviceDescription][@"NSScreenNumber"] unsignedIntValue];
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "CVDisplaySource: using display 0x%x (reported %d Hz)",
                    displayId, displayFps);
        status = CVDisplayLinkCreateWithCGDisplay(displayId, &displayLink);
    }
    else {
        // Window not visible on any display, so use all active displays.
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "CVDisplaySource: window not on a display yet");
        status = CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
    }
    [pool release];

    if (status != kCVReturnSuccess || displayLink == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CVDisplaySource: CVDisplayLinkCreate failed: %d",
                     (int)status);
        return false;
    }

    status = CVDisplayLinkSetOutputCallback(displayLink, displayLinkCallback, this);
    if (status != kCVReturnSuccess) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CVDisplaySource: CVDisplayLinkSetOutputCallback failed: %d",
                     (int)status);
        CVDisplayLinkRelease(displayLink);
        return false;
    }

    status = CVDisplayLinkStart(displayLink);
    if (status != kCVReturnSuccess) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CVDisplaySource: CVDisplayLinkStart failed: %d",
                     (int)status);
        CVDisplayLinkRelease(displayLink);
        return false;
    }

    m_DisplayLink = displayLink;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "CVDisplaySource initialized (display %d Hz)", displayFps);
    return true;
}
#endif // __APPLE__
