#include "streaming/audio/dualsensehapticscalibration.h"
#include "streaming/audio/dualsensehapticsrouting.h"
#include "streaming/audio/dualsensehapticsstream.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

int main()
{
    LI_DS5_HAPTICS_IR_FRAME_V2 frame = {};
    frame.lanes[0].rmsAmplitude = 0.5f;
    frame.lanes[0].lowBandRatio = 1.0f;
    frame.lanes[1].rmsAmplitude = 0.4f;
    frame.lanes[1].transientStrength = 1.0f;

    const auto active = dualsense_haptics::renderIrV2(frame);
    CHECK(active.lowFrequency > 0);
    CHECK(active.highFrequency > 0);

    const auto native = dualsense_haptics::renderIrV2Native(frame);
    CHECK(native.left.intensity > 0.0f);
    CHECK(native.right.intensity > 0.0f);
    CHECK(native.left.sharpness < native.right.sharpness);
    CHECK(native.left.intensity <= 1.0f);
    CHECK(native.right.intensity <= 1.0f);

    frame.flags = LI_DS5_HAPTICS_IR_FLAG_STREAM_END;
    const auto ended = dualsense_haptics::renderIrV2(frame);
    CHECK(ended.lowFrequency == 0);
    CHECK(ended.highFrequency == 0);
    const auto nativeEnded = dualsense_haptics::renderIrV2Native(frame);
    CHECK(nativeEnded.left.intensity == 0.0f);
    CHECK(nativeEnded.right.intensity == 0.0f);

    frame.flags = LI_DS5_HAPTICS_IR_FLAG_SILENT;
    const auto silent = dualsense_haptics::renderIrV2(frame);
    CHECK(silent.lowFrequency == 0);
    CHECK(silent.highFrequency == 0);
    const auto nativeSilent = dualsense_haptics::renderIrV2Native(frame);
    CHECK(nativeSilent.left.intensity == 0.0f);
    CHECK(nativeSilent.right.intensity == 0.0f);

    using Candidate = dualsense_haptics::LocalControllerCandidate;
    const Candidate singleDualSense[] = {{0, true}};
    CHECK(dualsense_haptics::selectUniqueLocalDualSense(singleDualSense, 1, true) == 0);
    CHECK(dualsense_haptics::selectUniqueLocalDualSense(singleDualSense, 1, false) == 0);

    const Candidate mixedControllers[] = {{0, false}, {1, true}};
    CHECK(dualsense_haptics::selectUniqueLocalDualSense(mixedControllers, 2, true) == 1);
    const Candidate mergedControllers[] = {{0, false}, {0, true}};
    CHECK(dualsense_haptics::selectUniqueLocalDualSense(mergedControllers, 2, false) == -1);

    const Candidate twoDualSense[] = {{0, true}, {1, true}};
    CHECK(dualsense_haptics::selectUniqueLocalDualSense(twoDualSense, 2, true) == -1);
    CHECK(dualsense_haptics::selectUniqueLocalDualSense(nullptr, 0, true) == -1);

    CHECK(!dualsense_haptics::canUseNativeController(0, 1, 1));
    CHECK(dualsense_haptics::canUseNativeController(1, 1, 1));
    CHECK(!dualsense_haptics::canUseNativeController(1, -1, 1));
    CHECK(!dualsense_haptics::canUseNativeController(1, 1, 0));
    CHECK(!dualsense_haptics::canUseNativeController(1, 1, 2));
    dualsense_haptics::IrBackendLatch backendLatch;
    CHECK(backendLatch.shouldAttemptNative(1, false));
    backendLatch.useFallback(1);
    CHECK(!backendLatch.shouldAttemptNative(1, false));
    CHECK(!backendLatch.shouldAttemptNative(1, false));
    backendLatch.useFallback(0);
    CHECK(!backendLatch.shouldAttemptNative(0, false));
    CHECK(!backendLatch.shouldAttemptNative(1, true));
    CHECK(backendLatch.shouldAttemptNative(1, false));
    CHECK(!backendLatch.shouldAttemptNative(0, false));
    CHECK(!backendLatch.shouldAttemptNative(0, true));
    CHECK(backendLatch.shouldAttemptNative(0, false));
    backendLatch.useFallback(0);
    backendLatch.useFallback(1);
    backendLatch.reset();
    CHECK(backendLatch.shouldAttemptNative(0, false));
    CHECK(backendLatch.shouldAttemptNative(1, false));

    using Action = dualsense_haptics::PcmStreamTracker::Action;
    dualsense_haptics::PcmStreamTracker tracker;
    CHECK(tracker.observe(LI_DS5_HAPTICS_PCM_FLAG_STREAM_START, 0, 10) ==
          Action::ResetAndAccept);
    CHECK(tracker.observe(0, 0, 11) == Action::Accept);

    // A second controller must not continually reset the endpoint owned by the
    // first controller, even when its stream starts while the first is active.
    CHECK(tracker.observe(0, 1, 20) == Action::Ignore);
    CHECK(tracker.observe(LI_DS5_HAPTICS_PCM_FLAG_STREAM_START, 1, 20) == Action::Ignore);
    CHECK(tracker.observe(LI_DS5_HAPTICS_PCM_FLAG_STREAM_END, 1, 21) == Action::Ignore);
    CHECK(tracker.observe(0, 0, 12) == Action::Accept);
    CHECK(tracker.observe(LI_DS5_HAPTICS_PCM_FLAG_STREAM_END, 0, 13) == Action::End);
    CHECK(tracker.observe(LI_DS5_HAPTICS_PCM_FLAG_STREAM_START, 1, 20) ==
          Action::ResetAndAccept);
    CHECK(tracker.observe(0, 0, 13) == Action::Ignore);
    CHECK(tracker.observe(LI_DS5_HAPTICS_PCM_FLAG_STREAM_END, 1, 21) == Action::End);

    // A long quiet period does not alter state: the next contiguous packet is
    // accepted without forcing another endpoint reset and prebuffer delay.
    CHECK(tracker.observe(LI_DS5_HAPTICS_PCM_FLAG_STREAM_START, 0, 100) ==
          Action::ResetAndAccept);
    CHECK(tracker.observe(0, 0, 101) == Action::Accept);

    // Missing packets still force the jitter buffer to resynchronize.
    CHECK(tracker.observe(0, 0, 103) == Action::ResetAndAccept);
    return 0;
}
