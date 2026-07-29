#include "vrrpacingworker.h"

#include "vrr/vrrtimingcontroller.h"

#include <Limelight.h>

#include <algorithm>
#include <utility>

namespace {

// Keep enough decoded successors to absorb the short gap-then-burst delivery
// pattern seen near the panel ceiling. Capacity remains bounded and evicts the
// oldest queued successor under sustained pressure, so it cannot accumulate
// an unbounded latency backlog.
constexpr size_t kMaximumQueuedFrames = 3;

constexpr uint32_t kVrrWindowStateMask =
    WINDOW_STATE_CHANGE_SIZE |
    WINDOW_STATE_CHANGE_DISPLAY |
    WINDOW_STATE_CHANGE_MINIMIZED |
    WINDOW_STATE_CHANGE_RESTORED;
constexpr uint32_t kVrrDisplayEpochStateMask =
    WINDOW_STATE_CHANGE_SIZE |
    WINDOW_STATE_CHANGE_DISPLAY;

uint64_t submissionBoundaryUs(const VrrPresentFeedback& feedback,
                              uint64_t operationStartUs,
                              uint64_t operationEndUs)
{
    if (!feedback.presented) {
        return 0;
    }

    // A backend may wait for physical scanout inside presentAdaptive(). Use
    // its exact native-call timestamp only when it belongs to this operation;
    // stale or cross-epoch feedback must not poison every future spacing floor.
    if (feedback.submissionTimeValid &&
            operationStartUs <= operationEndUs &&
            feedback.submissionTimeUs >= operationStartUs &&
            feedback.submissionTimeUs <= operationEndUs) {
        return feedback.submissionTimeUs;
    }

    // Presenters without native timing are required to be thin. Anchoring at
    // entry prevents an unrelated blocking return from adding a display period
    // to every subsequent frame.
    return operationStartUs;
}

} // namespace

VrrPacingWorker::VrrPacingWorker(IVrrFramePresenter* presenter,
                                 const VrrSessionConfig& config,
                                 PVIDEO_STATS videoStats) :
    m_Presenter(presenter),
    m_VideoStats(videoStats),
    m_TimingController(std::make_unique<VrrTimingController>(
        config, presenter != nullptr &&
                presenter->canLatchAdaptivePresent()))
{
}

VrrPacingWorker::~VrrPacingWorker()
{
    {
        QMutexLocker lock(&m_FrameQueueLock);
        m_Stopping.store(true);
        m_FrameQueueNotEmpty.wakeAll();
    }

    if (m_WorkerThread != nullptr) {
        SDL_WaitThread(m_WorkerThread, nullptr);
        m_WorkerThread = nullptr;
    }

    discardQueuedFrames(false);
}

bool VrrPacingWorker::start()
{
    if (m_Presenter == nullptr ||
        m_Presenter->checkSupport() != VrrFallbackReason::NoFallback) {
        return false;
    }

    m_WorkerThread = SDL_CreateThread(VrrPacingWorker::threadProc,
                                      "PacerVRR", this);
    if (m_WorkerThread == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create VRR pacing worker: %s", SDL_GetError());
        return false;
    }

    return true;
}

void VrrPacingWorker::submit(PacedFrame&& frame)
{
    if (!frame) {
        return;
    }

    PacedFrame dropped;
    bool queued = false;
    {
        QMutexLocker lock(&m_FrameQueueLock);
        // Close the race where suspension can begin after the optimistic
        // check above but before this producer acquires the queue lock.
        if (m_Stopping.load() || m_Suspended.load()) {
            dropped = std::move(frame);
        }
        else {
            if (m_FrameQueue.size() >= kMaximumQueuedFrames) {
                dropped = std::move(m_FrameQueue.front());
                m_FrameQueue.pop_front();
            }
            m_FrameQueue.emplace_back(std::move(frame));
            queued = true;
        }
    }

    // The evicted frame is released here rather than under the queue lock, so
    // returning a decoder surface cannot stall the time-critical worker.
    if (dropped) {
        m_VideoStats->pacerDroppedFrames++;
    }
    if (queued) {
        m_FrameQueueNotEmpty.wakeOne();
    }
}

void VrrPacingWorker::notifyWindowChanged(PWINDOW_STATE_CHANGE_INFO info)
{
    if (info == nullptr) {
        return;
    }

    const uint32_t flags = info->stateChangeFlags & kVrrWindowStateMask;
    if (flags == 0) {
        return;
    }

    if (flags & WINDOW_STATE_CHANGE_MINIMIZED) {
        m_Suspended.store(true);
        discardQueuedFrames(true);
    }
    if (flags & WINDOW_STATE_CHANGE_RESTORED) {
        m_Suspended.store(false);
    }

    m_PendingWindowStateFlags.fetch_or(flags);
    m_FrameQueueNotEmpty.wakeAll();
}

