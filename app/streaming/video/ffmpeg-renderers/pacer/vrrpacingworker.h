#pragma once

#include "../../decoder.h"
#include "../ivrrframepresenter.h"
#include "vrr/vrrtargetwaiter.h"
#include "vrr/vrrtypes.h"

#include <atomic>
#include <deque>
#include <memory>

#include <QMutex>
#include <QWaitCondition>

class VrrTimingController;
struct VrrTimingDecision;

// The complete VRR execution path lives in this one worker.  It owns a bounded
// queue and the renderer context from preparation through presentation;
// fixed-VSync and unpaced Pacer behavior never enter it.
class VrrPacingWorker {
public:
    VrrPacingWorker(IVrrFramePresenter* presenter,
                    const VrrSessionConfig& config,
                    PVIDEO_STATS videoStats);
    ~VrrPacingWorker();

    VrrPacingWorker(const VrrPacingWorker&) = delete;
    VrrPacingWorker& operator=(const VrrPacingWorker&) = delete;

    bool start();

    void submit(PacedFrame&& frame);

    // Calls from the UI/main thread only manipulate worker-owned state. All
    // backend notifications are delivered by the worker itself.
    void notifyWindowChanged(PWINDOW_STATE_CHANGE_INFO info);

private:
    static int threadProc(void* context);

    int run();
    bool dequeueFrame(PacedFrame& frame);
    bool hasQueuedFrame();
    void discardQueuedFrames(bool countDrops);
    uint32_t consumeWindowStateNotifications();
    bool presentationSuspended() const;
    bool isStopping() const;
    void waitForSubmissionFloor(const VrrTimingDecision& decision);
    void recordSubmission(const VrrPresentFeedback& feedback,
                          uint64_t operationStartUs,
                          uint64_t operationEndUs);

    IVrrFramePresenter* m_Presenter;
    PVIDEO_STATS m_VideoStats;

    std::unique_ptr<VrrTimingController> m_TimingController;
    VrrTargetWaiter m_TargetWaiter;

    QMutex m_FrameQueueLock;
    QWaitCondition m_FrameQueueNotEmpty;
    std::deque<PacedFrame> m_FrameQueue;
    // Keeps the most recently presented frame alive until the next result so a
    // decoder-owned surface cannot be recycled while the GPU still reads it.
    PacedFrame m_DeferredFrame;
    SDL_Thread* m_WorkerThread = nullptr;
    std::atomic_bool m_Stopping { false };
    std::atomic_bool m_Suspended { false };
    std::atomic_uint32_t m_PendingWindowStateFlags { 0 };
    bool m_PresenterSuspended = false;
    bool m_RebaseOnNextFrame = false;
};
