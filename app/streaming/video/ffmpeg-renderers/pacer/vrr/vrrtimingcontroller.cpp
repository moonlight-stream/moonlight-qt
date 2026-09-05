#include "vrrtimingcontroller.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

namespace {

constexpr uint64_t kMicrosecondsPerSecond = 1000000ULL;
constexpr uint64_t kRtpClockRate = 90000ULL;
constexpr uint64_t kQ16One = 1ULL << 16;
constexpr uint64_t kQ16Half = kQ16One >> 1;

// Display-spacing guard.
constexpr uint64_t kBaseGuardDivisor = 96;
constexpr uint64_t kMinimumGuardUs = 100;
constexpr uint64_t kMaximumBaseGuardUs = 250;
constexpr uint64_t kMaximumAdaptiveGuardUs = 1000;
constexpr uint64_t kGuardStepUs = 50;
constexpr size_t kGuardDecayFrames = 120;

// Render and scheduler lead budgets.
constexpr uint64_t kRenderLeadFloorUs = 1000;
constexpr uint64_t kRenderLeadCeilingUs = 6500;
constexpr uint64_t kMaximumRenderWakeLeadUs = 2000;
constexpr uint64_t kMaximumTargetWakeLeadUs = 500;
constexpr unsigned int kPreparationPercentile = 99;
constexpr unsigned int kSchedulerPercentile = 95;
constexpr size_t kPreparationLearningSamples = 96;
constexpr size_t kSchedulerLearningSamples = 19;

// Readiness reserve.
constexpr uint64_t kReadinessCeilingUs = 10000;
constexpr uint64_t kColdStartReadinessDemandUs = 1500;
constexpr uint64_t kMinimumReadinessReserveUs = 500;
constexpr uint64_t kArrivalSpreadGuardUs = 900;
constexpr uint64_t kReadinessAcquireStepUs = 1000;
constexpr uint64_t kReadinessReleaseDivisor = 32;
constexpr uint64_t kUsableHeadroomNumerator = 3;
constexpr uint64_t kUsableHeadroomDenominator = 4;
constexpr size_t kReadinessLearningSamples = 16;
constexpr size_t kMinimumReadinessSamples = 16;
constexpr unsigned int kReadinessLoosePercentile = 80;
constexpr unsigned int kReadinessTightPercentile = 100;

// Source cadence tracking.
constexpr uint64_t kMaximumForwardMovementUs = 1000000;
constexpr size_t kMinimumCadenceSamples = 6;
constexpr size_t kMaximumCadenceSamples = 512;
constexpr size_t kRateCandidateSamples = 3;
constexpr uint64_t kLooseCadenceWindowUs = 350000;
constexpr uint64_t kTightCadenceWindowUs = 1000000;
constexpr uint64_t kLooseHeadroomDisplayPeriods = 2;
constexpr uint64_t kMajorCadenceRatioNumerator = 7;
constexpr uint64_t kMajorCadenceRatioDenominator = 2;
constexpr uint64_t kCandidateCadenceRatio = 2;
constexpr unsigned int kMaterialRateChangePercent = 12;
constexpr size_t kPhaseErrorFrames = 3;

// Latched near-refresh presentation hysteresis.
constexpr uint64_t kLatchEnterHeadroomUs = 1500;
constexpr uint64_t kLatchEnterHeadroomPeriods = 3;
constexpr uint64_t kLatchExitHeadroomUs = 2000;
constexpr uint64_t kLatchExitHeadroomPeriodNumerator = 13;
constexpr uint64_t kLatchExitHeadroomPeriodDenominator = 4;

uint64_t clampUnsigned(uint64_t value, uint64_t low, uint64_t high)
{
    if (high < low) {
        high = low;
    }
    return std::max(low, std::min(value, high));
}

template<typename T>
T percentile(const std::deque<T>& values, unsigned int requestedPercentile)
{
    if (values.empty()) {
        return 0;
    }
    std::vector<T> ordered(values.begin(), values.end());
    std::sort(ordered.begin(), ordered.end());
    const size_t rank = std::max<size_t>(
        1, (ordered.size() * requestedPercentile + 99) / 100);
    return ordered[rank - 1];
}

template<typename T>
void appendBounded(std::deque<T>& values, T value, size_t limit)
{
    while (values.size() >= limit) {
        values.pop_front();
    }
    values.push_back(value);
}

} // namespace

VrrTimingController::VrrTimingController(const VrrSessionConfig& config,
                                         bool canLatchPresentation) :
    m_Config(config),
    m_CanLatchPresentation(canLatchPresentation)
{
    m_DisplayPeriodUs = periodForRate(m_Config.displayRefreshHz, 16667);
    m_ConfiguredStreamPeriodQ16 = periodForRateQ16(
        m_Config.streamRateHz, m_DisplayPeriodUs * kQ16One);
    m_ConfiguredStreamPeriodUs = std::max<uint64_t>(
        1, roundedQ16(m_ConfiguredStreamPeriodQ16));
    m_BaseGuardUs = clampUnsigned(m_DisplayPeriodUs / kBaseGuardDivisor,
                                  kMinimumGuardUs,
                                  kMaximumBaseGuardUs);
    clearTimeline(false);
}

