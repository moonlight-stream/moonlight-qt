# HDR Implementation Notes for Moonlight-Qt on Raspberry Pi

## TL;DR - Key Decisions

**The Core Insight**: For game streaming, use **content-based metadata** (describes the video signal) instead of **display-based metadata** (describes the source monitor). This is different from traditional HDR video playback.

**Why 400 Nits?**
- Budget HDR TVs typically peak at 300-500 nits
- 400 nits avoids triggering aggressive tone mapping while ensuring proper 10-bit BT.2020 color
- Matches actual TV capabilities better than high values (1000+ nits cause blown highlights)
- User-configurable via `MOONLIGHT_HDR_MAX_NITS` environment variable

**Why Ignore Source Metadata?**
- Source = PC monitor or dummy plug (reports 9700 nits after calibration through streaming)
- Client = Raspberry Pi TV (actually ~400 nits peak brightness)
- Passing through source → TV tone-maps incorrectly → blown highlights or washed colors
- Fixed 400 nits → TV displays signal faithfully

**Implementation**: See [drm.cpp:888-956](app/streaming/video/ffmpeg-renderers/drm.cpp#L888-L956)

---

## Current Implementation Status

### What Works Now ✅
- HDR mode activates properly on the TV
- Using BT.2020 standard primaries (not source monitor's primaries)
- Using BT2020_RGB colorspace (matches Windows behavior)
- Using content-based luminance values (400 nits default, user-configurable)
- Proper EOTF (SMPTE ST 2084 PQ curve)
- Correct color encoding (BT.2020 YCbCr) on the plane
- Environment variable support for fine-tuning

### Current Configuration
**File:** `app/streaming/video/ffmpeg-renderers/drm.cpp`
**Function:** `DrmRenderer::setHdrMode()`

#### HDMI Colorspace
- Set to: `DRM_MODE_COLORIMETRY_BT2020_YCC` (value 10)
- **Critical**: Must match actual video format (P010 = YCbCr)
- Previous BT2020_RGB caused washed out colors on Raspberry Pi 5 (VC4 driver)
- Intel i915 driver tolerates RGB/YCbCr mismatch, VC4 does not
- YCbCr end-to-end ensures proper color reproduction across all drivers

#### HDR Metadata Blob
```cpp
// Primaries: BT.2020 standard (not display panel primaries)
Red:   (0.7080, 0.2920)
Green: (0.1700, 0.7970)
Blue:  (0.1310, 0.0460)
White: (0.3127, 0.3290) D65

// Luminance
max_display_mastering_luminance: 400 nits
min_display_mastering_luminance: 0 nits
max_cll:  400 nits
max_fall: 200 nits
```

## Issues Fixed

### Problem: Washed Out Colors with Passthrough
**Root Cause:** Original Moonlight passed through the source PC monitor's HDR metadata directly to the client display. This caused issues because:
- Source monitor metadata (351 nits, specific primaries) ≠ Client TV capabilities
- TV received conflicting information between metadata and actual content
- TV's tone mapper applied incorrect mapping

**Solution:** Use content-based metadata instead of display-based metadata:
- Use BT.2020 standard primaries (describes the video content)
- Use moderate luminance values (400 nits) that work with most displays
- Let each display's tone mapper handle its own capabilities

### Problem: Dummy Plug Reporting Unrealistic Values (9700 nits)
**Root Cause:** The Windows HDR calibration tool was run **through the streaming connection**, measuring the client TV instead of the dummy plug's actual capabilities. This created a circular dependency where:
- Dummy plug reports 9700 nits (based on client TV measurement)
- Moonlight receives 9700 nits and passes it to client TV
- Client TV tone-maps assuming content is 9700 nits bright
- Result: Blown out highlights

**Solution:** Ignore source display metadata entirely and use fixed content-based values:
- Always use 400 nits regardless of what Sunshine reports
- This avoids the circular dependency problem
- Works correctly whether source is a real monitor, dummy plug, virtual device, or misconfigured device

### Problem: High Luminance Values Trigger Aggressive Tone Mapping
**Root Cause:** Setting max_display_mastering_luminance to 1000+ nits causes budget HDR TVs to activate aggressive tone mapping. The "HDR badge" on these TVs just indicates tone mapping is active, not true wide-gamut HDR mode.

**Testing Results:**
- **1000-2000 nits**: HDR badge appears, but highlights blown out due to aggressive tone mapping
- **400 nits**: No HDR badge, but 10-bit BT.2020 signal displays correctly without crushing highlights

**Solution:** Use 400 nits to avoid triggering aggressive tone mapping while still delivering proper 10-bit BT.2020 color.

### Problem: Washed Out Colors on Raspberry Pi 5 (VC4 Driver)
**Root Cause:** Mismatch between connector colorspace and actual video format.
- Initial implementation used **BT2020_RGB** connector colorspace (to match Windows D3D11)
- Actual video frames are in **YCbCr format** (P010 = 10-bit 4:2:0 YCbCr)
- Intel i915 driver handles this mismatch gracefully with internal conversion
- Broadcom VC4 driver (RPI5) does NOT - causes washed out, non-vibrant colors

**Testing Evidence (2025-12-15):**
- x86 Intel NUC (i915 driver) with BT2020_RGB: **HDR looks amazing, vibrant colors**
- Raspberry Pi 5 (VC4 driver) with BT2020_RGB: **Washed out, colors don't "pop"**
- Same LG UT7550AUA TV, same HDR metadata, different drivers = driver-specific issue
- Confirmed via modetest logs: Both set Colorspace correctly, but VC4 doesn't handle RGB with YCbCr planes

**Solution:** Use **BT2020_YCC** connector colorspace to match actual video format.
- Connector colorspace must match plane format for proper hardware conversion
- YCbCr video frames (P010) → BT2020_YCC connector colorspace
- Lets the display controller/TV do proper BT.2020 YCbCr handling
- Works correctly across both Intel and Broadcom drivers

### Why 400 Nits Is The Right Default

**Industry Context:**
- Most budget HDR TVs peak at 300-500 nits (confirmed by reviews and EDID analysis)
- Professional HDR content is often mastered at 400-1000 nits
- Gamescope (Valve) and Kodi use similar moderate values when EDID has no luminance data
- The key insight: **match content metadata to the display's actual capabilities, not aspirational spec numbers**

**What Happens with Different Values:**
- **< 300 nits**: Some displays might not trigger HDR mode at all
- **300-500 nits**: Sweet spot for budget HDR TVs - proper color, minimal aggressive tone mapping
- **1000+ nits**: Triggers aggressive tone mapping on budget TVs → blown highlights
- **2000+ nits**: Designed for OLED/high-end displays, causes severe issues on budget TVs

**Why Not Use Higher Values?**
The mistake is thinking "higher nits = better HDR". In reality:
- Metadata describes what the *content* looks like, not what you want the display to do
- If you send 1000 nit metadata to a 400 nit TV, the TV thinks the content is 2.5x brighter than it can display
- This causes the TV to aggressively compress highlights, losing detail
- With 400 nit metadata matching the TV's capabilities, it displays the signal more faithfully

## Future Improvements

### 1. EDID-Based Dynamic Metadata (Gamescope Approach)

**What Gamescope Does:**
- Uses libdisplay-info to parse EDID
- Extracts display primaries from chromaticity coordinates
- Reads HDR static metadata from CTA-861 extensions
- Uses display's max_fall for default max_cll
- Allows per-frame metadata override from apps

**Implementation Plan:**
```cpp
// Add libdisplay-info dependency
#include <libdisplay-info/edid.h>
#include <libdisplay-info/cta.h>

// In DrmRenderer::initialize()
// 1. Read EDID from connector property
// 2. Parse with libdisplay-info
// 3. Extract HDR capabilities
// 4. Use as defaults, with fallback to current values
```

**Benefits:**
- Accurate primaries for calibrated displays (Steam Deck OLED)
- Display-specific luminance ranges
- Better tone mapping on capable displays

**Fallback Strategy:**
- If EDID has no HDR info: use current BT.2020 defaults
- If EDID has partial info: mix EDID + defaults
- Always validate values are sane

### 2. Per-Frame Dynamic Metadata

**Concept:** Extract HDR metadata from video stream (if Sunshine provides it) and update blob per-frame for better dynamic range.

**Challenges:**
- Performance overhead of updating blob frequently
- Need to validate Sunshine actually sends per-frame metadata
- Fallback needed when no per-frame data

### 3. User-Configurable Values ✅ IMPLEMENTED

**Environment Variables:**
```bash
MOONLIGHT_HDR_MAX_NITS=400      # max_display_mastering_luminance (default: 400)
MOONLIGHT_HDR_MIN_NITS=0        # min_display_mastering_luminance (default: 0)
MOONLIGHT_HDR_MAX_CLL=400       # max_cll (default: same as MAX_NITS)
MOONLIGHT_HDR_MAX_FALL=200      # max_fall (default: MAX_NITS/2)
```

**Usage Examples:**
```bash
# Match Horizon Zero Dawn's calibration exactly
MOONLIGHT_HDR_MAX_NITS=360 ./app/moonlight

# Try lower value to avoid badge entirely
MOONLIGHT_HDR_MAX_NITS=300 ./app/moonlight

# Try higher value for brighter displays
MOONLIGHT_HDR_MAX_NITS=600 ./app/moonlight

# Fine-tune all values independently
MOONLIGHT_HDR_MAX_NITS=400 MOONLIGHT_HDR_MAX_CLL=500 MOONLIGHT_HDR_MAX_FALL=250 ./app/moonlight
```

**Benefits:**
- Users can tune for their specific display
- Easy A/B testing without recompilation
- Game-specific values (e.g., match in-game calibration)
- Works with shell scripts and launchers

## Technical Details

### Color Pipeline
```
Sunshine (PC) → HEVC encode (YCbCr 4:2:0 10-bit)
    ↓
Network stream
    ↓
Moonlight decoder → YCbCr (P010 format)
    ↓
DRM KMS Plane (COLOR_ENCODING: BT.2020 YCbCr)
    ↓
Display Controller → YCbCr to RGB conversion (automatic)
    ↓
HDMI Output (Colorspace property: BT2020_RGB)
    ↓
TV/Monitor → HDR tone mapping based on metadata blob
```

### Why RGB Colorspace for YCbCr Content?
The `Colorspace` property tells the display what format the **HDMI signal** is in, not what format the **video plane** uses. The display controller automatically converts YCbCr→RGB before HDMI output when `Colorspace` is set to RGB.

This matches Windows behavior where D3D11 converts to RGB before swapchain output.

## Comparison with Other Implementations

### Kodi ✅ Matches Our Colorspace Approach
- **Colorspace**: BT2020_YCC (same as us now!)
- **Luminance**: Extracted directly from video file metadata (AVMasteringDisplayMetadata, AVContentLightMetadata)
- **Approach**: Uses per-content metadata embedded in video files by FFmpeg
- **No Fallback**: If video has no HDR metadata, Kodi doesn't send any - only works for properly mastered HDR content
- **Note**: Kodi is for local media playback where professional content has reliable embedded metadata
- **Why YCC**: Video files are YCbCr format, so connector colorspace matches content format

### Gamescope (Valve) ✅ Matches Our Luminance Approach
- **Colorspace**: BT2020_RGB (Gamescope does compositing/RGB rendering, different from our use case)
- **Luminance**: **Default: 400 nits for SDR content** (`--hdr-sdr-content-nits`, exactly matches our default!)
- **ITM (Inverse Tone Mapping)**: Default 100 nits input → 1000 nits target for SDR→HDR conversion
- **Approach**: Reads from display EDID when available, uses app-provided metadata with validation
- **Key Insight**: Valve uses 400 nits as the baseline for SDR content brightness in HDR mode
- **Our Implementation**: We use the same 400 nit baseline, but for game streaming instead of compositing
- **Colorspace Difference**: Gamescope renders to RGB framebuffers (compositor), we pass through YCbCr video

### Windows Moonlight (D3D11) ❌ The Problem We Fixed
- **Colorspace**: RGB (DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
- **Luminance**: Passes through source metadata directly
- **Issue**: This is what caused our original washed-out/blown-out problems
- **Why It's Wrong**: Source display (dummy plug: 9700 nits) ≠ client display (TV: ~400 nits)

### Why Our Approach Works Better for Game Streaming
Traditional HDR implementations (Kodi, video players) can trust content metadata because:
- Content is professionally mastered with known luminance values
- The metadata describes the content accurately

Game streaming is different:
- "Source" metadata describes the PC's monitor, not the game content
- Dummy plugs report nonsensical values (9700 nits from calibration through streaming)
- Game engines generate content dynamically with varying brightness
- **Solution**: Ignore source entirely, use fixed content-based values matching typical display capabilities

## Testing Notes

### TV: LG UT7550AUA

**Display Characteristics:**
- Budget HDR TV (not OLED) - reviewed as "not bright enough to do HDR well"
- Likely peak brightness around 400 nits (based on reviews and testing)
- This explains why 400 nit metadata works perfectly - matches TV's actual capabilities

**EDID Color Primaries:**
- Red: (0.6396, 0.3300) - vs BT.2020 (0.7080, 0.2920) - ~10% smaller
- Green: (0.2998, 0.5996) - vs BT.2020 (0.1700, 0.7970) - ~25% smaller
- Blue: (0.1503, 0.0595) - vs BT.2020 (0.1310, 0.0460) - ~15% smaller
- White: (0.3125, 0.3291) - vs BT.2020 (0.3127, 0.3290) - perfect D65 match
- Color gamut is significantly narrower than BT.2020, typical for budget HDR TVs

**EDID HDR Capabilities:**
- ✅ Supports BT.2020 YCbCr and BT.2020 RGB colorimetry
- ✅ Supports SMPTE ST2084 (PQ curve), HLG, and SDR gamma
- ✅ Supports Static Metadata Type 1
- ❌ No luminance values in EDID (no max/min nits, no max_cll/max_fall)

**Testing Results:**
- Works well with 400 nit defaults (matches TV's actual peak brightness)
- Shadow boost in-game settings helps with tone mapping
- HDR badge behavior is inconsistent, and doesnt make much of a difference.
- Badge just indicates tone mapping activity beyond a threshold, not HDR on/off

### Raspberry Pi 5
- V3D display driver
- Direct DRM scanout (zero-copy)
- Hardware YCbCr→RGB conversion
- Works with both RGB and YCC colorspace

### Testing Different Values

**Quick Test Script:**
```bash
# Test with default 400 nits
./test-hdr.sh

# Match Horizon Zero Dawn's calibration (360 nits)
./test-hdr.sh 360

# Try lower to avoid badge entirely
./test-hdr.sh 300

# Try higher for brighter displays
./test-hdr.sh 600
```

**Manual Testing:**
```bash
# Test specific values
MOONLIGHT_HDR_MAX_NITS=360 ./app/moonlight

# Fine-tune all parameters
MOONLIGHT_HDR_MAX_NITS=400 MOONLIGHT_HDR_MAX_CLL=500 MOONLIGHT_HDR_MAX_FALL=250 ./app/moonlight
```

**What to Look For:**
- Blown out highlights (too high)
- Washed out colors (too low)
- Inconsistent badge behavior (normal on budget TVs)
- In-game calibration recommendations

## References

- [DRM HDR Documentation](https://www.kernel.org/doc/html/latest/gpu/drm-kms.html#standard-connector-properties)
- [CTA-861 HDR Standard](https://en.wikipedia.org/wiki/HDMI#Enhanced_features)
- [Gamescope HDR Implementation](https://github.com/ValveSoftware/gamescope)
- [libdisplay-info](https://gitlab.freedesktop.org/emersion/libdisplay-info)

## Quick Reference

### For Users
```bash
# Default (works for most budget HDR TVs)
./app/moonlight

# Match your specific TV if you know its peak brightness
MOONLIGHT_HDR_MAX_NITS=360 ./app/moonlight  # Horizon calibration value
MOONLIGHT_HDR_MAX_NITS=500 ./app/moonlight  # Slightly brighter TV

# Use the test script for easy experimentation
./test-hdr.sh 360
```

### For Developers
- **File**: [drm.cpp](app/streaming/video/ffmpeg-renderers/drm.cpp)
- **Function**: `DrmRenderer::setHdrMode(bool enabled)` at line 811
- **Key Section**: Lines 888-956 (content-based metadata logic)
- **Environment Variables**: Lines 912-918 (getenv calls)

### Understanding Your Display
```bash
# Read your TV's EDID
sudo cat /sys/class/drm/card1/card1-HDMI-A-1/edid | edid-decode

# Look for:
# - Color primaries (how wide is the color gamut?)
# - HDR Static Metadata block (does it have luminance values?)
# - Supported EOTFs (ST2084 = HDR10, HLG = broadcast HDR)
```

## Contributors

- Initial HDR implementation: Moonlight-qt contributors
- Clientside metadata fix: Claude Code debugging session (2025-12-15)
- Analysis based on Gamescope, Kodi, and Windows implementations
- EDID analysis and documentation: LG UT7550AUA (2024 model, 86" 4K budget HDR TV)