int VrrPacingWorker::threadProc(void* context)
{
    return static_cast<VrrPacingWorker*>(context)->run();
}

int VrrPacingWorker::run()
{
#if SDL_VERSION_ATLEAST(2, 0, 9)
    if (SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL) < 0) {
#else
    if (SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH) < 0) {
#endif
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Unable to set VRR pacing worker priority: %s",
                    SDL_GetError());
    }

    // Abandons the prepared image and counts the frame as dropped. The worker
    // owns the target and display-spacing policy, so the timing controller is
    // always told how the abandoned submission ended.
    auto cancelPresentation = [this]() {
        const uint64_t startUs = LiGetMicroseconds();
        VrrPresentFeedback feedback = m_Presenter->cancelFrame();
        const uint64_t endUs = LiGetMicroseconds();
        feedback.cancelled = true;
        recordSubmission(feedback, startUs, endUs);
        m_VideoStats->pacerDroppedFrames++;
    };

    while (!isStopping()) {
        consumeWindowStateNotifications();

        PacedFrame frame;
        if (!dequeueFrame(frame)) {
            break;
        }

        consumeWindowStateNotifications();
        if (isStopping()) {
            if (frame) {
                m_VideoStats->pacerDroppedFrames++;
            }
            continue;
        }
        if (presentationSuspended()) {
            if (frame) {
                m_VideoStats->pacerDroppedFrames++;
            }
            // dequeueFrame() wakes without a frame so suspension can be
            // delivered to the backend. Once delivered, block here until a
            // restore or shutdown notification changes the atomic state.
            QMutexLocker lock(&m_FrameQueueLock);
            while (!isStopping() && m_Suspended.load()) {
                m_FrameQueueNotEmpty.wait(&m_FrameQueueLock);
            }
            continue;
        }
        if (!frame) {
            // dequeueFrame() also wakes the worker without a frame so window
            // state can be delivered while suspended.
            continue;
        }

        if (m_RebaseOnNextFrame) {
            m_TimingController->rebase();
            m_RebaseOnNextFrame = false;
        }

        const VrrTimingDecision decision = m_TimingController->schedule(
            frame, LiGetMicroseconds());

        // schedule() deliberately clamps an overdue target to the current
        // one-slot deadline. That is right for the newest frame, but it makes
        // the later target-relative stale check unable to see time already
        // spent waiting in this worker's queue. If a fresher successor exists,
        // skip a frame that is already more than one source interval old before
        // rendering it; its RTP/frame delta remains in the controller, so the
        // successor preserves cadence without re-anchoring the whole model.
        const uint64_t scheduleNowUs = LiGetMicroseconds();
        const uint64_t scheduleAgeUs = scheduleNowUs >=
                frame.decodeCompleteUs() ?
            scheduleNowUs - frame.decodeCompleteUs() : 0;
        if (decision.sourcePeriodUs != 0 &&
            scheduleAgeUs > decision.sourcePeriodUs && hasQueuedFrame()) {
            m_VideoStats->pacerDroppedFrames++;
            m_TimingController->noteSubmission(false, false, 0);
            continue;
        }

        const VrrTargetWaitResult renderWait =
            m_TargetWaiter.waitUntil(decision.renderStartUs);
        // A deadline that was already in the past is not an OS wake delay.
        // This matters when a decoded frame arrives after its projected render
        // start: readiness/render learning owns that lateness instead.
        const uint64_t renderWaitOvershootUs =
            renderWait.deadlineAlreadyElapsed ||
                    renderWait.finalNowUs <= decision.renderStartUs ?
                0 : renderWait.finalNowUs - decision.renderStartUs;

        bool displayEpochInterrupted =
            (consumeWindowStateNotifications() &
                kVrrDisplayEpochStateMask) != 0;
        if (displayEpochInterrupted ||
                presentationSuspended() || isStopping()) {
            cancelPresentation();
            continue;
        }

        // A frame can become stale while the worker waits for its render
        // start. Leave the surface unprepared and let the next iteration start
        // fresh rather than rendering an avoidably old image.
        if (decision.sourcePeriodUs != 0 &&
            LiGetMicroseconds() > decision.targetUs + decision.sourcePeriodUs &&
            hasQueuedFrame()) {
            m_VideoStats->pacerDroppedFrames++;
            m_TimingController->rebase();
            continue;
        }

        const uint64_t preparationStartUs = LiGetMicroseconds();
        const VrrPrepareResult preparation =
            m_Presenter->prepareFrame(frame.frame());
        const uint64_t preparationEndUs = LiGetMicroseconds();
        const uint64_t preparationDurationUs =
            preparationEndUs >= preparationStartUs ?
                preparationEndUs - preparationStartUs : 0;
        m_TimingController->notePreparationDuration(preparationDurationUs);

        // Always drain, even when already interrupted: the drain is what
        // reconciles the presenter's suspension state and arms the rebase.
        displayEpochInterrupted =
            (consumeWindowStateNotifications() &
                kVrrDisplayEpochStateMask) != 0 || displayEpochInterrupted;
        if (!preparation.prepared || displayEpochInterrupted ||
                presentationSuspended() || isStopping()) {
            VrrPresentFeedback feedback = preparation.feedback;
            uint64_t operationStartUs = preparationStartUs;
            uint64_t operationEndUs = preparationEndUs;
            const bool mustCancel = !feedback.presented &&
                (preparation.prepared || preparation.cancellationMaySubmit ||
                 !feedback.cancelled);
            if (mustCancel) {
                // A presenter may need to submit an acquired image in order
                // to abandon it. It reports only that neutral fact; the worker
                // owns the target and display-spacing policy.
                if (preparation.cancellationMaySubmit) {
                    waitForSubmissionFloor(decision);
                }
                operationStartUs = LiGetMicroseconds();
                feedback = m_Presenter->cancelFrame();
                operationEndUs = LiGetMicroseconds();
            }
            feedback.cancelled = true;
            recordSubmission(feedback, operationStartUs, operationEndUs);
            m_VideoStats->pacerDroppedFrames++;
            m_DeferredFrame = std::move(frame);
            continue;
        }

        const VrrTargetWaitResult targetWait =
            m_TargetWaiter.waitUntil(decision.targetUs,
                                      decision.targetWakeLeadUs);
        // Always drain, even when already interrupted: the drain is what
        // reconciles the presenter's suspension state and arms the rebase.
        displayEpochInterrupted =
            (consumeWindowStateNotifications() &
                kVrrDisplayEpochStateMask) != 0 || displayEpochInterrupted;
        if (displayEpochInterrupted ||
                presentationSuspended() || isStopping()) {
            if (preparation.cancellationMaySubmit) {
                waitForSubmissionFloor(decision);
            }
            cancelPresentation();
            m_DeferredFrame = std::move(frame);
            continue;
        }

        // Recheck both mathematical floors immediately before Present. The
        // waiter deliberately has a bounded active phase, so a pathological
        // clock must not turn an early return into an early submission.
        // This frame's floor is fixed before the clean-spacing feedback, so a
        // guard decay can only take effect from the next frame onward.
        const uint64_t presentationFloorUs = std::max(
            decision.targetUs, m_TimingController->earliestSubmissionUs());
        m_TimingController->noteSpacingDeficit(0);
        while (LiGetMicroseconds() < presentationFloorUs) {
            m_TargetWaiter.waitUntil(presentationFloorUs);
        }

        if (m_TimingController->hasLastSubmission()) {
            const uint64_t minimumUntornUs =
                m_TimingController->lastSubmissionUs() +
                m_TimingController->displayPeriodUs();

            // A second check protects against a clock anomaly between the
            // first check and the actual call boundary.
            const uint64_t nowUs = LiGetMicroseconds();
            if (nowUs < minimumUntornUs) {
                m_TimingController->noteSpacingDeficit(minimumUntornUs - nowUs);
                const uint64_t correctedFloorUs =
                    m_TimingController->earliestSubmissionUs();
                while (LiGetMicroseconds() < correctedFloorUs) {
                    m_TargetWaiter.waitUntil(correctedFloorUs);
                }
            }
        }

        // The mathematical floor and its correction can add another
        // display-period wait after the primary target checkpoint. Drain
        // notifications once more at the actual submission boundary so a
        // minimize or a renderer-accepted display epoch cannot slip through
        // that final wait and be presented under the old decision.
        // Always drain, even when already interrupted: the drain is what
        // reconciles the presenter's suspension state and arms the rebase.
        displayEpochInterrupted =
            (consumeWindowStateNotifications() &
                kVrrDisplayEpochStateMask) != 0 || displayEpochInterrupted;
        if (displayEpochInterrupted ||
                presentationSuspended() || isStopping()) {
            cancelPresentation();
            m_DeferredFrame = std::move(frame);
            continue;
        }

        m_TimingController->noteSchedulerDelays(
            renderWaitOvershootUs,
            targetWait.schedulerDelayUs,
            targetWait.schedulerDelayValid);

        VrrPresentRequest presentRequest;
        presentRequest.latchedPresentation = decision.latchedPresentation;

        const uint64_t presentStartUs = LiGetMicroseconds();
        const VrrPresentFeedback feedback =
            m_Presenter->presentAdaptive(presentRequest);
        const uint64_t presentEndUs = LiGetMicroseconds();
        recordSubmission(feedback, presentStartUs, presentEndUs);

        if (feedback.presented && !feedback.cancelled) {
            m_VideoStats->totalPacerTimeUs +=
                preparationStartUs >= frame.decodeCompleteUs() ?
                    preparationStartUs - frame.decodeCompleteUs() : 0;
            m_VideoStats->totalRenderTimeUs += preparationDurationUs +
                (presentEndUs >= presentStartUs ?
                    presentEndUs - presentStartUs : 0);
            m_VideoStats->renderedFrames++;
        }
        else {
            m_VideoStats->pacerDroppedFrames++;
        }

        m_DeferredFrame = std::move(frame);
    }

    // Release any native state retained between preparation and presentation.
    m_Presenter->cancelFrame();
    return 0;
}