void VrrTimingController::rebase()
{
    clearTimeline(true);
}

void VrrTimingController::clearTimeline(bool retainLearnedBudgets)
{
    const uint64_t previousReadinessDemandUs = m_ReadinessDemandUs;
    const bool previousReadinessModelValid = m_ReadinessModelValid;

    m_SourcePeriodUsQ16 = m_ConfiguredStreamPeriodQ16;
    m_SourcePeriodUs = std::max<uint64_t>(
        1, roundedQ16(m_SourcePeriodUsQ16));
    m_LatchedPresentation = m_CanLatchPresentation && m_SourcePeriodUs <
        saturatingAdd(m_DisplayPeriodUs,
                      latchedPresentationHeadroomUs());
    m_ReadinessBudgetUs = 0;
    m_ReadinessPhaseUs = 0;
    m_ReadinessDemandUs = retainLearnedBudgets ?
        previousReadinessDemandUs : kColdStartReadinessDemandUs;
    m_ReadinessModelValid = retainLearnedBudgets &&
        previousReadinessModelValid;
    m_HaveTimeline = false;
    m_SourceTimeUs = 0;
    m_SourceTimeUsQ16 = 0;
    m_SourceFrameOrdinal = 0;
    m_UnwrappedRtpTicks = 0;
    m_LastFrameNumber = -1;
    m_HaveLastFrameNumber = false;
    m_LastRtpTimestamp = 0;
    m_LastTimestampValid = false;
    m_RtpConversionRemainder = 0;
    m_FrameConversionRemainder = 0;
    m_LastCadenceUsedRtp = false;
    m_PhaseErrorFrames = 0;

    m_CadenceSamples.clear();
    m_RateCandidateSamples.clear();
    m_ReadyOffsets.clear();
    m_PreparationDurations.clear();
    m_RenderSchedulerDelays.clear();
    m_TargetSchedulerDelays.clear();
    m_Pending = PendingFrame {};

    if (retainLearnedBudgets) {
        m_RenderLeadUs = clampUnsigned(m_RenderLeadUs,
                                       renderLeadFloorUs(),
                                       renderLeadCeilingUs());
        m_RenderWakeLeadUs = std::min(m_RenderWakeLeadUs,
                                      kMaximumRenderWakeLeadUs);
        m_TargetWakeLeadUs = std::min(m_TargetWakeLeadUs,
                                      kMaximumTargetWakeLeadUs);
        m_GuardUs = clampUnsigned(m_GuardUs,
                                  m_BaseGuardUs,
                                  guardCeilingUs());
    }
    else {
        m_RenderLeadUs = clampUnsigned(kRenderLeadFloorUs,
                                       renderLeadFloorUs(),
                                       renderLeadCeilingUs());
        m_RenderWakeLeadUs = 0;
        m_TargetWakeLeadUs = 0;
        m_GuardUs = m_BaseGuardUs;
    }
}

void VrrTimingController::initializeTimeline(const PacedFrame& frame)
{
    m_HaveTimeline = true;
    anchorSourceTime(frame.decodeCompleteUs());
    m_SourceFrameOrdinal = 0;
    m_UnwrappedRtpTicks = 0;
    m_LastFrameNumber = frame.frameNumber();
    m_HaveLastFrameNumber = frame.frameNumber() >= 0;
    m_LastRtpTimestamp = frame.rtpTimestamp();
    m_LastTimestampValid = frame.timestampValid();
    m_CadenceSamples.clear();
    m_RateCandidateSamples.clear();
    if (frame.timestampValid()) {
        m_CadenceSamples.push_back(CadenceSample {});
    }
}

