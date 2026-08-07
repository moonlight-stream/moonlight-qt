#pragma once

// VRR rate selection and stream/display qualification live in one small policy
// object shared by the settings UI, session startup, and Pacer. It has no
// dependency on SDL, QSettings, or a renderer.
class VrrRatePolicy
{
public:
    // floor(refresh - refresh^2 / 3600)
    static int vrrRateForRefresh(int refreshHz);

    // floor((refresh * 5 / 6) / 5) * 5
    static int lowLatencyRateForRefresh(int refreshHz);

    // Adaptive presentation needs enough time between stream frames for one
    // display period and the pacer's baseline safety guard. This is a
    // deterministic session qualification, not a per-frame latching policy.
    static bool hasAdaptiveHeadroom(int streamRateHz, int displayRefreshHz);
};
