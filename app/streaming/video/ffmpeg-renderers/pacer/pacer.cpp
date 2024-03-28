#include "pacer.h"
#include "streaming/streamutils.h"

#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <VersionHelpers.h>
#include "dxvsyncsource.h"
#endif

#ifdef HAS_WAYLAND
#include "waylandvsyncsource.h"
#endif

#include <SDL_syswm.h>

// Limit the number of queued frames to prevent excessive memory consumption
// if the V-Sync source or renderer is blocked for a while. It's important
// that the sum of all queued frames between both pacing and rendering queues
// must not exceed the number buffer pool size to avoid running the decoder
// out of available decoding surfaces.
#define MAX_QUEUED_FRAMES 4

// We may be woken up slightly late so don't go all the way
// up to the next V-sync since we may accidentally step into
// the next V-sync period. It also takes some amount of time
// to do the render itself, so we can't render right before
// V-sync happens.
#define TIMER_SLACK_MS 3

Pacer::Pacer(IFFmpegRenderer* renderer, PVIDEO_STATS videoStats) :
    m_RenderThread(nullptr),
    m_VsyncThread(nullptr),
    m_LatencyThread(nullptr),
    m_Stopping(false),
    m_VsyncSource(nullptr),
    m_VsyncRenderer(renderer),
    m_MaxVideoFps(0),
    m_DisplayFps(0),
    m_MinimumLatency(0),
    m_MaxLatencyQueuedFrames(0),
    m_VideoStats(videoStats)
{

}

Pacer::~Pacer()
{
    m_Stopping = true;

    // Stop the latency thread
    if (m_LatencyThread != nullptr) {
        m_LatencyQueueNotEmpty.wakeAll();
        SDL_WaitThread(m_LatencyThread, nullptr);
    }

    // Stop the V-sync thread
    if (m_VsyncThread != nullptr) {
        m_PacingQueueNotEmpty.wakeAll();
        m_VsyncSignalled.wakeAll();
        SDL_WaitThread(m_VsyncThread, nullptr);
    }

    // Stop V-sync callbacks
    delete m_VsyncSource;
    m_VsyncSource = nullptr;

    // Stop the render thread
    if (m_RenderThread != nullptr) {
        m_RenderQueueNotEmpty.wakeAll();
        SDL_WaitThread(m_RenderThread, nullptr);
    }
    else {
        // Notify the renderer that it is being destroyed soon
        // NB: This must happen on the same thread that calls renderFrame().
        m_VsyncRenderer->cleanupRenderContext();
    }

    // Delete any remaining unconsumed frames
    while (!m_RenderQueue.isEmpty()) {
        AVFrame* frame = m_RenderQueue.dequeue();
        av_frame_free(&frame);
    }
    while (!m_PacingQueue.isEmpty()) {
        AVFrame* frame = m_PacingQueue.dequeue();
        av_frame_free(&frame);
    }
}

void Pacer::renderOnMainThread()
{
    // Ignore this call for renderers that work on a dedicated render thread
    if (m_RenderThread != nullptr) {
        return;
    }

    m_FrameQueueLock.lock();

    if (!m_RenderQueue.isEmpty()) {
        AVFrame* frame = m_RenderQueue.dequeue();
        m_FrameQueueLock.unlock();

        renderFrame(frame);
    }
    else {
        m_FrameQueueLock.unlock();
    }
}

int Pacer::vsyncThread(void *context)
{
    Pacer* me = reinterpret_cast<Pacer*>(context);

#if SDL_VERSION_ATLEAST(2, 0, 9)
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
#else
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
#endif

    bool async = me->m_VsyncSource->isAsync();
    while (!me->m_Stopping) {
        if (async) {
            // Wait for the VSync source to invoke signalVsync() or 100ms to elapse
            me->m_FrameQueueLock.lock();
            me->m_VsyncSignalled.wait(&me->m_FrameQueueLock, 100);
            me->m_FrameQueueLock.unlock();
        }
        else {
            // Let the VSync source wait in the context of our thread
            me->m_VsyncSource->waitForVsync();
        }

        if (me->m_Stopping) {
            break;
        }

        me->handleVsync(1000 / me->m_DisplayFps);
    }

    return 0;
}