VrrTimingDecision VrrTimingController::schedule(const PacedFrame& frame,
                                                 uint64_t nowUs)
{
    m_Pending = PendingFrame {};

    CadenceObservation cadence;
    bool rebased = false;
    if (!m_HaveTimeline) {
        initializeTimeline(frame);
        rebased = true;
    }
    else {
        const bool frameNumberReset =
            m_HaveLastFrameNumber && frame.frameNumber() >= 0 &&
            frame.frameNumber() <= m_LastFrameNumber;

        if (frameNumberReset) {
            rebase();
            initializeTimeline(frame);
            rebased = true;
        }
        else {
            cadence = observeCadence(frame);
            if (cadence.needsRebase) {
                rebase();
                initializeTimeline(frame);
                rebased = true;
            }
            else {
                const uint64_t maximum = std::numeric_limits<uint64_t>::max();
                const uint64_t projectedMovementQ16 =
                    cadence.frameDelta != 0 &&
                    m_SourcePeriodUsQ16 > maximum / cadence.frameDelta ?
                        maximum : m_SourcePeriodUsQ16 * cadence.frameDelta;
                m_SourceTimeUsQ16 = saturatingAdd(m_SourceTimeUsQ16,
                                                   projectedMovementQ16);
                m_SourceTimeUs = roundedQ16(m_SourceTimeUsQ16);

                if (cadence.phaseDiscontinuity) {
                    // A large cadence transition or isolated source gap is a
                    // local phase event, not a reason to forget the learned
                    // rate. Anchor the live one-slot path to the ready frame
                    // while the cumulative estimator confirms or abandons its
                    // provisional segment.
                    anchorSourceTime(frame.decodeCompleteUs());
                }

                m_LastFrameNumber = frame.frameNumber();
                m_HaveLastFrameNumber = frame.frameNumber() >= 0;
                m_LastRtpTimestamp = frame.rtpTimestamp();
                m_LastTimestampValid = frame.timestampValid();
            }
        }
    }

    int64_t readyOffsetUs = signedDifference(frame.decodeCompleteUs(),
                                              m_SourceTimeUs);

    // Local phase recovery: re-anchor the live one-slot path on this frame
    // while retaining the cumulative cadence fit.
    const auto reanchorPhase = [&]() {
        anchorSourceTime(frame.decodeCompleteUs());
        readyOffsetUs = 0;
        cadence.phaseDiscontinuity = true;
        cadence.eligible = false;
        m_PhaseErrorFrames = 0;
    };

    if (!rebased && !cadence.phaseDiscontinuity && cadence.eligible) {
        const int64_t ceilingUs = static_cast<int64_t>(readinessCeilingUs());
        if (readyOffsetUs < -ceilingUs) {
            // A frame that is ready well before the old slower clock must not
            // wait behind an obsolete cutscene cadence. The display floor and
            // latched near-refresh mode still bound how quickly it can submit.
            reanchorPhase();
        }
        else if (readyOffsetUs > ceilingUs) {
            ++m_PhaseErrorFrames;
        }
        else {
            m_PhaseErrorFrames = 0;
        }

        if (!cadence.phaseDiscontinuity &&
                m_PhaseErrorFrames >= kPhaseErrorFrames) {
            // A bounded readiness reserve cannot repay a sustained source
            // phase error. Re-anchor locally while retaining the cumulative
            // cadence fit, rather than repeatedly rebasing the whole model.
            reanchorPhase();
        }
    }
    else {
        m_PhaseErrorFrames = 0;
    }
    if (rebased || cadence.sourceRateChanged ||
        cadence.phaseDiscontinuity) {
        // A new source epoch or local phase recovery is anchored by the first
        // directly observed ready offset. Cadence history is retained for the
        // phase-only cases above.
        m_ReadyOffsets.clear();
        const int64_t ceilingUs = static_cast<int64_t>(readinessCeilingUs());
        m_ReadinessPhaseUs = std::max(
            -ceilingUs, std::min(readyOffsetUs, ceilingUs));
        // A source-phase reset must not acquire a standing reserve in one
        // cadence-breaking jump. Start on the observed phase and let clean
        // arrival evidence build or release the reserve smoothly.
        applyReadinessBudget(false);
    }

    uint64_t targetUs = saturatingAdd(
        std::max(addSigned(m_SourceTimeUs, m_ReadinessBudgetUs), nowUs),
        m_RenderLeadUs);

    // This is a live, one-slot path. An unconfirmed RTP/frame jump may
    // describe already-skipped content, never hundreds of milliseconds that
    // the client should wait again. Reseed poisoned playout phase without
    // discarding the cumulative cadence model.
    const uint64_t maximumDirectTargetUs = saturatingAdd(
        nowUs,
        saturatingAdd(std::max(m_ConfiguredStreamPeriodUs,
                               m_SourcePeriodUs),
                      m_RenderLeadUs));
    if (targetUs > maximumDirectTargetUs) {
        // Do not clear cadence history when a faster source makes the old
        // playout phase point into the future. Reseed phase from this already
        // decoded frame and let the cumulative fit heal the rate.
        reanchorPhase();
        m_ReadyOffsets.clear();
        m_ReadinessBudgetUs = 0;
        m_ReadinessPhaseUs = 0;
        targetUs = saturatingAdd(std::max(frame.decodeCompleteUs(), nowUs),
                                 m_RenderLeadUs);
    }

    targetUs = std::max(targetUs, earliestSubmissionUs());
    const uint64_t totalLeadUs = saturatingAdd(m_RenderLeadUs,
                                               m_RenderWakeLeadUs);

    VrrTimingDecision decision;
    decision.sourcePeriodUs = m_SourcePeriodUs;
    decision.renderStartUs = targetUs > totalLeadUs ?
        targetUs - totalLeadUs : 0;
    decision.targetUs = targetUs;
    decision.targetWakeLeadUs = m_TargetWakeLeadUs;

    const uint64_t learnedHeadroomUs = headroomUs();
    if (!m_CanLatchPresentation) {
        m_LatchedPresentation = false;
    }
    else if (m_LatchedPresentation) {
        // A temporary spacing correction can take a cadence just below the
        // entry threshold and correctly select the conservative path. Once
        // that guard has completely decayed, however, retaining the wider
        // hysteresis band would make an otherwise safe 100 FPS / 120 Hz
        // stream stay latched indefinitely. Keep hysteresis while protection
        // is still elevated, then restore immediate presentation at the
        // normal eligibility boundary.
        if (learnedHeadroomUs >=
                latchedPresentationExitHeadroomUs() ||
            (m_GuardUs == m_BaseGuardUs &&
             learnedHeadroomUs >=
                latchedPresentationHeadroomUs())) {
            m_LatchedPresentation = false;
        }
    }
    else if (learnedHeadroomUs <
             latchedPresentationHeadroomUs()) {
        m_LatchedPresentation = true;
    }
    decision.latchedPresentation = m_LatchedPresentation;

    m_Pending.valid = true;
    m_Pending.cadenceEligible = !rebased && cadence.eligible;
    m_Pending.readyOffsetUs = readyOffsetUs;
    return decision;
}

