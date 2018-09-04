#include "displaylinkvsyncsource.h"

#include <SDL_syswm.h>

#include <CoreVideo/CoreVideo.h>

#import <Cocoa/Cocoa.h>

class DisplayLinkVsyncSource : public IVsyncSource
{
public:
    DisplayLinkVsyncSource(Pacer* pacer)
        : m_Pacer(pacer),
          m_Window(nullptr),
          m_DisplayLink(nullptr),
          m_DisplayId(UINT_MAX)
    {

    }

    virtual ~DisplayLinkVsyncSource() override
    {
        if (m_DisplayLink != nullptr) {
            CVDisplayLinkStop(m_DisplayLink);
            CVDisplayLinkRelease(m_DisplayLink);
        }
    }

    static
    CGDirectDisplayID
    getDisplayID(NSScreen* screen)
    {
        NSNumber* screenNumber = [screen deviceDescription][@"NSScreenNumber"];
        return [screenNumber unsignedIntValue];
    }

    static
    CVReturn
    displayLinkOutputCallback(
        CVDisplayLinkRef displayLink,
        const CVTimeStamp* /* now */,
        const CVTimeStamp* /* vsyncTime */,
        CVOptionFlags,
        CVOptionFlags*,
        void *displayLinkContext)
    {
        auto me = reinterpret_cast<DisplayLinkVsyncSource*>(displayLinkContext);

        SDL_assert(displayLink == me->m_DisplayLink);

        NSScreen* screen = [me->m_Window screen];
        if (screen != nullptr && getDisplayID(screen) != me->m_DisplayId) {
            // CVDisplayLinkSetCurrentCGDisplay() will deadlock if called
            // within a CVDisplayLink callback, so we just teardown the
            // decoder (and pacer) to let it come back up on the new display.
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Streaming window display has changed; resetting decoder");
            SDL_Event event;
            event.type = SDL_RENDER_TARGETS_RESET;
            SDL_PushEvent(&event);
            return kCVReturnSuccess;
        }

        // In my testing on macOS 10.13, this callback is invoked about 24 ms
        // prior to the specified v-sync time (now - vsyncTime). Since this is
        // greater than the standard v-sync interval (16 ms = 60 FPS), we will
        // draw using the current host time, rather than the actual v-sync target
        // time. Because the CVDisplayLink is in sync with the actual v-sync
        // interval, even if many ms prior, we can safely use the current host time
        // and get a consistent callback for each v-sync. This reduces video latency
        // by at least 1 frame vs. rendering with the actual vsyncTime.
        me->m_Pacer->vsyncCallback(500 / me->m_DisplayFps);

        return kCVReturnSuccess;
    }

    virtual bool initialize(SDL_Window* window, int displayFps) override
    {
        SDL_SysWMinfo info;

        SDL_VERSION(&info.version);

        if (!SDL_GetWindowWMInfo(window, &info)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_GetWindowWMInfo() failed: %s",
                         SDL_GetError());
            return false;
        }

        SDL_assert(info.subsystem == SDL_SYSWM_COCOA);

        m_Window = info.info.cocoa.window;
        m_DisplayFps = displayFps;

        NSScreen* screen = [m_Window screen];
        CVReturn status;
        if (screen == nullptr) {
            // Window not visible on any display, so use a
            // CVDisplayLink that can work with all active displays.
            // When we become visible, we'll recreate ourselves
            // and associate with the new screen.
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "NSWindow is not visible on any display");
            status = CVDisplayLinkCreateWithActiveCGDisplays(&m_DisplayLink);
        }
        else {
            m_DisplayId = getDisplayID(screen);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "NSWindow on display: %x",
                        m_DisplayId);
            status = CVDisplayLinkCreateWithCGDisplay(m_DisplayId, &m_DisplayLink);
        }
        if (status != kCVReturnSuccess) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create CVDisplayLink: %d",
                         status);
            return false;
        }

        status = CVDisplayLinkSetOutputCallback(m_DisplayLink, displayLinkOutputCallback, this);
        if (status != kCVReturnSuccess) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "CVDisplayLinkSetOutputCallback() failed: %d",
                         status);
            return false;
        }

        status = CVDisplayLinkStart(m_DisplayLink);
        if (status != kCVReturnSuccess) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "CVDisplayLinkStart() failed: %d",
                         status);
            return false;
        }

        return true;
    }

private:
    Pacer* m_Pacer;
    NSWindow* m_Window;
    CVDisplayLinkRef m_DisplayLink;
    int m_DisplayFps;
    CGDirectDisplayID m_DisplayId;
};

IVsyncSource* DisplayLinkVsyncSourceFactory::createVsyncSource(Pacer* pacer) {
    return new DisplayLinkVsyncSource(pacer);
}