int Pacer::renderThread(void* context)
{
    Pacer* me = reinterpret_cast<Pacer*>(context);

    if (SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH) < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Unable to set render thread to high priority: %s",
                    SDL_GetError());
    }

    while (!me->m_Stopping) {
        // Wait for the renderer to be ready for the next frame
        me->m_VsyncRenderer->waitToRender();

        // Acquire the frame queue lock to protect the queue and
        // the not empty condition
        me->m_FrameQueueLock.lock();

        // Wait for a frame to be ready to render
        while (!me->m_Stopping && me->m_RenderQueue.isEmpty()) {
            me->m_RenderQueueNotEmpty.wait(&me->m_FrameQueueLock);
        }

        if (me->m_Stopping) {
            // Exit this thread
            me->m_FrameQueueLock.unlock();
            break;
        }

        AVFrame* frame = me->m_RenderQueue.dequeue();
        me->m_FrameQueueLock.unlock();

        me->renderFrame(frame);
    }

    // Notify the renderer that it is being destroyed soon
    // NB: This must happen on the same thread that calls renderFrame().
    me->m_VsyncRenderer->cleanupRenderContext();

    return 0;
}


int Pacer::latencyThread(void* context)
{
    Pacer* me = reinterpret_cast<Pacer*>(context);

#if SDL_VERSION_ATLEAST(2, 0, 9)
    int threadPriorityResult = SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
#else
    int threadPriorityResult = SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
#endif
    if (threadPriorityResult < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Unable to increase latency thread to higher priority: %s",
                    SDL_GetError());
    }

    // Make sure initialize() has been called
    SDL_assert(m_MaxVideoFps != 0);

    Uint32 startTime = SDL_GetTicks();
    double releasePeriod = 1000.0 / me->m_MaxVideoFps;
    double scheduleShift = 0;

    int baseFrameBufferAllowance = 1 + me->m_MinimumLatency * me->m_MaxVideoFps / 1000;

    int iteration = 0;
    while (!me->m_Stopping) {
        iteration++;

        // Wait for the next frame release target
        double nextReleaseTime = startTime + scheduleShift + releasePeriod * iteration;
        double timeToNextRelease = nextReleaseTime - SDL_GetTicks();
        if (timeToNextRelease < -releasePeriod * 0.5) {
            // We somehow fell far behind our release target (maybe due to thread scheduling or
            // a super late frame). Just skip to the next iteration.
            continue;
        }
        if (timeToNextRelease >= 1.0) {
            SDL_Delay((Uint32) timeToNextRelease);
        }

        if (me->m_Stopping) {
            break;
        }

        // Acquire the latency queue lock to protect the queue and
        // the not empty condition
        me->m_LatencyQueueLock.lock();

        // If the queue length history entries are large, be strict
        // about dropping excess frames.
        int frameDropTarget = baseFrameBufferAllowance;

        // If we may get more frames per second than we can display, use
        // frame history to drop frames only if consistently above the
        // our frame buffer allowance.
        for (int queueHistoryEntry : me->m_LatencyQueueHistory) {
            if (queueHistoryEntry <= baseFrameBufferAllowance) {
                // Be lenient as long as the queue length
                // resolves before the end of frame history
                frameDropTarget = baseFrameBufferAllowance + 2;
                break;
            }
        }

        // Keep a rolling 500 ms window of latency queue history
        if (me->m_LatencyQueueHistory.count() == me->m_DisplayFps / 2) {
            me->m_LatencyQueueHistory.dequeue();
        }

        me->m_LatencyQueueHistory.enqueue(me->m_LatencyQueue.count());

        // Drop excess frames if we're above our drop target
        while (me->m_LatencyQueue.count() > frameDropTarget) {
            AVFrame* frame = me->m_LatencyQueue.dequeue();

            // Drop the lock while we call av_frame_free()
            me->m_LatencyQueueLock.unlock();
            me->m_VideoStats->pacerDroppedFrames++;
            av_frame_free(&frame);
            me->m_LatencyQueueLock.lock();
        }

        // Wait for a frame. Ideally a frame is already ready, but that may not be the case
        while (!me->m_Stopping && me->m_LatencyQueue.empty()) {
            me->m_LatencyQueueNotEmpty.wait(&me->m_LatencyQueueLock);
        }

        if (me->m_Stopping) {
            me->m_LatencyQueueLock.unlock();
            break;
        }

        AVFrame* frame = me->m_LatencyQueue.dequeue();
        me->m_LatencyQueueLock.unlock();

        // Adjust our frame release schedule to target our minimum latency.
        int frameLatency = (int) (SDL_GetTicks() - frame->pkt_dts);
        scheduleShift += (me->m_MinimumLatency - frameLatency) * 0.1;

        // Enqueue the frame in the next queue
        me->m_FrameQueueLock.lock();
        if (me->m_VsyncSource == nullptr) {
            me->enqueueFrameForRenderingAndUnlock(frame);
        }
        else {
            me->dropFrameForEnqueue(me->m_PacingQueue, MAX_QUEUED_FRAMES);
            me->m_PacingQueue.enqueue(frame);
            me->m_FrameQueueLock.unlock();
            me->m_PacingQueueNotEmpty.wakeOne();
        }
    }

    return 0;
}

