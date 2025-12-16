# VC4 Driver HDR Limitation on Raspberry Pi 5

## Problem Summary

HDR colors appear **washed out** on Raspberry Pi 5 (Broadcom VC4 driver) but work **perfectly** on Intel NUC (i915 driver) using the **same moonlight-qt build**.

## Root Cause

The Broadcom VC4 DRM driver does **not support setting the `COLOR_ENCODING` plane property via the legacy DRM API** (`drmModeSetPlane` + `drmModeObjectSetProperty`).

### Evidence

**During active streaming:**
- Our code logs: `COLOR_ENCODING: ITU-R BT.2020 YCbCr` ✅
- modetest shows: `COLOR_ENCODING: value: 1` (BT.709) ❌

The property is set in software but **not committed to hardware** because:
- Legacy API (`drmModeSetPlane`) doesn't persist plane properties on VC4
- Intel i915 driver tolerates this and keeps the property
- VC4 driver resets back to BT.709 default

### Why This Causes Washed Out Colors

- Video frames are **BT.2020 wide-gamut** 10-bit HDR content
- VC4 interprets them as **BT.709 narrow-gamut** due to wrong COLOR_ENCODING
- Wide colors compressed into narrow space → washed out appearance
- This is why NUC looks "vibrant" and RPI5 looks "washed out"

## What Works

✅ **HDR metadata blob** - Correctly set (400 nits, BT.2020 primaries, PQ curve)
✅ **Connector Colorspace** - Set to BT.2020 YCbCr
✅ **COLOR_RANGE** - Limited range (correct for video)
✅ **Intel i915 driver** - Everything works perfectly

## What Doesn't Work

❌ **VC4 COLOR_ENCODING** - Stuck at BT.709, should be BT.2020
❌ **Legacy DRM API workarounds** - Can't fix this without atomic API
❌ **drmModeObjectSetProperty** - Returns success but doesn't commit on VC4

## Solution Required

**Migrate to DRM Atomic Modesetting API**

### Current Code (Legacy API - Broken on VC4)
```cpp
// Set property (not committed on VC4)
drmModeObjectSetProperty(fd, planeId, DRM_MODE_OBJECT_PLANE,
                         colorEncodingProp, BT2020_VALUE);

// Update plane (resets COLOR_ENCODING to BT.709 on VC4)
drmModeSetPlane(fd, planeId, crtcId, fbId, ...);
```

### Required Code (Atomic API - Works on VC4)
```cpp
drmModeAtomicReq *req = drmModeAtomicAlloc();

// Add all properties to atomic request
drmModeAtomicAddProperty(req, planeId, fbIdPropId, fbId);
drmModeAtomicAddProperty(req, planeId, crtcIdPropId, crtcId);
drmModeAtomicAddProperty(req, planeId, colorEncodingPropId, BT2020_VALUE);
drmModeAtomicAddProperty(req, planeId, colorRangePropId, LIMITED_RANGE);
// ... all other plane properties ...

// Commit everything atomically
drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_NONBLOCK, NULL);
drmModeAtomicFree(req);
```

## References

- **moonlight-embedded**: Has working atomic modesetting implementation for VC4
- **Kodi (WinSystemGbm.cpp)**: Uses atomic modesetting for HDR
- **Gamescope**: Uses atomic modesetting

## Workarounds Attempted (All Failed)

1. ❌ Setting COLOR_ENCODING before drmModeSetPlane
2. ❌ Setting COLOR_ENCODING after drmModeSetPlane
3. ❌ Changing connector Colorspace from RGB to YCC
4. ❌ Re-applying property every frame

**None work because the fundamental issue is the legacy API limitation in VC4.**

## Current Status

- **Intel NUC**: Perfect HDR with current code ✅
- **Raspberry Pi 5**: Washed out colors until atomic API is implemented ❌
- **HDR metadata**: Working correctly on both platforms ✅
- **Everything except COLOR_ENCODING**: Working on both platforms ✅

## Path Forward

1. Study moonlight-embedded's atomic implementation
2. Refactor DrmRenderer::renderFrame() to use atomic API
3. Keep legacy API as fallback for older drivers
4. Test on both Intel i915 and Broadcom VC4

**Estimated effort**: Moderate - requires careful refactoring of plane management code

---

*Last updated: 2025-12-15*
*Issue discovered through side-by-side comparison: Same build, same TV, different result*
