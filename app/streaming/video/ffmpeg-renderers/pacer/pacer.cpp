#include "pacer.h"

#ifdef Q_OS_DARWIN
#include "displaylinkvsyncsource.h"
#endif

#define FRAME_HISTORY_ENTRIES 8

Pacer::Pacer(IVsyncRenderer* renderer) :
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

    drain();
}

// Called in an arbitrary thread by the IVsyncSource on V-sync
// or an event synchronized with V-sync
void Pacer::vsyncCallback()
{
    // Make sure initialize() has been called
    SDL_assert(m_MaxVideoFps != 0);

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

        // Nothing to render at this time
        return;
    }

    // Grab the first frame
    AVFrame* frame = m_FrameQueue.dequeue();
    SDL_AtomicUnlock(&m_FrameQueueLock);

    // Render it
    m_VsyncRenderer->renderFrameAtVsync(frame);

    // Free the frame
    av_frame_free(&frame);
}

bool Pacer::initialize(SDL_Window* window, int maxVideoFps)
{
    m_MaxVideoFps = maxVideoFps;

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

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Frame pacing: target %d Hz with %d FPS stream",
                m_DisplayFps, m_MaxVideoFps);

#ifdef Q_OS_DARWIN
    m_VsyncSource = new DisplayLinkVsyncSource(this);
#else
    SDL_assert(false);
#endif

    return m_VsyncSource->initialize(window);
}

void Pacer::submitFrame(AVFrame* frame)
{
    // Make sure initialize() has been called
    SDL_assert(m_MaxVideoFps != 0);

    SDL_AtomicLock(&m_FrameQueueLock);
    m_FrameQueue.enqueue(frame);
    SDL_AtomicUnlock(&m_FrameQueueLock);
}

void Pacer::drain()
{
    while (!m_FrameQueue.isEmpty()) {
        AVFrame* frame = m_FrameQueue.dequeue();
        av_frame_free(&frame);
    }
}