bool VrrPacingWorker::dequeueFrame(PacedFrame& frame)
{
    QMutexLocker lock(&m_FrameQueueLock);
    while (!isStopping() && !m_Suspended.load() && m_FrameQueue.empty()) {
        m_FrameQueueNotEmpty.wait(&m_FrameQueueLock);
    }

    if (isStopping()) {
        return false;
    }
    if (m_Suspended.load()) {
        return true;
    }

    frame = std::move(m_FrameQueue.front());
    m_FrameQueue.pop_front();
    return true;
}

bool VrrPacingWorker::hasQueuedFrame()
{
    QMutexLocker lock(&m_FrameQueueLock);
    return !m_FrameQueue.empty();
}

void VrrPacingWorker::discardQueuedFrames(bool countDrops)
{
    std::deque<PacedFrame> discardedFrames;
    {
        QMutexLocker lock(&m_FrameQueueLock);
        discardedFrames.swap(m_FrameQueue);
    }

    if (countDrops) {
        m_VideoStats->pacerDroppedFrames +=
            static_cast<uint32_t>(discardedFrames.size());
    }
}

uint32_t VrrPacingWorker::consumeWindowStateNotifications()
{
    uint32_t consumedFlags = m_PendingWindowStateFlags.exchange(0);
    if (consumedFlags == 0) {
        return 0;
    }

    // Minimize/restore can race while this worker is draining notifications.
    // Reconcile to the authoritative atomic state rather than applying a stale
    // flag snapshot in event order.
    bool suspended = m_Suspended.load();
    while (true) {
        const uint32_t newerFlags = m_PendingWindowStateFlags.exchange(0);
        consumedFlags |= newerFlags;
        const bool latestSuspended = m_Suspended.load();
        if (newerFlags == 0 && latestSuspended == suspended) {
            break;
        }
        suspended = latestSuspended;
    }

    if (suspended != m_PresenterSuspended) {
        m_Presenter->setSuspended(suspended);
        m_PresenterSuspended = suspended;
    }
    m_RebaseOnNextFrame = true;
    return consumedFlags;
}

