#include "pacer.h"

#include "nullthreadedvsyncsource.h"

#ifdef Q_OS_DARWIN
#include "displaylinkvsyncsource.h"
#endif

#ifdef Q_OS_WIN32
#include "dxvsyncsource.h"
#endif

#define FRAME_HISTORY_ENTRIES 8

// We may be woken up slightly late so don't go all the way
// up to the next V-sync since we may accidentally step into
// the next V-sync period. It also takes some amount of time
// to do the render itself, so we can't render right before
// V-sync happens.
#define TIMER_SLACK_MS 3

Pacer::Pacer(IFFmpegRenderer* renderer) :
    m_FrameQueueLock(0),
    m_VsyncSource(nullptr),
    m_VsyncRenderer(renderer),
    m_MaxVideoFps(0),
    m_DisplayFps(0)
{

}

Pacer::~Pacer()
{
    // Stop V-sync callbacks
    delete m_VsyncSource;
    m_VsyncSource = nullptr;

    while (!m_FrameQueue.isEmpty()) {
        AVFrame* frame = m_FrameQueue.dequeue();
        av_frame_free(&frame);
    }
}

// Called in an arbitrary thread by the IVsyncSource on V-sync
// or an event synchronized with V-sync
void Pacer::vsyncCallback(int timeUntilNextVsyncMillis)
{
    // Make sure initialize() has been called
    SDL_assert(m_MaxVideoFps != 0);

    SDL_assert(timeUntilNextVsyncMillis >= TIMER_SLACK_MS);

    Uint32 vsyncCallbackStartTime = SDL_GetTicks();

    SDL_AtomicLock(&m_FrameQueueLock);

    // If the queue length history entries are large, be strict
    // about dropping excess frames.
    int frameDropTarget = 1;

    // If we may get more frames per second than we can display, use
    // frame history to drop frames only if consistently above the
    // one queued frame mark.
    if (m_MaxVideoFps >= m_DisplayFps) {
        for (int i = 0; i < m_FrameQueueHistory.count(); i++) {
            if (m_FrameQueueHistory[i] <= 1) {
                // Be lenient as long as the queue length
                // resolves before the end of frame history
                frameDropTarget = 3;
                break;
            }
        }

        if (m_FrameQueueHistory.count() == FRAME_HISTORY_ENTRIES) {
            m_FrameQueueHistory.dequeue();
        }

        m_FrameQueueHistory.enqueue(m_FrameQueue.count());
    }

    // Catch up if we're several frames ahead
    while (m_FrameQueue.count() > frameDropTarget) {
        AVFrame* frame = m_FrameQueue.dequeue();
        av_frame_free(&frame);
    }


    if (m_FrameQueue.isEmpty()) {
        SDL_AtomicUnlock(&m_FrameQueueLock);

        while (!SDL_TICKS_PASSED(SDL_GetTicks(),
                                 vsyncCallbackStartTime + timeUntilNextVsyncMillis - TIMER_SLACK_MS)) {
            SDL_Delay(1);

            SDL_AtomicLock(&m_FrameQueueLock);
            if (!m_FrameQueue.isEmpty()) {
                // Don't release the lock
                goto RenderNextFrame;
            }
            SDL_AtomicUnlock(&m_FrameQueueLock);
        }

        // Nothing to render at this time
        return;
    }

RenderNextFrame:
    // Grab the first frame
    AVFrame* frame = m_FrameQueue.dequeue();
    SDL_AtomicUnlock(&m_FrameQueueLock);

    // Render it
    m_VsyncRenderer->renderFrameAtVsync(frame);

    // Free the frame
    av_frame_free(&frame);
}

bool Pacer::initialize(SDL_Window* window, int maxVideoFps, bool enableVsync)
{
    m_MaxVideoFps = maxVideoFps;
    m_EnableVsync = enableVsync;

    int displayIndex = SDL_GetWindowDisplayIndex(window);
    if (displayIndex < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to get current display: %s",
                     SDL_GetError());

        // Assume display 0 if it fails
        displayIndex = 0;
    }

    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(displayIndex, &mode) == 0) {
        // May be zero if undefined
        m_DisplayFps = mode.refresh_rate;
    }

    if (m_EnableVsync) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Frame pacing in tear-free mode: target %d Hz with %d FPS stream",
                    m_DisplayFps, m_MaxVideoFps);

    #if defined(Q_OS_DARWIN)
        m_VsyncSource = DisplayLinkVsyncSourceFactory::createVsyncSource(this);
    #elif defined(Q_OS_WIN32)
        m_VsyncSource = new DxVsyncSource(this);
    #else
        // Platforms without a VsyncSource will just render frames
        // immediately like they used to.
    #endif

        if (m_VsyncSource != nullptr && !m_VsyncSource->initialize(window, m_DisplayFps)) {
            return false;
        }
    }
    else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Minimal latency tearing mode: target %d Hz with %d FPS stream",
                    m_DisplayFps, m_MaxVideoFps);
    }

    return true;
}

void Pacer::submitFrame(AVFrame* frame)
{
    // Make sure initialize() has been called
    SDL_assert(m_MaxVideoFps != 0);

    // Queue the frame until the V-sync callback if
    // we have a V-sync source, otherwise deliver it
    // immediately and hope for the best.
    if (isUsingFrameQueue()) {
        SDL_AtomicLock(&m_FrameQueueLock);
        m_FrameQueue.enqueue(frame);
        SDL_AtomicUnlock(&m_FrameQueueLock);
    }
    else {
        m_VsyncRenderer->renderFrameAtVsync(frame);
        av_frame_free(&frame);
    }
}

bool Pacer::isUsingFrameQueue()
{
    return m_VsyncSource != nullptr;
}
