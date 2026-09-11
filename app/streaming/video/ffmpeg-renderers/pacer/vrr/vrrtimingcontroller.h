#pragma once

#include "vrrtypes.h"

#include <cstdint>
#include <deque>

struct VrrTimingDecision {
    uint64_t sourcePeriodUs = 0;
    uint64_t renderStartUs = 0;
    uint64_t targetUs = 0;
    uint64_t targetWakeLeadUs = 0;
    bool latchedPresentation = false;
};

// Platform-neutral, feed-forward VRR timing. The controller projects the
// sender clock into the local monotonic epoch and learns bounded readiness,
// render, scheduler, and spacing budgets. It contains no renderer/native API
// types; the worker translates platform observations into neutral timing
// feedback.
class VrrTimingController {
public:
    VrrTimingController(const VrrSessionConfig& config,
                        bool canLatchPresentation);

    // Starts a fresh source epoch while retaining learned render/wake budgets
    // and the last submission instant used by the display-spacing floor.
    void rebase();

    VrrTimingDecision schedule(const PacedFrame& frame, uint64_t nowUs);

    // Samples affect subsequent frames only. The current presentation target
    // never moves after rendering has begun.
    void notePreparationDuration(uint64_t preparationDurationUs);
    void noteSchedulerDelays(uint64_t renderDelayUs,
                             uint64_t targetDelayUs,
                             bool targetDelayValid);

    // A positive deficit means the worker reached the presentation boundary
    // before the display-spacing floor. The worker corrects the current frame;
    // this feedback adjusts one bounded guard for future frames.
    void noteSpacingDeficit(uint64_t deficitUs);

    // The worker records its own call boundary and supplies only the neutral
    // lifecycle result. The timing controller has no renderer/native types.
    void noteSubmission(bool submitted, bool cancelled,
                        uint64_t submissionUs);

    uint64_t displayPeriodUs() const;
    uint64_t earliestSubmissionUs() const;
    uint64_t lastSubmissionUs() const;
    bool hasLastSubmission() const;

private:
    struct PendingFrame {
        bool valid = false;
        bool cadenceEligible = false;
        bool hasPreparationDuration = false;
        int64_t readyOffsetUs = 0;
        uint64_t preparationDurationUs = 0;
    };

    struct CadenceObservation {
        uint64_t intervalUs = 0;
        uint64_t frameDelta = 1;
        bool eligible = false;
        bool sourceRateChanged = false;
        bool phaseDiscontinuity = false;
        bool needsRebase = false;
    };

    struct CadenceSample {
        uint64_t frameOrdinal = 0;
        uint64_t rtpTicks = 0;
    };

    void clearTimeline(bool retainLearnedBudgets);
    void initializeTimeline(const PacedFrame& frame);
    CadenceObservation observeCadence(const PacedFrame& frame);
    void observeRtpCadence(uint32_t rtpDelta,
                           CadenceObservation& observation);
    void appendCadenceSample(std::deque<CadenceSample>& samples,
                             const CadenceSample& sample);
    uint64_t fittedSourcePeriodQ16(
        const std::deque<CadenceSample>& samples) const;
    uint64_t cadenceWindowUs() const;
    bool isMajorCadenceDeparture(uint64_t intervalUs,
                                 uint64_t frameDelta) const;
    bool acceptSourcePeriodQ16(uint64_t periodUsQ16);
    void anchorSourceTime(uint64_t sourceTimeUs);
    void updateLearnedBudgets();
    void updateReadinessModel();
    void applyReadinessBudget(bool acquireReserve);

    uint64_t headroomUs() const;
    uint64_t renderLeadFloorUs() const;
    uint64_t renderLeadCeilingUs() const;
    uint64_t readinessCeilingUs() const;
    uint64_t guardCeilingUs() const;
    uint64_t latchedPresentationHeadroomUs() const;
    uint64_t latchedPresentationExitHeadroomUs() const;
    static uint64_t periodForRate(int rateHz, uint64_t fallbackUs);
    static uint64_t periodForRateQ16(int rateHz, uint64_t fallbackQ16);
    static uint64_t saturatingAdd(uint64_t left, uint64_t right);
    static uint64_t addSigned(uint64_t value, int64_t adjustment);
    static int64_t signedDifference(uint64_t left, uint64_t right);
    static uint64_t roundedQ16(uint64_t valueQ16);

    VrrSessionConfig m_Config;
    uint64_t m_ConfiguredStreamPeriodUs = 0;
    uint64_t m_ConfiguredStreamPeriodQ16 = 0;
    uint64_t m_DisplayPeriodUs = 0;
    uint64_t m_BaseGuardUs = 0;
    uint64_t m_GuardUs = 0;

    uint64_t m_SourcePeriodUs = 0;
    uint64_t m_SourcePeriodUsQ16 = 0;
    int64_t m_ReadinessBudgetUs = 0;
    int64_t m_ReadinessPhaseUs = 0;
    uint64_t m_ReadinessDemandUs = 0;
    bool m_ReadinessModelValid = false;
    uint64_t m_RenderLeadUs = 0;
    uint64_t m_RenderWakeLeadUs = 0;
    uint64_t m_TargetWakeLeadUs = 0;
    bool m_CanLatchPresentation = true;
    bool m_LatchedPresentation = false;

    bool m_HaveTimeline = false;
    uint64_t m_SourceTimeUs = 0;
    uint64_t m_SourceTimeUsQ16 = 0;
    uint64_t m_SourceFrameOrdinal = 0;
    uint64_t m_UnwrappedRtpTicks = 0;
    int m_LastFrameNumber = -1;
    bool m_HaveLastFrameNumber = false;
    uint32_t m_LastRtpTimestamp = 0;
    bool m_LastTimestampValid = false;
    // Fractional-rate remainder carry. The carried remainder belongs to the
    // cadence source that produced it, so switching between RTP and frame
    // numbers must not reuse the other source's remainder.
    uint64_t m_RtpConversionRemainder = 0;
    uint64_t m_FrameConversionRemainder = 0;
    bool m_LastCadenceUsedRtp = false;

    bool m_HaveLastSubmission = false;
    uint64_t m_LastSubmissionUs = 0;
    unsigned int m_CleanSpacingFrames = 0;
    unsigned int m_PhaseErrorFrames = 0;

    std::deque<CadenceSample> m_CadenceSamples;
    std::deque<CadenceSample> m_RateCandidateSamples;
    std::deque<int64_t> m_ReadyOffsets;
    std::deque<uint64_t> m_PreparationDurations;
    std::deque<uint64_t> m_RenderSchedulerDelays;
    std::deque<uint64_t> m_TargetSchedulerDelays;

    PendingFrame m_Pending;
};