VrrTimingController::CadenceObservation
VrrTimingController::observeCadence(const PacedFrame& frame)
{
    CadenceObservation observation;

    uint64_t frameDelta = 1;
    if (m_HaveLastFrameNumber && frame.frameNumber() >= 0) {
        if (frame.frameNumber() <= m_LastFrameNumber) {
            observation.needsRebase = true;
            return observation;
        }
        frameDelta = static_cast<uint64_t>(frame.frameNumber() -
                                           m_LastFrameNumber);
    }
    observation.frameDelta = frameDelta;

    if (frame.timestampValid() && m_LastTimestampValid) {
        const uint32_t wrappedDelta =
            frame.rtpTimestamp() - m_LastRtpTimestamp;
        if (wrappedDelta == 0 || wrappedDelta > 0x7fffffffU) {
            observation.needsRebase = true;
            return observation;
        }

        const uint64_t carriedRemainder = m_LastCadenceUsedRtp ?
            m_RtpConversionRemainder : 0;
        const uint64_t intervalNumerator =
            static_cast<uint64_t>(wrappedDelta) * kMicrosecondsPerSecond +
            carriedRemainder;
        const uint64_t intervalUs = intervalNumerator / kRtpClockRate;
        if (intervalUs > kMaximumForwardMovementUs) {
            observation.needsRebase = true;
            return observation;
        }

        m_RtpConversionRemainder = intervalNumerator % kRtpClockRate;
        m_FrameConversionRemainder = 0;
        m_LastCadenceUsedRtp = true;

        observation.intervalUs = intervalUs;
        observeRtpCadence(wrappedDelta, observation);
        return observation;
    }

    if (m_Config.streamRateHz <= 0 ||
        frameDelta > static_cast<uint64_t>(m_Config.streamRateHz)) {
        observation.needsRebase = true;
        return observation;
    }

    const uint64_t carriedRemainder = !m_LastCadenceUsedRtp ?
        m_FrameConversionRemainder : 0;
    const uint64_t intervalNumerator =
        frameDelta * kMicrosecondsPerSecond + carriedRemainder;
    observation.intervalUs = intervalNumerator /
        static_cast<uint64_t>(m_Config.streamRateHz);
    m_FrameConversionRemainder = intervalNumerator %
        static_cast<uint64_t>(m_Config.streamRateHz);
    m_RtpConversionRemainder = 0;
    m_LastCadenceUsedRtp = false;
    m_CadenceSamples.clear();
    m_RateCandidateSamples.clear();
    observation.eligible = frameDelta == 1;
    return observation;
}

