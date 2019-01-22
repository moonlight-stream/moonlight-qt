#include "pacer.h"
#include "streaming/streamutils.h"

#include "nullthreadedvsyncsource.h"

#ifdef Q_OS_DARWIN
#include "displaylinkvsyncsource.h"
#endif

#ifdef Q_OS_WIN32
#include "dxvsyncsource.h"
#endif

#define FRAME_HISTORY_ENTRIES 8
#define RENDER_TIME_HISTORY_ENTRIES 8

// We may be woken up slightly late so don't go all the way
// up to the next V-sync since we may accidentally step into
// the next V-sync period. It also takes some amount of time
// to do the render itself, so we can't render right before
// V-sync happens.
#define TIMER_SLACK_MS 3

Pacer::Pacer(IFFmpegRenderer* renderer, PVIDEO_STATS videoStats) :
    m_FrameQueueLock(0),
    m_VsyncSource(nullptr),
    m_VsyncRenderer(renderer),
    m_MaxVideoFps(0),
    m_DisplayFps(0),
    m_VideoStats(videoStats)
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
        for (int queueHistoryEntry : m_FrameQueueHistory) {
            if (queueHistoryEntry <= 1) {
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
        m_VideoStats->pacerDroppedFrames++;
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

        // Nothing to render at this time. This counts as a drop.
        addRenderTimeToHistory(0);
        return;
    }

RenderNextFrame:
    // Grab the first frame
    AVFrame* frame = m_FrameQueue.dequeue();
    SDL_AtomicUnlock(&m_FrameQueueLock);

    renderFrame(frame);
}

bool Pacer::initialize(SDL_Window* window, int maxVideoFps, bool enablePacing)
{
    m_MaxVideoFps = maxVideoFps;
    m_DisplayFps = StreamUtils::getDisplayRefreshRate(window);

    if (enablePacing) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Frame pacing active: target %d Hz with %d FPS stream",
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
                    "Frame pacing disabled: target %d Hz with %d FPS stream",
                    m_DisplayFps, m_MaxVideoFps);
    }

    return true;
}

void Pacer::addRenderTimeToHistory(int renderTime)
{
    if (m_RenderTimeHistory.count() == RENDER_TIME_HISTORY_ENTRIES) {
        m_RenderTimeHistory.dequeue();
    }

    m_RenderTimeHistory.enqueue(renderTime);
}

void Pacer::renderFrame(AVFrame* frame)
{
    bool dropFrame = !m_RenderTimeHistory.isEmpty();

    // If rendering consistently takes longer than an entire V-sync interval,
    // there must have been frames pending in the pipeline that are blocking us.
    // Drop our this frame to clear the backed up display pipeline.
    for (int renderTime : m_RenderTimeHistory) {
        if (renderTime <= 1000 / m_DisplayFps) {
            dropFrame = false;
            break;
        }
    }

    // Drop this frame if we believe we've run into V-sync waits
    if (dropFrame) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Render time (%d ms) consistently exceeded V-sync period; dropping frame to reduce latency",
                    m_RenderTimeHistory[0]);

        // Add 0 render time to history to account for the drop
        addRenderTimeToHistory(0);

        m_VideoStats->pacerDroppedFrames++;
        av_frame_free(&frame);
        return;
    }

    // Count time spent in Pacer's queues
    Uint32 beforeRender = SDL_GetTicks();
    m_VideoStats->totalPacerTime += beforeRender - frame->pts;

    // Render it
    m_VsyncRenderer->renderFrameAtVsync(frame);
    Uint32 afterRender = SDL_GetTicks();

    addRenderTimeToHistory(afterRender - beforeRender);

    m_VideoStats->totalRenderTime += afterRender - beforeRender;
    m_VideoStats->renderedFrames++;
    av_frame_free(&frame);
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
        renderFrame(frame);
    }
}

bool Pacer::isUsingFrameQueue()
{
    return m_VsyncSource != nullptr;
}
