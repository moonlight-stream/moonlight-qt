#pragma once

#include <cstdint>

struct AVFrame;

// Renderer-facing VRR contract. Pacing timestamps and sender metadata never
// cross this boundary: the presenter only prepares, adaptively presents, or
// abandons a decoded image.
enum class VrrFallbackReason : uint8_t {
    NoFallback,
    IneffectiveVsync,
    InvalidRefresh,
    UnsupportedRenderer,
    MainThreadRenderer,
    WindowsVulkan,
    AdaptivePresentationUnavailable,
    InsufficientHeadroom,
    InitializationFailed,
};

inline const char* vrrFallbackReasonName(VrrFallbackReason reason)
{
    switch (reason) {
    case VrrFallbackReason::NoFallback:
        return "none";
    case VrrFallbackReason::IneffectiveVsync:
        return "ineffective V-sync";
    case VrrFallbackReason::InvalidRefresh:
        return "invalid display refresh";
    case VrrFallbackReason::UnsupportedRenderer:
        return "unsupported renderer";
    case VrrFallbackReason::MainThreadRenderer:
        return "main-thread renderer";
    case VrrFallbackReason::WindowsVulkan:
        return "Windows Vulkan is not supported";
    case VrrFallbackReason::AdaptivePresentationUnavailable:
        return "adaptive presentation is unavailable";
    case VrrFallbackReason::InsufficientHeadroom:
        return "stream rate leaves insufficient adaptive-refresh headroom";
    case VrrFallbackReason::InitializationFailed:
        return "VRR initialization failed";
    }

    return "unknown fallback";
}

// presented means the native API accepted a display transition. cancelled
// describes why the operation ended, not whether the platform submitted: some
// APIs must submit an acquired image even while abandoning it.
struct VrrPresentFeedback {
    bool presented = false;
    bool cancelled = false;
    // Set when the backend knows the exact instant of its native submission.
    // The worker anchors its display-spacing floor there instead of at its own
    // call boundary, which may include a blocking return.
    bool submissionTimeValid = false;
    uint64_t submissionTimeUs = 0;
};

// Per-present request from the platform-neutral controller. When the learned
// source cadence leaves too little adaptive-refresh headroom for safe
// immediate flips, the controller asks for a latched (non-tearing) present:
// at near-refresh rates the cadence cost of latching is a few repeated frames
// per second while immediate flips tear. Backends whose presentation mode is
// immutable after swapchain creation may ignore the preference.
struct VrrPresentRequest {
    bool latchedPresentation = false;
};

struct VrrPrepareResult {
    bool prepared = false;
    // Some acquired images (notably Vulkan swapchain frames) can only be
    // abandoned by submitting them. The worker owns any required wait.
    bool cancellationMaySubmit = false;
    VrrPresentFeedback feedback;
};

class IVrrFramePresenter {
public:
    virtual ~IVrrFramePresenter() = default;

    // Some adaptive backends can select a fixed-vsync latch for an individual
    // present. Vulkan WSI modes are immutable for the swapchain lifetime, so
    // cadence-following Mailbox/Immediate implementations leave this false.
    virtual bool canLatchAdaptivePresent() const
    {
        return false;
    }

    // Startup eligibility only. NoFallback means the presenter supports a worker-
    // thread split prepare/present path using its adaptive presentation mode.
    virtual VrrFallbackReason checkSupport() const = 0;

    // May acquire a swapchain image and submit rendering work, but must not
    // intentionally pace or wait for the worker's presentation target.
    virtual VrrPrepareResult prepareFrame(AVFrame* frame) = 0;

    // Presents the prepared image using the backend's adaptive presentation
    // path without intentionally waiting.
    virtual VrrPresentFeedback presentAdaptive(
        const VrrPresentRequest& request) = 0;

    // Releases a prepared image. The result must report a native submission
    // when the platform cannot abandon an acquired image without submitting.
    virtual VrrPresentFeedback cancelFrame()
    {
        VrrPresentFeedback feedback;
        feedback.cancelled = true;
        return feedback;
    }

    virtual void setSuspended(bool)
    {
    }

    // Called only when creating the VRR worker failed, before any frame was
    // handed to it or a legacy render worker was started.
    virtual bool restoreFixedPresentation(VrrFallbackReason)
    {
        return false;
    }
};
