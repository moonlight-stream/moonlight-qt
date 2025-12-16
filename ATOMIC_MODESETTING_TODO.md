# ✅ COMPLETED - Atomic Modesetting for VC4 HDR Fix

**Status**: Fully implemented in commit `9f626ea0` (2025-12-15)

**This document is now historical** - kept for reference to show the problem analysis and solution design.

---

## The Problem (SOLVED)

Raspberry Pi 5's Broadcom VC4 driver does not properly support setting `COLOR_ENCODING` via the legacy DRM API. This causes HDR colors to appear washed out because the hardware interprets BT.2020 wide-gamut content as BT.709 narrow-gamut.

## The Solution

Migrate from legacy DRM API to Atomic Modesetting API for plane updates.

## Current Code (Legacy API - Broken on VC4)

**File**: `app/streaming/video/ffmpeg-renderers/drm.cpp`
**Function**: `DrmRenderer::renderFrame()`
**Lines**: ~1350-1450

```cpp
// Set properties (doesn't stick on VC4)
drmModeObjectSetProperty(m_DrmFd, m_PlaneId, DRM_MODE_OBJECT_PLANE,
                         m_ColorRangeProp->prop_id, rangeValue);
drmModeObjectSetProperty(m_DrmFd, m_PlaneId, DRM_MODE_OBJECT_PLANE,
                         m_ColorEncodingProp->prop_id, encodingValue);

// Update plane (resets COLOR_ENCODING back to BT.709 on VC4!)
drmModeSetPlane(m_DrmFd, m_PlaneId, m_CrtcId, m_CurrentFbId, 0,
                dst.x, dst.y, dst.w, dst.h,
                0, 0, frame->width << 16, frame->height << 16);
```

## Required Code (Atomic API - Works on VC4)

```cpp
// Create atomic request
drmModeAtomicReqPtr req = drmModeAtomicAlloc();
if (!req) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate atomic request");
    return;
}

// Add all plane properties atomically
drmModeAtomicAddProperty(req, m_PlaneId, m_FbIdProp->prop_id, m_CurrentFbId);
drmModeAtomicAddProperty(req, m_PlaneId, m_CrtcIdProp->prop_id, m_CrtcId);
drmModeAtomicAddProperty(req, m_PlaneId, m_SrcXProp->prop_id, 0);
drmModeAtomicAddProperty(req, m_PlaneId, m_SrcYProp->prop_id, 0);
drmModeAtomicAddProperty(req, m_PlaneId, m_SrcWProp->prop_id, frame->width << 16);
drmModeAtomicAddProperty(req, m_PlaneId, m_SrcHProp->prop_id, frame->height << 16);
drmModeAtomicAddProperty(req, m_PlaneId, m_CrtcXProp->prop_id, dst.x);
drmModeAtomicAddProperty(req, m_PlaneId, m_CrtcYProp->prop_id, dst.y);
drmModeAtomicAddProperty(req, m_PlaneId, m_CrtcWProp->prop_id, dst.w);
drmModeAtomicAddProperty(req, m_PlaneId, m_CrtcHProp->prop_id, dst.h);

// CRITICAL: Color properties that were being reset before
if (m_ColorEncodingProp) {
    drmModeAtomicAddProperty(req, m_PlaneId, m_ColorEncodingProp->prop_id, bt2020Value);
}
if (m_ColorRangeProp) {
    drmModeAtomicAddProperty(req, m_PlaneId, m_ColorRangeProp->prop_id, limitedRangeValue);
}

// Commit all changes atomically (this is the key difference!)
int ret = drmModeAtomicCommit(m_DrmFd, req, DRM_MODE_ATOMIC_NONBLOCK, NULL);
if (ret != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "drmModeAtomicCommit failed: %d", errno);
    // Fall back to legacy API?
}

drmModeAtomicFree(req);
```

## Implementation Steps

### 1. Add Property ID Members to DrmRenderer Class

**File**: `app/streaming/video/ffmpeg-renderers/drm.h`

Add to private members:
```cpp
// Atomic modesetting support
bool m_SupportsAtomic;
drmModePropertyPtr m_FbIdProp;
drmModePropertyPtr m_CrtcIdProp;
drmModePropertyPtr m_SrcXProp, m_SrcYProp, m_SrcWProp, m_SrcHProp;
drmModePropertyPtr m_CrtcXProp, m_CrtcYProp, m_CrtcWProp, m_CrtcHProp;
```

### 2. Detect Atomic Capability During Initialization

**File**: `app/streaming/video/ffmpeg-renderers/drm.cpp`
**Function**: `DrmRenderer::DrmRenderer()` or similar init function

```cpp
// Check if driver supports atomic modesetting
uint64_t hasDumbBuffer = 0, hasAtomic = 0;
if (drmGetCap(m_DrmFd, DRM_CAP_DUMB_BUFFER, &hasDumbBuffer) == 0 && hasDumbBuffer) {
    if (drmSetClientCap(m_DrmFd, DRM_CLIENT_CAP_ATOMIC, 1) == 0) {
        m_SupportsAtomic = true;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "DRM atomic modesetting supported and enabled");
    } else {
        m_SupportsAtomic = false;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "DRM atomic modesetting not supported, using legacy API (COLOR_ENCODING may not work on VC4)");
    }
}
```

### 3. Find All Required Property IDs

During plane setup, enumerate and cache all property IDs needed for atomic commits:

