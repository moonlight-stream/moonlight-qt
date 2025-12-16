# HDR Implementation Notes for Moonlight-Qt on Raspberry Pi

## TL;DR - Key Decisions

**The Core Fix**: The washed-out colors on Raspberry Pi 5 were caused by two bugs:
1. **Wrong colorspace**: BT2020_RGB connector colorspace with YCbCr video frames
2. **COLOR_ENCODING not applied**: VC4 driver doesn't support setting COLOR_ENCODING via legacy DRM API

**The Solution**:
- **BT2020_YCC colorspace** - Matches actual P010 YCbCr video format
- **DRM atomic modesetting** - Properly applies COLOR_ENCODING property on VC4
- **Passthrough HDR metadata from Sunshine** - Describes the source display's mastering environment

**What the metadata means**:
- HDR mastering metadata describes **where the content was created** (the source PC's monitor)
- This is NOT describing the client TV's capabilities
- The client TV uses this to understand the brightness range of the incoming signal
- The TV then tone-maps the signal to its own capabilities

---

## Current Implementation Status

### What Works Now ✅
- HDR mode activates properly on the TV
- Vibrant, accurate BT.2020 wide-gamut colors on both Intel and Raspberry Pi 5
- Using BT.2020 standard primaries in metadata blob
- Using BT2020_YCC colorspace (matches P010 YCbCr video format)
- Passthrough of source display mastering metadata from Sunshine
- Proper EOTF (SMPTE ST 2084 PQ curve)
- Correct color encoding (BT.2020 YCbCr) applied atomically on the plane
- Dynamic bit depth (8/10/12-bit) based on actual frame format
- Environment variable support for fine-tuning/overrides

### Current Configuration
**File:** `app/streaming/video/ffmpeg-renderers/drm.cpp`
**Functions:** `DrmRenderer::setHdrMode()`, `DrmRenderer::renderFrame()`

#### HDMI Colorspace
- Set to: `DRM_MODE_COLORIMETRY_BT2020_YCC` (value 10)
- **Critical**: Must match actual video format (P010 = YCbCr)
- Previous BT2020_RGB caused washed out colors on Raspberry Pi 5 (VC4 driver)
- Intel i915 driver tolerates RGB/YCbCr mismatch, VC4 does not
- YCbCr end-to-end ensures proper color reproduction across all drivers

#### HDR Metadata Blob
```cpp
// Primaries: BT.2020 standard (standard wide-gamut color space)
Red:   (0.7080, 0.2920)
Green: (0.1700, 0.7970)
Blue:  (0.1310, 0.0460)
White: (0.3127, 0.3290) D65

// Luminance - Passthrough from source display
// Priority: env vars > Sunshine metadata > 400 nit fallback
max_display_mastering_luminance: [from source or 400 nits default]
min_display_mastering_luminance: [from source or 0 nits default]
max_cll:  [from source or same as max_display_mastering_luminance]
max_fall: [from source or max_display_mastering_luminance / 2]
```

## Issues Fixed

### Problem 1: Washed Out Colors on Raspberry Pi 5 (VC4 Driver) - MAIN ISSUE
**Root Cause:** Two-part problem:

**Part A: Wrong Colorspace**
- Initial implementation used **BT2020_RGB** connector colorspace (to match Windows D3D11)
- Actual video frames are in **YCbCr format** (P010 = 10-bit 4:2:0 YCbCr)
- Intel i915 driver handles this mismatch gracefully with internal conversion
- Broadcom VC4 driver (RPI5) does NOT - requires exact format matching

**Part B: COLOR_ENCODING Not Applied (VC4 limitation)**
- VC4 DRM driver doesn't support setting COLOR_ENCODING via legacy DRM API
- `drmModeObjectSetProperty()` succeeds but `drmModeSetPlane()` resets it to BT.709
- Video frames are BT.2020 wide-gamut, hardware interprets as BT.709 narrow-gamut
- Wide colors compressed into narrow space → washed out appearance

**Testing Evidence (2025-12-15):**
- x86 Intel NUC (i915 driver) with legacy API: **HDR looks amazing, vibrant colors**
- Raspberry Pi 5 (VC4 driver) with legacy API: **Washed out, colors don't "pop"**
- Same LG UT7550AUA TV, same HDR metadata, different drivers = driver-specific issue
- Confirmed via modetest logs: Both set Colorspace correctly, but only i915 kept COLOR_ENCODING

**Solution:**
1. Changed connector colorspace to **BT2020_YCC** (matches P010 YCbCr format)
2. Implemented **DRM atomic modesetting** to properly apply COLOR_ENCODING on VC4
3. Atomic commits apply all properties (geometry + color) in one transaction
4. VC4 now correctly interprets frames as BT.2020 YCbCr

**Result:** Both Intel and Raspberry Pi 5 now show vibrant, accurate HDR colors

### Problem 2: Dynamic Bit Depth Support
**Root Cause:** Bit depth was hardcoded to 10 during initialization when VIDEO_FORMAT_MASK_10BIT was detected.

**Solution:**
- Store "max bpc" property pointer during init
- Dynamically set based on actual frame bit depth using `getFrameBitsPerChannel(frame)`
- Only triggered when frame format changes (zero steady-state performance impact)
- Supports 8-bit, 10-bit, 12-bit automatically

### Understanding HDR Mastering Metadata

**What it means:**
- HDR mastering metadata describes **the display environment where the content was created**
- For game streaming, this is the source PC's monitor
- Example: If the game was rendered on a 1000-nit monitor, metadata says "1000 nits"
- The client TV receives this and knows: "This signal was created for a 1000-nit display"

**What the TV does with it:**
1. Receives signal metadata: "This content was mastered on a 1000-nit display"
2. Knows its own capabilities: "I can only display 400 nits peak"
3. Applies tone mapping: "I need to compress 1000 nits → 400 nits"
4. Result: Bright highlights are preserved but compressed to TV's range

**Why passthrough is correct:**
- Games are rendered with the source display's brightness in mind
- The metadata accurately describes that rendering environment
- Each TV can then tone-map appropriately for its own capabilities
- This is the standard HDR workflow (same as Blu-ray discs)

**Current Implementation:**
```cpp
// Priority order:
1. MOONLIGHT_HDR_MAX_NITS environment variable (user override)
2. Sunshine metadata from source display (passthrough)
3. 400 nit fallback (if no source metadata available)
```

## Technical Details

### Color Pipeline
```
Sunshine (PC) → HEVC encode (YCbCr 4:2:0 10-bit, BT.2020)
    ↓
Network stream
    ↓
Moonlight decoder → YCbCr (P010 format, BT.2020)
    ↓
DRM KMS Plane (COLOR_ENCODING: BT.2020 YCbCr) ← Applied atomically
    ↓
Display Controller → YCbCr to RGB conversion (automatic)
    ↓
HDMI Output (Colorspace: BT2020_YCC) ← Tells TV format is YCbCr
    ↓
TV/Monitor → Receives BT.2020 YCbCr signal + HDR metadata
    ↓
TV Tone Mapping → Maps source mastering range to TV capabilities
```

### DRM Atomic Modesetting (The Fix for VC4)

**Why atomic modesetting is required:**
- Legacy API: `drmModeObjectSetProperty()` + `drmModeSetPlane()` = properties reset on VC4
- Atomic API: All properties committed in one transaction = guaranteed to apply
- VC4 driver only respects COLOR_ENCODING when set via atomic API

**Implementation:**
```cpp
// Create atomic request
drmModeAtomicReqPtr req = drmModeAtomicAlloc();

// Add ALL properties to the request
drmModeAtomicAddProperty(req, planeId, FB_ID, framebufferId);
drmModeAtomicAddProperty(req, planeId, CRTC_ID, crtcId);
// ... geometry properties ...
drmModeAtomicAddProperty(req, planeId, COLOR_ENCODING, BT2020_YCC); // ← This now works!
drmModeAtomicAddProperty(req, planeId, COLOR_RANGE, LIMITED);

// Commit everything atomically
drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
```

**Benefits:**
- Proper COLOR_ENCODING on VC4 driver (Raspberry Pi 5)
- Can set connector properties (Colorspace, HDR metadata) atomically with plane updates
- Eliminates race conditions and EBUSY errors
- Falls back to legacy API if atomic not supported

### Shared DRM Master with EGLFS

**The Challenge:**
- Qt EGLFS holds DRM master for the UI
- Video renderer also needs DRM access
- Both need to work without interfering

**The Solution:**
- Share the same DRM FD (captured via masterhook.c from Qt)
- Use `drmSetMaster()` to temporarily grab master before atomic commit
- Use `DRM_MODE_ATOMIC_ALLOW_MODESET` flag to prevent EBUSY when EGLFS reclaims
- Both Qt UI and video playback coexist peacefully

## Comparison with Other Implementations

### Kodi ✅ Matches Our Approach
- **Colorspace**: BT2020_YCC (same as us!)
- **Luminance**: Extracted from video file metadata (AVMasteringDisplayMetadata)
- **Atomic modesetting**: Uses atomic API for all DRM operations
- **Approach**: Passthrough metadata embedded in video files
- **Why YCC**: Video files are YCbCr format, connector colorspace matches content

### Gamescope (Valve)
- **Colorspace**: BT2020_RGB (Gamescope does RGB compositing, different use case)
- **Luminance**: App-provided metadata, reads from display EDID when available
- **Atomic modesetting**: Uses atomic API exclusively
- **Approach**: Compositor with HDR → HDR and SDR → HDR (inverse tone mapping)

### Windows Moonlight (D3D11)
- **Colorspace**: RGB (DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
- **Luminance**: Passes through source metadata
- **API**: D3D11 swap chain with HDR support
- **Why it works on Windows**: DXGI handles YCbCr→RGB conversion before output

### Why Our Approach Works for Game Streaming
- **Matches video format**: YCbCr video → YCC colorspace (critical for VC4)
- **Proper plane properties**: COLOR_ENCODING applied atomically (critical for VC4)
- **Standard metadata**: Passthrough source mastering info (standard HDR workflow)
- **Works on multiple drivers**: Intel i915 and Broadcom VC4 both supported

## Testing Notes

### TV: LG UT7550AUA

**Display Characteristics:**
- Budget HDR TV (not OLED) - reviewed as "not bright enough to do HDR well"
- Likely peak brightness around 400 nits (based on reviews and testing)

**EDID Color Primaries:**
- Red: (0.6396, 0.3300) - vs BT.2020 (0.7080, 0.2920) - ~10% smaller gamut
- Green: (0.2998, 0.5996) - vs BT.2020 (0.1700, 0.7970) - ~25% smaller gamut
- Blue: (0.1503, 0.0595) - vs BT.2020 (0.1310, 0.0460) - ~15% smaller gamut
- White: (0.3125, 0.3291) - vs BT.2020 (0.3127, 0.3290) - perfect D65 match
- Color gamut is significantly narrower than BT.2020 (typical for budget HDR TVs)

**EDID HDR Capabilities:**
- ✅ Supports BT.2020 YCbCr and BT.2020 RGB colorimetry
- ✅ Supports SMPTE ST2084 (PQ curve), HLG, and SDR gamma
- ✅ Supports Static Metadata Type 1
- ❌ No luminance values in EDID (no max/min nits, no max_cll/max_fall)

**Testing Results:**
- Works perfectly with BT2020_YCC colorspace + atomic modesetting
- Vibrant colors on both Intel NUC and Raspberry Pi 5
- HDR badge behavior varies (just indicates active tone mapping, not quality)
- Source metadata passthrough works correctly

### Raspberry Pi 5
- Broadcom VC4 display driver
- Direct DRM scanout (zero-copy)
- Hardware YCbCr→RGB conversion
- **Requires atomic modesetting for COLOR_ENCODING**
- Works perfectly with BT2020_YCC colorspace

### Intel NUC (Reference Platform)
- Intel i915 display driver
- Tolerates colorspace mismatches (but BT2020_YCC still better)
- Works with both legacy and atomic modesetting
- Perfect reference for "what HDR should look like"

## Environment Variables

### User Configuration
```bash
MOONLIGHT_HDR_MAX_NITS=1000     # Override source display mastering luminance
MOONLIGHT_HDR_MIN_NITS=0        # Override min luminance
MOONLIGHT_HDR_MAX_CLL=1000      # Override max content light level
MOONLIGHT_HDR_MAX_FALL=500      # Override max frame-average light level
MOONLIGHT_DISABLE_ATOMIC=1      # Force legacy API (for debugging)
```

### When to Use Overrides

**Normally:** Let Sunshine provide the source metadata (passthrough mode)

**Override when:**
- Testing different values to see TV behavior
- Source metadata is missing or incorrect
- Specific game calibration recommendations
- Debugging HDR issues

**Examples:**
```bash
# Let Sunshine provide metadata (default, recommended)
./app/moonlight

# Override if source reports unrealistic values
MOONLIGHT_HDR_MAX_NITS=400 ./app/moonlight

# Match specific game calibration
MOONLIGHT_HDR_MAX_NITS=360 ./app/moonlight  # Horizon Zero Dawn

# Fine-tune all parameters independently
MOONLIGHT_HDR_MAX_NITS=1000 MOONLIGHT_HDR_MAX_CLL=1500 ./app/moonlight
```

## Future Improvements

### 1. EDID-Based Fallback Values
**Concept:** When Sunshine doesn't provide metadata, read client TV's EDID instead of hardcoded 400 nits

**Implementation:**
- Use libdisplay-info to parse EDID
- Extract HDR static metadata from CTA-861 extensions
- Use as fallback only (Sunshine metadata still has priority)
- Validate values are sane

**Benefits:**
- Better fallback for displays that advertise capabilities
- Still respects source metadata when available

### 2. Per-Frame Dynamic Metadata
**Concept:** Update HDR metadata per-frame for better dynamic range

**Challenges:**
- Performance overhead of frequent blob updates
- Need to verify Sunshine sends per-frame metadata
- Most content uses static metadata anyway

### 3. Hardware Overlay Plane for Performance Stats
**Concept:** Use second DRM plane for compositing performance overlay

**Benefits:**
- Performance overlay would work in HDR mode
- Zero performance impact (hardware composition)
- Maintains HDR quality

**Implementation needed:**
- Find available overlay plane
- Render overlay to separate framebuffer
- Composite via DRM atomic commit

## References

- [DRM HDR Documentation](https://www.kernel.org/doc/html/latest/gpu/drm-kms.html#standard-connector-properties)
- [CTA-861 HDR Standard](https://en.wikipedia.org/wiki/HDMI#Enhanced_features)
- [Gamescope HDR Implementation](https://github.com/ValveSoftware/gamescope)
- [Kodi GBM Implementation](https://github.com/xbmc/xbmc)

## Quick Reference

### For Users
```bash
# Default (recommended - uses source metadata)
./app/moonlight

# Override if needed
MOONLIGHT_HDR_MAX_NITS=400 ./app/moonlight
```

### For Developers
- **File**: [drm.cpp](app/streaming/video/ffmpeg-renderers/drm.cpp)
- **HDR Setup**: `DrmRenderer::setHdrMode()` at line ~908
- **Metadata Logic**: Lines 1022-1068 (passthrough with fallback)
- **Atomic Rendering**: `DrmRenderer::renderFrame()` at line ~1447

### Verifying It Works
```bash
# During active streaming, check DRM state
sudo modetest -M vc4 -p

# Look for:
# - Plane COLOR_ENCODING: value 2 (BT.2020 YCbCr) ✅
# - Connector Colorspace: value 10 (BT2020_YCC) ✅
# - HDR_OUTPUT_METADATA: blob with your metadata ✅
```

## Contributors

- HDR atomic modesetting implementation: 2025-12-15 debugging session
- Analysis based on Gamescope, Kodi, and Windows implementations
- EDID analysis: LG UT7550AUA (2024 model, 86" 4K budget HDR TV)
- Testing platforms: Intel NUC (i915) and Raspberry Pi 5 (VC4)