void VrrTimingController::observeRtpCadence(
    uint32_t rtpDelta, CadenceObservation& observation)
{
    const CadenceSample previous {
        m_SourceFrameOrdinal,
        m_UnwrappedRtpTicks,
    };
    m_SourceFrameOrdinal = saturatingAdd(m_SourceFrameOrdinal,
                                          observation.frameDelta);
    m_UnwrappedRtpTicks = saturatingAdd(m_UnwrappedRtpTicks,
                                         static_cast<uint64_t>(rtpDelta));
    const CadenceSample current {
        m_SourceFrameOrdinal,
        m_UnwrappedRtpTicks,
    };

    const bool majorDeparture = isMajorCadenceDeparture(
        observation.intervalUs, observation.frameDelta);

    if (!m_RateCandidateSamples.empty()) {
        // A normal interval immediately after a major departure identifies an
        // isolated source/capture gap. Preserve the stable rate, discard the
        // provisional segment, and begin a fresh cumulative phase at the
        // current sample.
        const uint64_t observedPeriodUs = std::max<uint64_t>(
            1, observation.intervalUs / observation.frameDelta);
        const bool returnedToStableCadence =
            observedPeriodUs <= m_SourcePeriodUs * kCandidateCadenceRatio &&
            m_SourcePeriodUs <= observedPeriodUs * kCandidateCadenceRatio;
        if (returnedToStableCadence) {
            m_RateCandidateSamples.clear();
            m_CadenceSamples.clear();
            m_CadenceSamples.push_back(current);
            observation.phaseDiscontinuity = true;
            return;
        }

        appendCadenceSample(m_RateCandidateSamples, current);
        observation.phaseDiscontinuity = true;
        if (m_RateCandidateSamples.size() >= kRateCandidateSamples) {
            const uint64_t candidatePeriodQ16 = fittedSourcePeriodQ16(
                m_RateCandidateSamples);
            if (candidatePeriodQ16 != 0) {
                observation.sourceRateChanged = acceptSourcePeriodQ16(
                    candidatePeriodQ16);
                m_CadenceSamples = m_RateCandidateSamples;
                m_RateCandidateSamples.clear();
                observation.eligible = true;
            }
        }
        return;
    }

    if (majorDeparture) {
        m_RateCandidateSamples.push_back(previous);
        m_RateCandidateSamples.push_back(current);
        observation.phaseDiscontinuity = true;
        return;
    }

    appendCadenceSample(m_CadenceSamples, current);
    observation.eligible = true;
    if (m_CadenceSamples.size() >= kMinimumCadenceSamples) {
        const uint64_t fittedPeriodQ16 = fittedSourcePeriodQ16(
            m_CadenceSamples);
        if (fittedPeriodQ16 != 0 &&
            acceptSourcePeriodQ16(fittedPeriodQ16)) {
            observation.sourceRateChanged = true;
            observation.phaseDiscontinuity = true;
        }
    }
}

void VrrTimingController::appendCadenceSample(
    std::deque<CadenceSample>& samples, const CadenceSample& sample)
{
    samples.push_back(sample);
    while (samples.size() > kMaximumCadenceSamples) {
        samples.pop_front();
    }

    while (samples.size() > kMinimumCadenceSamples) {
        const uint64_t spanTicks = samples.back().rtpTicks -
            samples.front().rtpTicks;
        const uint64_t spanUs = spanTicks * kMicrosecondsPerSecond /
            kRtpClockRate;
        if (spanUs <= cadenceWindowUs()) {
            break;
        }
        samples.pop_front();
    }
}

uint64_t VrrTimingController::fittedSourcePeriodQ16(
    const std::deque<CadenceSample>& samples) const
{
    if (samples.size() < 2) {
        return 0;
    }

    const CadenceSample& first = samples.front();
    const CadenceSample& last = samples.back();
    if (last.frameOrdinal <= first.frameOrdinal ||
        last.rtpTicks <= first.rtpTicks) {
        return 0;
    }

    // RTP timestamps from a host-refresh-quantized source form a staircase.
    // OLS weighs the placement of each staircase step, so its slope breathes
    // as a long atom crosses the window.  The endpoint span measures only
    // the net source movement and still accounts for skipped local frames.
    const uint64_t spanFrames = last.frameOrdinal - first.frameOrdinal;
    const uint64_t spanTicks = last.rtpTicks - first.rtpTicks;
    constexpr uint64_t kPeriodQ16Scale =
        kMicrosecondsPerSecond * kQ16One;
    const uint64_t maximum = std::numeric_limits<uint64_t>::max();

    // The cadence window is at most one previous window plus one valid
    // one-second RTP interval, so these products are comfortably 64-bit in
    // normal operation. Keep explicit guards for malformed frame numbers.
    if (spanTicks > maximum / kPeriodQ16Scale ||
        spanFrames > maximum / kRtpClockRate) {
        return 0;
    }

    const uint64_t numerator = spanTicks * kPeriodQ16Scale;
    const uint64_t denominator = spanFrames * kRtpClockRate;
    const uint64_t quotient = numerator / denominator;
    const uint64_t remainder = numerator % denominator;
    const uint64_t halfway = denominator / 2 + denominator % 2;
    if (remainder >= halfway && quotient < maximum) {
        return quotient + 1;
    }
    return quotient;
}

uint64_t VrrTimingController::cadenceWindowUs() const
{
    const uint64_t cadenceHeadroomUs = headroomUs();
    const uint64_t looseHeadroomUs =
        m_DisplayPeriodUs * kLooseHeadroomDisplayPeriods;
    if (cadenceHeadroomUs >= looseHeadroomUs) {
        return kLooseCadenceWindowUs;
    }
    if (cadenceHeadroomUs <= m_DisplayPeriodUs) {
        return kTightCadenceWindowUs;
    }

    const uint64_t tightnessNumerator = looseHeadroomUs - cadenceHeadroomUs;
    const uint64_t windowRangeUs = kTightCadenceWindowUs -
        kLooseCadenceWindowUs;
    return kLooseCadenceWindowUs +
        windowRangeUs * tightnessNumerator /
            std::max<uint64_t>(1, looseHeadroomUs - m_DisplayPeriodUs);
}