```cpp
drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(m_DrmFd, m_PlaneId, DRM_MODE_OBJECT_PLANE);
if (props) {
    for (uint32_t i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(m_DrmFd, props->props[i]);
        if (prop) {
            if (!strcmp(prop->name, "FB_ID")) m_FbIdProp = prop;
            else if (!strcmp(prop->name, "CRTC_ID")) m_CrtcIdProp = prop;
            else if (!strcmp(prop->name, "SRC_X")) m_SrcXProp = prop;
            else if (!strcmp(prop->name, "SRC_Y")) m_SrcYProp = prop;
            else if (!strcmp(prop->name, "SRC_W")) m_SrcWProp = prop;
            else if (!strcmp(prop->name, "SRC_H")) m_SrcHProp = prop;
            else if (!strcmp(prop->name, "CRTC_X")) m_CrtcXProp = prop;
            else if (!strcmp(prop->name, "CRTC_Y")) m_CrtcYProp = prop;
            else if (!strcmp(prop->name, "CRTC_W")) m_CrtcWProp = prop;
            else if (!strcmp(prop->name, "CRTC_H")) m_CrtcHProp = prop;
            // COLOR_ENCODING and COLOR_RANGE already cached
            else drmModeFreeProperty(prop);
        }
    }
    drmModeFreeObjectProperties(props);
}
```

### 4. Create Helper Function for Atomic Plane Update

```cpp
bool DrmRenderer::updatePlaneAtomic(AVFrame* frame, SDL_Rect& dst) {
    drmModeAtomicReqPtr req = drmModeAtomicAlloc();
    if (!req) return false;

    // Add framebuffer and geometry
    drmModeAtomicAddProperty(req, m_PlaneId, m_FbIdProp->prop_id, m_CurrentFbId);
    drmModeAtomicAddProperty(req, m_PlaneId, m_CrtcIdProp->prop_id, m_CrtcId);

    // Source rectangle (in 16.16 fixed point)
    drmModeAtomicAddProperty(req, m_PlaneId, m_SrcXProp->prop_id, 0);
    drmModeAtomicAddProperty(req, m_PlaneId, m_SrcYProp->prop_id, 0);
    drmModeAtomicAddProperty(req, m_PlaneId, m_SrcWProp->prop_id, frame->width << 16);
    drmModeAtomicAddProperty(req, m_PlaneId, m_SrcHProp->prop_id, frame->height << 16);

    // Destination rectangle
    drmModeAtomicAddProperty(req, m_PlaneId, m_CrtcXProp->prop_id, dst.x);
    drmModeAtomicAddProperty(req, m_PlaneId, m_CrtcYProp->prop_id, dst.y);
    drmModeAtomicAddProperty(req, m_PlaneId, m_CrtcWProp->prop_id, dst.w);
    drmModeAtomicAddProperty(req, m_PlaneId, m_CrtcHProp->prop_id, dst.h);

    // Color properties (THE FIX FOR VC4!)
    if (m_ColorEncodingProp) {
        const char* desiredEncoding = getDrmColorEncodingValue(frame);
        for (int i = 0; i < m_ColorEncodingProp->count_enums; i++) {
            if (!strcmp(desiredEncoding, m_ColorEncodingProp->enums[i].name)) {
                drmModeAtomicAddProperty(req, m_PlaneId, m_ColorEncodingProp->prop_id,
                                       m_ColorEncodingProp->enums[i].value);
                break;
            }
        }
    }

    if (m_ColorRangeProp) {
        const char* desiredRange = getDrmColorRangeValue(frame);
        for (int i = 0; i < m_ColorRangeProp->count_enums; i++) {
            if (!strcmp(desiredRange, m_ColorRangeProp->enums[i].name)) {
                drmModeAtomicAddProperty(req, m_PlaneId, m_ColorRangeProp->prop_id,
                                       m_ColorRangeProp->enums[i].value);
                break;
            }
        }
    }

    // Commit
    int ret = drmModeAtomicCommit(m_DrmFd, req, DRM_MODE_ATOMIC_NONBLOCK, NULL);
    drmModeAtomicFree(req);

    return (ret == 0);
}
```

### 5. Update renderFrame() to Use Atomic or Legacy

```cpp
void DrmRenderer::renderFrame(AVFrame* frame) {
    // ... existing framebuffer creation code ...

    // Update plane using atomic API if supported, otherwise fall back to legacy
    bool success;
    if (m_SupportsAtomic) {
        success = updatePlaneAtomic(frame, dst);
        if (!success) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                       "Atomic commit failed, falling back to legacy API");
            // Fall through to legacy path
        }
    }

    if (!m_SupportsAtomic || !success) {
        // Legacy path (current code)
        updatePlaneLegacy(frame, dst);
    }

    // ... cleanup code ...
}
```

## Testing Plan

1. **Build with atomic support**
2. **Test on Intel NUC (i915)** - Should still work perfectly (might use legacy or atomic)
3. **Test on RPI5 (VC4)** - Should now have vibrant colors!
4. **Verify with modetest during stream**: `COLOR_ENCODING: value: 2` (BT.2020)

## Reference Implementations

- **Kodi**: `xbmc/windowing/gbm/WinSystemGbm.cpp` - Full atomic implementation
- **Gamescope**: Uses atomic for all DRM operations
- **moonlight-embedded**: `src/video/rk.c` - Rockchip backend with atomic

## Estimated Effort

**Time**: 2-4 hours for someone familiar with the codebase
**Complexity**: Moderate - mostly mechanical work finding and caching property IDs
**Risk**: Low - can fall back to legacy API if atomic fails

---

*This is the proper fix for VC4 HDR washed-out colors on Raspberry Pi 5*
