#include "dualsensehapticsmac.h"

#include "dualsensehapticscalibration.h"
#include "dualsensehapticsrouting.h"

#include "SDL_compat.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

#import <CoreHaptics/CoreHaptics.h>
#import <GameController/GameController.h>

@interface MoonlightHapticInvalidationToken : NSObject
@property(atomic, assign, getter=isInvalidated) BOOL invalidated;
@end

@implementation MoonlightHapticInvalidationToken
@end

@interface MoonlightDualSenseHapticState : NSObject
@property(nonatomic, strong) GCController* controller;
@property(nonatomic, strong) CHHapticEngine* leftEngine;
@property(nonatomic, strong) CHHapticEngine* rightEngine;
@property(nonatomic, strong) id<CHHapticPatternPlayer> leftPlayer;
@property(nonatomic, strong) id<CHHapticPatternPlayer> rightPlayer;
@property(nonatomic, strong) MoonlightHapticInvalidationToken* invalidationToken;
@end

@implementation MoonlightDualSenseHapticState
- (void)dealloc
{
    [_controller release];
    [_leftEngine release];
    [_rightEngine release];
    [_leftPlayer release];
    [_rightPlayer release];
    [_invalidationToken release];
    [super dealloc];
}
@end

namespace {

bool isDualSenseController(GCController* controller)
{
    return controller != nil &&
           [controller.extendedGamepad isKindOfClass:[GCDualSenseGamepad class]];
}

struct DualSenseSelection
{
    GCController* controller;
    std::size_t count;
};

DualSenseSelection findDualSenseSelection()
{
    GCController* candidate = nil;
    std::size_t candidateCount = 0;
    for (GCController* controller in GCController.controllers) {
        if (!isDualSenseController(controller)) {
            continue;
        }
        candidate = controller;
        candidateCount++;
    }

    return {candidate, candidateCount};
}

id<CHHapticPatternPlayer> createPlayer(CHHapticEngine* engine, NSError** error)
{
    CHHapticEventParameter* intensity =
        [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity
                                                     value:1.0f];
    CHHapticEventParameter* sharpness =
        [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness
                                                     value:0.5f];
    CHHapticEvent* event =
        [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
                                     parameters:@[intensity, sharpness]
                                   relativeTime:0
                                       duration:GCHapticDurationInfinite];
    CHHapticDynamicParameter* initialIntensity =
        [[CHHapticDynamicParameter alloc]
            initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl
                           value:0.0f
                    relativeTime:0];
    CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[event]
                                                            parameters:@[initialIntensity]
                                                                 error:error];
    [intensity release];
    [sharpness release];
    [event release];
    [initialIntensity release];
    if (pattern == nil) {
        return nil;
    }

    id<CHHapticPatternPlayer> player = [engine createPlayerWithPattern:pattern error:error];
    [pattern release];
    if (player == nil || ![player startAtTime:CHHapticTimeImmediate error:error]) {
        return nil;
    }
    return player;
}

bool startHandle(GCDeviceHaptics* haptics, GCHapticsLocality locality,
                 MoonlightHapticInvalidationToken* invalidationToken,
                 CHHapticEngine** engineOut, id<CHHapticPatternPlayer>* playerOut)
{
    if (![haptics.supportedLocalities containsObject:locality]) {
        return false;
    }

    CHHapticEngine* engine = [haptics createEngineWithLocality:locality];
    NSError* error = nil;
    engine.playsHapticsOnly = YES;
    engine.stoppedHandler = ^(CHHapticEngineStoppedReason reason) {
        (void)reason;
        invalidationToken.invalidated = YES;
    };
    engine.resetHandler = ^{
        invalidationToken.invalidated = YES;
    };
    if (engine == nil || ![engine startAndReturnError:&error]) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,
                    "Unable to start macOS DualSense haptic engine: %s",
                    error.localizedDescription.UTF8String ?: "unknown error");
        return false;
    }

    id<CHHapticPatternPlayer> player = createPlayer(engine, &error);
    if (player == nil) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,
                    "Unable to create macOS DualSense haptic player: %s",
                    error.localizedDescription.UTF8String ?: "unknown error");
        [engine stopWithCompletionHandler:nil];
        return false;
    }

    *engineOut = engine;
    *playerOut = player;
    return true;
}