bool VrrTimingController::isMajorCadenceDeparture(
    uint64_t intervalUs, uint64_t frameDelta) const
{
    if (intervalUs == 0 || frameDelta == 0 || m_SourcePeriodUs == 0) {
        return false;
    }
    const uint64_t observedPeriodUs = std::max<uint64_t>(
        1, intervalUs / frameDelta);
    return observedPeriodUs * kMajorCadenceRatioDenominator >
               m_SourcePeriodUs * kMajorCadenceRatioNumerator ||
        m_SourcePeriodUs * kMajorCadenceRatioDenominator >
               observedPeriodUs * kMajorCadenceRatioNumerator;
}

bool VrrTimingController::acceptSourcePeriodQ16(uint64_t periodUsQ16)
{
    if (periodUsQ16 == 0) {
        return false;
    }

    // The negotiated stream rate is an upper bound on source FPS. Preserve
    // it at Q16 precision so fractional rates do not acquire an artificial
    // drift from the rounded microsecond period.
    periodUsQ16 = std::max(periodUsQ16, m_ConfiguredStreamPeriodQ16);
    const uint64_t previousPeriodUs = m_SourcePeriodUs;
    m_SourcePeriodUsQ16 = periodUsQ16;
    m_SourcePeriodUs = std::max<uint64_t>(1, roundedQ16(periodUsQ16));
    m_RenderLeadUs = clampUnsigned(m_RenderLeadUs,
                                   renderLeadFloorUs(),
                                   renderLeadCeilingUs());
    m_GuardUs = clampUnsigned(m_GuardUs,
                              m_BaseGuardUs,
                              guardCeilingUs());

    // Only a material departure from the previous period counts as a rate
    // change for the caller's phase/eligibility handling.
    if (previousPeriodUs == 0) {
        return m_SourcePeriodUs != 0;
    }
    const uint64_t differenceUs = m_SourcePeriodUs > previousPeriodUs ?
        m_SourcePeriodUs - previousPeriodUs :
        previousPeriodUs - m_SourcePeriodUs;
    return differenceUs * 100 >
        previousPeriodUs * kMaterialRateChangePercent;
}

void VrrTimingController::anchorSourceTime(uint64_t sourceTimeUs)
{
    m_SourceTimeUs = sourceTimeUs;
    const uint64_t maximum = std::numeric_limits<uint64_t>::max();
    m_SourceTimeUsQ16 = sourceTimeUs > maximum / kQ16One ?
        maximum : sourceTimeUs * kQ16One;
}

void VrrTimingController::notePreparationDuration(
    uint64_t preparationDurationUs)
{
    if (!m_Pending.valid) {
        return;
    }
    m_Pending.hasPreparationDuration = true;
    m_Pending.preparationDurationUs = preparationDurationUs;
}

void VrrTimingController::noteSchedulerDelays(uint64_t renderDelayUs,
                                              uint64_t targetDelayUs,
                                              bool targetDelayValid)
{
    appendBounded(m_RenderSchedulerDelays, renderDelayUs,
                  kSchedulerLearningSamples);
    if (targetDelayValid) {
        appendBounded(m_TargetSchedulerDelays, targetDelayUs,
                      kSchedulerLearningSamples);
    }
    updateLearnedBudgets();
}

void VrrTimingController::noteSpacingDeficit(uint64_t deficitUs)
{
    if (deficitUs != 0) {
        m_CleanSpacingFrames = 0;
        const uint64_t increaseUs = std::max(kGuardStepUs, deficitUs);
        m_GuardUs = std::min(guardCeilingUs(),
                             saturatingAdd(m_GuardUs, increaseUs));
        return;
    }

    if (m_GuardUs > m_BaseGuardUs &&
        ++m_CleanSpacingFrames >= kGuardDecayFrames) {
        m_GuardUs -= std::min(kGuardStepUs, m_GuardUs - m_BaseGuardUs);
        m_CleanSpacingFrames = 0;
    }
}

void VrrTimingController::noteSubmission(bool submitted, bool cancelled,
                                         uint64_t submissionUs)
{
    if (!m_Pending.valid) {
        return;
    }

    if (submitted) {
        // Cancellation is a reason, not proof that nothing reached the native
        // presentation queue (Vulkan must submit some abandoned images).
        m_HaveLastSubmission = true;
        m_LastSubmissionUs = submissionUs;

        if (!cancelled) {
            if (m_Pending.cadenceEligible) {
                appendBounded(m_ReadyOffsets, m_Pending.readyOffsetUs,
                              kReadinessLearningSamples);
            }
            if (m_Pending.hasPreparationDuration) {
                appendBounded(m_PreparationDurations,
                              m_Pending.preparationDurationUs,
                              kPreparationLearningSamples);
            }
            updateReadinessModel();
            updateLearnedBudgets();
        }
    }

    m_Pending = PendingFrame {};
}

