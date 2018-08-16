#include "displaylinkvsyncsource.h"

DisplayLinkVsyncSource::DisplayLinkVsyncSource(Pacer* pacer)
    : m_Pacer(pacer),
      m_DisplayLink(nullptr)
{

}

DisplayLinkVsyncSource::~DisplayLinkVsyncSource()
{
    if (m_DisplayLink != nullptr) {
        CVDisplayLinkStop(m_DisplayLink);
        CVDisplayLinkRelease(m_DisplayLink);
    }
}

CVReturn
DisplayLinkVsyncSource::displayLinkOutputCallback(
    CVDisplayLinkRef,
    const CVTimeStamp* /* now */,
    const CVTimeStamp* /* vsyncTime */,
    CVOptionFlags,
    CVOptionFlags*,
    void *displayLinkContext)
{
    auto me = reinterpret_cast<DisplayLinkVsyncSource*>(displayLinkContext);

    // In my testing on macOS 10.13, this callback is invoked about 24 ms
    // prior to the specified v-sync time (now - vsyncTime). Since this is
    // greater than the standard v-sync interval (16 ms = 60 FPS), we will
    // draw using the current host time, rather than the actual v-sync target
    // time. Because the CVDisplayLink is in sync with the actual v-sync
    // interval, even if many ms prior, we can safely use the current host time
    // and get a consistent callback for each v-sync. This reduces video latency
    // by at least 1 frame vs. rendering with the actual vsyncTime.
    me->m_Pacer->vsyncCallback();

    return kCVReturnSuccess;
}

bool DisplayLinkVsyncSource::initialize(SDL_Window*)
{
    CVDisplayLinkCreateWithActiveCGDisplays(&m_DisplayLink);
    CVDisplayLinkSetOutputCallback(m_DisplayLink, displayLinkOutputCallback, this);
    CVDisplayLinkStart(m_DisplayLink);
    return true;
}