void Pacer::enqueueFrameForRenderingAndUnlock(AVFrame *frame)
{
    dropFrameForEnqueue(m_RenderQueue, MAX_QUEUED_FRAMES);
    m_RenderQueue.enqueue(frame);

    m_FrameQueueLock.unlock();

    if (m_RenderThread != nullptr) {
        m_RenderQueueNotEmpty.wakeOne();
    }
    else {
        SDL_Event event;

        // For main thread rendering, we'll push an event to trigger a callback
        event.type = SDL_USEREVENT;
        event.user.code = SDL_CODE_FRAME_READY;
        SDL_PushEvent(&event);
    }
}

// Called in an arbitrary thread by the IVsyncSource on V-sync
// or an event synchronized with V-sync
void Pacer::handleVsync(int timeUntilNextVsyncMillis)
{
    // Make sure initialize() has been called
    SDL_assert(m_MaxVideoFps != 0);

    m_FrameQueueLock.lock();

    // If the queue length history entries are large, be strict
    // about dropping excess frames.
    int frameDropTarget = 1;

    // If we may get more frames per second than we can display, use
    // frame history to drop frames only if consistently above the
    // one queued frame mark.
    if (m_MaxVideoFps >= m_DisplayFps) {
        for (int queueHistoryEntry : m_PacingQueueHistory) {
            if (queueHistoryEntry <= 1) {
                // Be lenient as long as the queue length
                // resolves before the end of frame history
                frameDropTarget = 3;
                break;
            }
        }

        // Keep a rolling 500 ms window of pacing queue history
        if (m_PacingQueueHistory.count() == m_DisplayFps / 2) {
            m_PacingQueueHistory.dequeue();
        }

        m_PacingQueueHistory.enqueue(m_PacingQueue.count());
    }

    // Catch up if we're several frames ahead
    while (m_PacingQueue.count() > frameDropTarget) {
        AVFrame* frame = m_PacingQueue.dequeue();

        // Drop the lock while we call av_frame_free()
        m_FrameQueueLock.unlock();
        m_VideoStats->pacerDroppedFrames++;
        av_frame_free(&frame);
        m_FrameQueueLock.lock();
    }

    if (m_PacingQueue.isEmpty()) {
        // Wait for a frame to arrive or our V-sync timeout to expire
        if (!m_PacingQueueNotEmpty.wait(&m_FrameQueueLock, SDL_max(timeUntilNextVsyncMillis, TIMER_SLACK_MS) - TIMER_SLACK_MS)) {
            // Wait timed out - unlock and bail
            m_FrameQueueLock.unlock();
            return;
        }

        if (m_Stopping) {
            m_FrameQueueLock.unlock();
            return;
        }
    }

    // Place the first frame on the render queue
    enqueueFrameForRenderingAndUnlock(m_PacingQueue.dequeue());
}

bool Pacer::initialize(SDL_Window* window, int maxVideoFps, bool enablePacing, int minimumLatency)
{
    m_MaxVideoFps = maxVideoFps;
    m_DisplayFps = StreamUtils::getDisplayRefreshRate(window);
    m_RendererAttributes = m_VsyncRenderer->getRendererAttributes();
    m_MinimumLatency = minimumLatency;

    if (enablePacing) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Frame pacing: target %d Hz with %d FPS stream",
                    m_DisplayFps, m_MaxVideoFps);

        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (!SDL_GetWindowWMInfo(window, &info)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_GetWindowWMInfo() failed: %s",
                         SDL_GetError());
            return false;
        }

        switch (info.subsystem) {
    #ifdef Q_OS_WIN32
        case SDL_SYSWM_WINDOWS:
            // Don't use D3DKMTWaitForVerticalBlankEvent() on Windows 7, because
            // it blocks during other concurrent DX operations (like actually rendering).
            if (IsWindows8OrGreater()) {
                m_VsyncSource = new DxVsyncSource(this);
            }
            break;
    #endif

    #if defined(SDL_VIDEO_DRIVER_WAYLAND) && defined(HAS_WAYLAND)
        case SDL_SYSWM_WAYLAND:
            m_VsyncSource = new WaylandVsyncSource(this);
            break;
    #endif

        default:
            // Platforms without a VsyncSource will just render frames
            // immediately like they used to.
            break;
        }

        SDL_assert(m_VsyncSource != nullptr || !(m_RendererAttributes & RENDERER_ATTRIBUTE_FORCE_PACING));

        if (m_VsyncSource != nullptr && !m_VsyncSource->initialize(window, m_DisplayFps)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Vsync source failed to initialize. Frame pacing will not be available!");
            delete m_VsyncSource;
            m_VsyncSource = nullptr;
        }
    }
    else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Frame pacing disabled: target %d Hz with %d FPS stream",
                    m_DisplayFps, m_MaxVideoFps);
    }

    if (m_MinimumLatency > 0) {
        m_MaxLatencyQueuedFrames = 3 + m_MinimumLatency * m_MaxVideoFps / 1000;
        m_LatencyThread = SDL_CreateThread(Pacer::latencyThread, "PacerLatency", this);
    }

    if (m_VsyncSource != nullptr) {
        m_VsyncThread = SDL_CreateThread(Pacer::vsyncThread, "PacerVsync", this);
    }

    if (m_VsyncRenderer->isRenderThreadSupported()) {
        m_RenderThread = SDL_CreateThread(Pacer::renderThread, "PacerRender", this);
    }

    return true;
}