MoonlightDualSenseHapticState* createState(GCController* controller,
                                           std::uint16_t controllerNumber)
{
    GCDeviceHaptics* haptics = controller.haptics;
    if (haptics == nil) {
        return nil;
    }

    MoonlightDualSenseHapticState* state = [[MoonlightDualSenseHapticState alloc] init];
    state.controller = controller;
    MoonlightHapticInvalidationToken* invalidationToken =
        [[MoonlightHapticInvalidationToken alloc] init];
    state.invalidationToken = invalidationToken;
    [invalidationToken release];
    CHHapticEngine* leftEngine = nil;
    CHHapticEngine* rightEngine = nil;
    id<CHHapticPatternPlayer> leftPlayer = nil;
    id<CHHapticPatternPlayer> rightPlayer = nil;
    if (!startHandle(haptics, GCHapticsLocalityLeftHandle, state.invalidationToken,
                     &leftEngine, &leftPlayer)) {
        [state release];
        return nil;
    }
    if (!startHandle(haptics, GCHapticsLocalityRightHandle, state.invalidationToken,
                     &rightEngine, &rightPlayer)) {
        NSError* error = nil;
        [leftPlayer stopAtTime:CHHapticTimeImmediate error:&error];
        [leftEngine stopWithCompletionHandler:nil];
        [state release];
        return nil;
    }
    state.leftEngine = leftEngine;
    state.rightEngine = rightEngine;
    state.leftPlayer = leftPlayer;
    state.rightPlayer = rightPlayer;

    SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO,
                "macOS native DualSense haptics ready for player %d",
                static_cast<int>(controllerNumber));
    return state;
}

bool updatePlayer(id<CHHapticPatternPlayer> player,
                  const dualsense_haptics::NativeHapticLaneOutput& output,
                  NSError** error)
{
    CHHapticDynamicParameter* intensity =
        [[CHHapticDynamicParameter alloc]
            initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl
                           value:std::clamp(output.intensity, 0.0f, 1.0f)
                    relativeTime:0];
    CHHapticDynamicParameter* sharpness =
        [[CHHapticDynamicParameter alloc]
            initWithParameterID:CHHapticDynamicParameterIDHapticSharpnessControl
                           value:std::clamp(output.sharpness, 0.0f, 1.0f) - 0.5f
                    relativeTime:0];
    const bool updated = [player sendParameters:@[intensity, sharpness]
                                         atTime:CHHapticTimeImmediate
                                          error:error];
    [intensity release];
    [sharpness release];
    return updated;
}

void stopState(MoonlightDualSenseHapticState* state)
{
    state.leftEngine.stoppedHandler = ^(CHHapticEngineStoppedReason reason) {
        (void)reason;
    };
    state.leftEngine.resetHandler = ^{};
    state.rightEngine.stoppedHandler = ^(CHHapticEngineStoppedReason reason) {
        (void)reason;
    };
    state.rightEngine.resetHandler = ^{};

    NSError* error = nil;
    [state.leftPlayer stopAtTime:CHHapticTimeImmediate error:&error];
    error = nil;
    [state.rightPlayer stopAtTime:CHHapticTimeImmediate error:&error];
    [state.leftEngine stopWithCompletionHandler:nil];
    [state.rightEngine stopWithCompletionHandler:nil];
}

} // namespace

struct MacDualSenseHapticsRenderer::Impl
{
    using Clock = std::chrono::steady_clock;
    // IR arrives every 5 ms, so this tolerates packet loss while bounding an
    // infinite Core Haptics player if the final silent/end frame is dropped.
    static constexpr std::chrono::milliseconds StateLeaseTimeout{250};

    std::mutex mutex;
    std::condition_variable watchdogWake;
    // Conservative routing admits at most one native controller at a time.
    MoonlightDualSenseHapticState* state = nil;
    dualsense_haptics::IrBackendLatch backendLatch;
    int selectedLocalController = -1;
    int stateController = -1;
    std::optional<Clock::time_point> leaseDeadline;
    bool watchdogStopping = false;
    std::thread watchdog;

    Impl() :
        watchdog([this] { runWatchdog(); })
    {
    }

    ~Impl()
    {
        {
            std::lock_guard lock(mutex);
            watchdogStopping = true;
            watchdogWake.notify_one();
        }
        watchdog.join();

        @autoreleasepool {
            std::lock_guard lock(mutex);
            clearState(false);
        }
    }

    void clearState(bool latchFallback)
    {
        leaseDeadline.reset();
        if (state != nil) {
            if (latchFallback) {
                backendLatch.useFallback(static_cast<std::uint16_t>(stateController));
            }
            stopState(state);
            [state release];
            state = nil;
            stateController = -1;
        }
        watchdogWake.notify_one();
    }