void VrrTimingController::updateLearnedBudgets()
{
    if (!m_PreparationDurations.empty()) {
        m_RenderLeadUs = clampUnsigned(
            percentile(m_PreparationDurations, kPreparationPercentile),
            renderLeadFloorUs(), renderLeadCeilingUs());
    }

    if (!m_RenderSchedulerDelays.empty()) {
        m_RenderWakeLeadUs = std::min(
            kMaximumRenderWakeLeadUs,
            percentile(m_RenderSchedulerDelays, kSchedulerPercentile));
    }

    if (!m_TargetSchedulerDelays.empty()) {
        m_TargetWakeLeadUs = std::min(
            kMaximumTargetWakeLeadUs,
            percentile(m_TargetSchedulerDelays, kSchedulerPercentile));
    }
}

void VrrTimingController::updateReadinessModel()
{
    if (m_ReadyOffsets.size() < kMinimumReadinessSamples) {
        return;
    }

    // Learn exogenous decode-arrival variation, not absolute source phase or
    // queue age created by this controller. The earliest observed offset is
    // the local phase baseline. Near the display ceiling, preserve the whole
    // arrival tail because there is too little cadence slack to absorb a late
    // frame. Slower sources can use p80 and avoid making the slowest fifth
    // standing latency for every frame.
    const int64_t lowUs = *std::min_element(m_ReadyOffsets.begin(),
                                            m_ReadyOffsets.end());
    const unsigned int highPercentile = headroomUs() > m_DisplayPeriodUs ?
        kReadinessLoosePercentile : kReadinessTightPercentile;
    const int64_t highUs = percentile(m_ReadyOffsets, highPercentile);
    const uint64_t spreadUs = highUs > lowUs ?
        static_cast<uint64_t>(highUs - lowUs) : 0;
    const uint64_t candidateDemandUs = clampUnsigned(
        saturatingAdd(spreadUs, kArrivalSpreadGuardUs),
        kMinimumReadinessReserveUs, readinessCeilingUs());

    if (!m_ReadinessModelValid) {
        m_ReadinessDemandUs = candidateDemandUs;
        m_ReadinessModelValid = true;
    }
    else if (candidateDemandUs > m_ReadinessDemandUs) {
        // Attack immediately, release slowly.
        m_ReadinessDemandUs = candidateDemandUs;
    }
    else if (candidateDemandUs < m_ReadinessDemandUs) {
        const uint64_t difference = m_ReadinessDemandUs - candidateDemandUs;
        m_ReadinessDemandUs -= std::max<uint64_t>(
            1, difference / kReadinessReleaseDivisor);
    }

    m_ReadinessPhaseUs = lowUs;
    applyReadinessBudget(true);
}

void VrrTimingController::applyReadinessBudget(bool acquireReserve)
{
    // Cadence slack can absorb most arrival variation without committing a
    // decoded frame early. Preserve one quarter as service margin, and retain
    // a small floor near the panel ceiling where Mailbox still needs a
    // standing cadence cushion. In this projected-source-clock design, small
    // cadence slack cannot substitute for readiness reserve: doing so lets
    // quantized late arrivals clamp the target to "now" and turns their 8/16
    // ms atoms into visible presentation bursts. Credit slack only when at
    // least a full additional display period is available; tight and
    // near-ceiling cadences retain the complete learned cushion.
    const uint64_t cadenceHeadroomUs = headroomUs();
    const uint64_t usableHeadroomUs =
        m_SourcePeriodUs >= m_DisplayPeriodUs * kLooseHeadroomDisplayPeriods ?
            cadenceHeadroomUs * kUsableHeadroomNumerator /
                kUsableHeadroomDenominator : 0;
    const uint64_t effectiveDemandUs = m_ReadinessModelValid ?
        m_ReadinessDemandUs : kColdStartReadinessDemandUs;
    const uint64_t appliedReserveUs = std::max(
        kMinimumReadinessReserveUs,
        effectiveDemandUs > usableHeadroomUs ?
            effectiveDemandUs - usableHeadroomUs : 0);

    const int64_t ceilingUs = static_cast<int64_t>(readinessCeilingUs());
    const int64_t reserveUs = static_cast<int64_t>(
        std::min<uint64_t>(appliedReserveUs,
                           static_cast<uint64_t>(ceilingUs)));
    const int64_t desiredUs = m_ReadinessPhaseUs >
            std::numeric_limits<int64_t>::max() - reserveUs ?
        std::numeric_limits<int64_t>::max() :
        m_ReadinessPhaseUs + reserveUs;
    const int64_t clampedDesiredUs = std::max(
        -ceilingUs, std::min(desiredUs, ceilingUs));
    if (!acquireReserve) {
        m_ReadinessBudgetUs = std::max(
            -ceilingUs, std::min(m_ReadinessPhaseUs, ceilingUs));
    }
    else if (clampedDesiredUs > m_ReadinessBudgetUs) {
        m_ReadinessBudgetUs += std::min<int64_t>(
            clampedDesiredUs - m_ReadinessBudgetUs,
            static_cast<int64_t>(kReadinessAcquireStepUs));
    }
    else if (clampedDesiredUs < m_ReadinessBudgetUs) {
        m_ReadinessBudgetUs -= std::min<int64_t>(
            m_ReadinessBudgetUs - clampedDesiredUs,
            static_cast<int64_t>(kReadinessAcquireStepUs));
    }
}