bool VrrPacingWorker::presentationSuspended() const
{
    // If UI state changed just after notification draining, either side being
    // suspended is enough to prevent a misclassified finish. The next loop
    // reconciles the presenter to the newest authoritative state.
    return m_Suspended.load() || m_PresenterSuspended;
}

bool VrrPacingWorker::isStopping() const
{
    return m_Stopping.load();
}

void VrrPacingWorker::waitForSubmissionFloor(const VrrTimingDecision& decision)
{
    const uint64_t submissionFloorUs = std::max(
        decision.targetUs, m_TimingController->earliestSubmissionUs());
    if (LiGetMicroseconds() >= submissionFloorUs) {
        return;
    }

    m_TargetWaiter.waitUntil(submissionFloorUs, decision.targetWakeLeadUs);

    // The waiter deliberately bounds each active phase. Re-enter it until the
    // shared monotonic clock confirms the floor; an incomplete wait must never
    // become permission to submit.
    while (LiGetMicroseconds() < submissionFloorUs) {
        m_TargetWaiter.waitUntil(submissionFloorUs);
    }
}

void VrrPacingWorker::recordSubmission(const VrrPresentFeedback& feedback,
                                       uint64_t operationStartUs,
                                       uint64_t operationEndUs)
{
    m_TimingController->noteSubmission(
        feedback.presented, feedback.cancelled,
        submissionBoundaryUs(feedback, operationStartUs, operationEndUs));
}