    void runWatchdog()
    {
        std::unique_lock lock(mutex);
        while (!watchdogStopping) {
            if (!leaseDeadline.has_value()) {
                watchdogWake.wait(lock, [this] {
                    return watchdogStopping || leaseDeadline.has_value();
                });
                continue;
            }

            const auto observedDeadline = *leaseDeadline;
            if (watchdogWake.wait_until(lock, observedDeadline, [this, observedDeadline] {
                    return watchdogStopping || !leaseDeadline.has_value() ||
                           *leaseDeadline != observedDeadline;
                })) {
                continue;
            }

            if (!leaseDeadline.has_value() || Clock::now() < *leaseDeadline) {
                continue;
            }

            @autoreleasepool {
                SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,
                            "macOS native DualSense haptics timed out for controller %d",
                            stateController);
                clearState(false);
            }
        }
    }

    void setControllerTarget(int controllerNumber)
    {
        std::lock_guard lock(mutex);
        if (selectedLocalController == controllerNumber) {
            return;
        }
        clearState(true);
        selectedLocalController = controllerNumber;
    }

    void reset()
    {
        std::lock_guard lock(mutex);
        clearState(false);
        backendLatch.reset();
        selectedLocalController = -1;
    }

    bool submit(const LI_DS5_HAPTICS_IR_FRAME_V2& frame, bool& startedNative)
    {
        std::lock_guard lock(mutex);
        startedNative = false;

        const auto selection = findDualSenseSelection();
        if (state != nil) {
            const bool valid = stateController >= 0 &&
                !state.invalidationToken.isInvalidated &&
                dualsense_haptics::canUseNativeController(
                    static_cast<std::uint16_t>(stateController),
                    selectedLocalController,
                    selection.count) &&
                state.controller == selection.controller;
            if (!valid) {
                clearState(true);
            }
        }

        MoonlightDualSenseHapticState* frameState =
            stateController == frame.controllerNumber ? state : nil;

        const bool streamEnd = (frame.flags & LI_DS5_HAPTICS_IR_FLAG_STREAM_END) != 0;
        if (!backendLatch.shouldAttemptNative(frame.controllerNumber, streamEnd)) {
            if (frameState != nil) {
                clearState(false);
            }
            return false;
        }

        const bool silent = (frame.flags & LI_DS5_HAPTICS_IR_FLAG_SILENT) != 0;
        if (silent && frameState == nil) {
            return false;
        }

        GCController* controller = dualsense_haptics::canUseNativeController(
            frame.controllerNumber, selectedLocalController, selection.count) ?
                selection.controller : nil;

        bool createdState = false;
        if (frameState == nil) {
            if (controller == nil) {
                backendLatch.useFallback(frame.controllerNumber);
                return false;
            }
            frameState = createState(controller, frame.controllerNumber);
            if (frameState == nil) {
                backendLatch.useFallback(frame.controllerNumber);
                return false;
            }
            state = frameState;
            stateController = frame.controllerNumber;
            createdState = true;
        }

        const auto output = dualsense_haptics::renderIrV2Native(frame);
        NSError* error = nil;
        if (!updatePlayer(frameState.leftPlayer, output.left, &error) ||
            !updatePlayer(frameState.rightPlayer, output.right, &error)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,
                        "Unable to update macOS DualSense haptics: %s",
                        error.localizedDescription.UTF8String ?: "unknown error");
            clearState(true);
            return false;
        }
        leaseDeadline = Clock::now() + StateLeaseTimeout;
        watchdogWake.notify_one();
        startedNative = createdState;
        return true;
    }
};

MacDualSenseHapticsRenderer::MacDualSenseHapticsRenderer() :
    m_Impl(std::make_unique<Impl>())
{
}

MacDualSenseHapticsRenderer::~MacDualSenseHapticsRenderer() = default;

void MacDualSenseHapticsRenderer::setControllerTarget(int controllerNumber)
{
    @autoreleasepool {
        m_Impl->setControllerTarget(controllerNumber);
    }
}

void MacDualSenseHapticsRenderer::reset()
{
    @autoreleasepool {
        m_Impl->reset();
    }
}

bool MacDualSenseHapticsRenderer::submit(const LI_DS5_HAPTICS_IR_FRAME_V2& frame,
                                         bool& startedNative)
{
    @autoreleasepool {
        return m_Impl->submit(frame, startedNative);
    }
}