uint64_t VrrTimingController::headroomUs() const
{
    const uint64_t floorUs = saturatingAdd(m_DisplayPeriodUs, m_GuardUs);
    return m_SourcePeriodUs > floorUs ? m_SourcePeriodUs - floorUs : 0;
}

uint64_t VrrTimingController::latchedPresentationHeadroomUs() const
{
    return std::max(kLatchEnterHeadroomUs,
                    m_DisplayPeriodUs * kLatchEnterHeadroomPeriods);
}

uint64_t VrrTimingController::latchedPresentationExitHeadroomUs() const
{
    return std::max(kLatchExitHeadroomUs,
                    m_DisplayPeriodUs * kLatchExitHeadroomPeriodNumerator /
                        kLatchExitHeadroomPeriodDenominator);
}

uint64_t VrrTimingController::displayPeriodUs() const
{
    return m_DisplayPeriodUs;
}

uint64_t VrrTimingController::earliestSubmissionUs() const
{
    if (!m_HaveLastSubmission) {
        return 0;
    }
    return saturatingAdd(
        m_LastSubmissionUs,
        saturatingAdd(m_DisplayPeriodUs, m_GuardUs));
}

uint64_t VrrTimingController::lastSubmissionUs() const
{
    return m_LastSubmissionUs;
}

bool VrrTimingController::hasLastSubmission() const
{
    return m_HaveLastSubmission;
}

uint64_t VrrTimingController::renderLeadFloorUs() const
{
    return std::min(kRenderLeadFloorUs, m_SourcePeriodUs);
}

uint64_t VrrTimingController::renderLeadCeilingUs() const
{
    const uint64_t ceilingUs = std::min(kRenderLeadCeilingUs,
                                        m_SourcePeriodUs);
    return std::max(renderLeadFloorUs(), ceilingUs);
}

uint64_t VrrTimingController::readinessCeilingUs() const
{
    return std::min(kReadinessCeilingUs, m_SourcePeriodUs);
}

uint64_t VrrTimingController::guardCeilingUs() const
{
    const uint64_t sourceSlackUs = m_SourcePeriodUs > m_DisplayPeriodUs ?
        m_SourcePeriodUs - m_DisplayPeriodUs : 0;
    return std::max(m_BaseGuardUs,
                    std::min(kMaximumAdaptiveGuardUs, sourceSlackUs));
}

uint64_t VrrTimingController::periodForRate(int rateHz, uint64_t fallbackUs)
{
    if (rateHz <= 0) {
        return fallbackUs;
    }
    const uint64_t rate = static_cast<uint64_t>(rateHz);
    return std::max<uint64_t>(1,
        (kMicrosecondsPerSecond + rate / 2) / rate);
}

uint64_t VrrTimingController::periodForRateQ16(int rateHz,
                                               uint64_t fallbackQ16)
{
    if (rateHz <= 0) {
        return fallbackQ16;
    }
    const uint64_t rate = static_cast<uint64_t>(rateHz);
    constexpr uint64_t kPeriodQ16Scale =
        kMicrosecondsPerSecond * kQ16One;
    return std::max<uint64_t>(1,
        (kPeriodQ16Scale + rate / 2) / rate);
}

uint64_t VrrTimingController::saturatingAdd(uint64_t left, uint64_t right)
{
    const uint64_t maximum = std::numeric_limits<uint64_t>::max();
    return left > maximum - right ? maximum : left + right;
}

uint64_t VrrTimingController::addSigned(uint64_t value, int64_t adjustment)
{
    if (adjustment >= 0) {
        return saturatingAdd(value, static_cast<uint64_t>(adjustment));
    }
    const uint64_t magnitude =
        static_cast<uint64_t>(-(adjustment + 1)) + 1;
    return value > magnitude ? value - magnitude : 0;
}

int64_t VrrTimingController::signedDifference(uint64_t left, uint64_t right)
{
    if (left >= right) {
        const uint64_t difference = left - right;
        return difference > static_cast<uint64_t>(
                   std::numeric_limits<int64_t>::max()) ?
            std::numeric_limits<int64_t>::max() :
            static_cast<int64_t>(difference);
    }

    const uint64_t difference = right - left;
    if (difference > static_cast<uint64_t>(
                         std::numeric_limits<int64_t>::max())) {
        return std::numeric_limits<int64_t>::min();
    }
    return -static_cast<int64_t>(difference);
}

uint64_t VrrTimingController::roundedQ16(uint64_t valueQ16)
{
    return saturatingAdd(valueQ16, kQ16Half) / kQ16One;
}