void Pacer::signalVsync()
{
    m_VsyncSignalled.wakeOne();
}

void Pacer::renderFrame(AVFrame* frame)
{
    // Count time spent in Pacer's queues
    Uint32 beforeRender = SDL_GetTicks();
    m_VideoStats->totalPacerTime += beforeRender - frame->pkt_dts;

    // Render it
    m_VsyncRenderer->renderFrame(frame);
    Uint32 afterRender = SDL_GetTicks();

    m_VideoStats->totalRenderTime += afterRender - beforeRender;
    m_VideoStats->renderedFrames++;
    av_frame_free(&frame);

    // Drop frames if we have too many queued up for a while
    m_FrameQueueLock.lock();

    int frameDropTarget;

    if (m_RendererAttributes & RENDERER_ATTRIBUTE_NO_BUFFERING) {
        // Renderers that don't buffer any frames but don't support waitToRender() need us to buffer
        // an extra frame to ensure they don't starve while waiting to present.
        frameDropTarget = 1;
    }
    else {
        frameDropTarget = 0;
        for (int queueHistoryEntry : m_RenderQueueHistory) {
            if (queueHistoryEntry == 0) {
                // Be lenient as long as the queue length
                // resolves before the end of frame history
                frameDropTarget = 2;
                break;
            }
        }

        // Keep a rolling 500 ms window of render queue history
        if (m_RenderQueueHistory.count() == m_MaxVideoFps / 2) {
            m_RenderQueueHistory.dequeue();
        }

        m_RenderQueueHistory.enqueue(m_RenderQueue.count());
    }

    // Catch up if we're several frames ahead
    while (m_RenderQueue.count() > frameDropTarget) {
        AVFrame* frame = m_RenderQueue.dequeue();

        // Drop the lock while we call av_frame_free()
        m_FrameQueueLock.unlock();
        m_VideoStats->pacerDroppedFrames++;
        av_frame_free(&frame);
        m_FrameQueueLock.lock();
    }

    m_FrameQueueLock.unlock();
}

void Pacer::dropFrameForEnqueue(QQueue<AVFrame*>& queue, int maxQueuedFrames)
{
    SDL_assert(queue.size() <= maxQueuedFrames);
    if (queue.size() == maxQueuedFrames) {
        AVFrame* frame = queue.dequeue();
        av_frame_free(&frame);
    }
}

void Pacer::submitFrame(AVFrame* frame)
{
    // Make sure initialize() has been called
    SDL_assert(m_MaxVideoFps != 0);

    // Queue the frame and possibly wake up the render thread
    if (m_LatencyThread != nullptr) {
        m_LatencyQueueLock.lock();
        dropFrameForEnqueue(m_LatencyQueue, m_MaxLatencyQueuedFrames);
        m_LatencyQueue.enqueue(frame);
        m_LatencyQueueLock.unlock();
        m_LatencyQueueNotEmpty.wakeOne();
    }
    else {
        m_FrameQueueLock.lock();
        if (m_VsyncSource != nullptr) {
            dropFrameForEnqueue(m_PacingQueue, MAX_QUEUED_FRAMES);
            m_PacingQueue.enqueue(frame);
            m_FrameQueueLock.unlock();
            m_PacingQueueNotEmpty.wakeOne();
        }
        else {
            enqueueFrameForRenderingAndUnlock(frame);
        }
    }
}
