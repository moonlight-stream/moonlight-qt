#include "streamutils.h"

#include <Qt>

#ifdef Q_OS_DARWIN
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef Q_OS_WINDOWS
#include <Windows.h>
#endif

#ifdef Q_OS_LINUX
#include <sys/auxv.h>

#ifndef HWCAP2_AES
#define HWCAP2_AES (1 << 0)
#endif
#endif

Uint32 StreamUtils::getPlatformWindowFlags()
{
#if defined(Q_OS_DARWIN)
    return SDL_WINDOW_METAL;
#elif defined(HAVE_LIBPLACEBO_VULKAN)
    // We'll fall back to GL if Vulkan fails
    return SDL_WINDOW_VULKAN;
#else
    return 0;
#endif
}

void StreamUtils::scaleSourceToDestinationSurface(SDL_Rect* src, SDL_Rect* dst)
{
    int dstH = SDL_ceilf((float)dst->w * src->h / src->w);
    int dstW = SDL_ceilf((float)dst->h * src->w / src->h);

    if (dstH > dst->h) {
        dst->x += (dst->w - dstW) / 2;
        dst->w = dstW;
    }
    else {
        dst->y += (dst->h - dstH) / 2;
        dst->h = dstH;
    }
}

void StreamUtils::screenSpaceToNormalizedDeviceCoords(SDL_FRect* rect, int viewportWidth, int viewportHeight)
{
    rect->x = (rect->x / (viewportWidth / 2.0f)) - 1.0f;
    rect->y = (rect->y / (viewportHeight / 2.0f)) - 1.0f;
    rect->w = rect->w / (viewportWidth / 2.0f);
    rect->h = rect->h / (viewportHeight / 2.0f);
}

void StreamUtils::screenSpaceToNormalizedDeviceCoords(SDL_Rect* src, SDL_FRect* dst, int viewportWidth, int viewportHeight)
{
    dst->x = ((float)src->x / (viewportWidth / 2.0f)) - 1.0f;
    dst->y = ((float)src->y / (viewportHeight / 2.0f)) - 1.0f;
    dst->w = (float)src->w / (viewportWidth / 2.0f);
    dst->h = (float)src->h / (viewportHeight / 2.0f);
}

int StreamUtils::getDisplayRefreshRate(SDL_Window* window)
{
    int displayIndex = SDL_GetWindowDisplayIndex(window);
    if (displayIndex < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to get current display: %s",
                     SDL_GetError());

        // Assume display 0 if it fails
        displayIndex = 0;
    }

    SDL_DisplayMode mode;
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
        // Use the window display mode for full-screen exclusive mode
        if (SDL_GetWindowDisplayMode(window, &mode) != 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_GetWindowDisplayMode() failed: %s",
                         SDL_GetError());

            // Assume 60 Hz
            return 60;
        }
    }
    else {
        // Use the current display mode for windowed and borderless
        if (SDL_GetCurrentDisplayMode(displayIndex, &mode) != 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_GetCurrentDisplayMode() failed: %s",
                         SDL_GetError());

            // Assume 60 Hz
            return 60;
        }
    }

    // May be zero if undefined
    if (mode.refresh_rate == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Refresh rate unknown; assuming 60 Hz");
        mode.refresh_rate = 60;
    }

    return mode.refresh_rate;
}

bool StreamUtils::hasFastAes()
{
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if __has_builtin(__builtin_cpu_supports) && defined(Q_PROCESSOR_X86)
    return __builtin_cpu_supports("aes");
#elif defined(__BUILTIN_CPU_SUPPORTS__) && defined(Q_PROCESSOR_POWER)
    return __builtin_cpu_supports("vcrypto");
#elif defined(Q_OS_WINDOWS) && defined(Q_PROCESSOR_ARM)
    return IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
#elif defined(_MSC_VER) && defined(Q_PROCESSOR_X86)
    int regs[4];
    __cpuid(regs, 1);
    return regs[2] & (1 << 25); // AES-NI
#elif defined(Q_OS_DARWIN)
    // Everything that runs Catalina and later has AES-NI or ARMv8 crypto instructions
    return true;
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_ARM) && QT_POINTER_SIZE == 4
    return getauxval(AT_HWCAP2) & HWCAP2_AES;
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_ARM) && QT_POINTER_SIZE == 8
    return getauxval(AT_HWCAP) & HWCAP_AES;
#elif defined(Q_PROCESSOR_RISCV)
    // TODO: Implement detection of RISC-V vector crypto extension when possible.
    // At the time of writing, no RISC-V hardware has it, so hardcode it off.
    return false;
#elif QT_POINTER_SIZE == 4
    #warning Unknown 32-bit platform. Assuming AES is slow on this CPU.
    return false;
#else
    #warning Unknown 64-bit platform. Assuming AES is fast on this CPU.
    return true;
#endif
}

bool StreamUtils::getNativeDesktopMode(int displayIndex, SDL_DisplayMode* mode)
{
#ifdef Q_OS_DARWIN
#define MAX_DISPLAYS 16
    CGDirectDisplayID displayIds[MAX_DISPLAYS];
    uint32_t displayCount = 0;
    CGGetActiveDisplayList(MAX_DISPLAYS, displayIds, &displayCount);
    if (displayIndex >= displayCount) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Too many displays: %d vs %d",
                     displayIndex, displayCount);
        return false;
    }

    SDL_zerop(mode);

    // Retina displays have non-native resolutions both below and above (!) their
    // native resolution, so it's impossible for us to figure out what's actually
    // native on macOS using the SDL API alone. We'll talk to CoreGraphics to
    // find the correct resolution and match it in our SDL list.
    CFArrayRef modeList = CGDisplayCopyAllDisplayModes(displayIds[displayIndex], nullptr);
    CFIndex count = CFArrayGetCount(modeList);
    for (CFIndex i = 0; i < count; i++) {
        auto cgMode = (CGDisplayModeRef)(CFArrayGetValueAtIndex(modeList, i));
        if ((CGDisplayModeGetIOFlags(cgMode) & kDisplayModeNativeFlag) != 0) {
            mode->w = static_cast<int>(CGDisplayModeGetWidth(cgMode));
            mode->h = static_cast<int>(CGDisplayModeGetHeight(cgMode));
            break;
        }
    }
    CFRelease(modeList);

    // Now find the SDL mode that matches the CG native mode
    for (int i = 0; i < SDL_GetNumDisplayModes(displayIndex); i++) {
        SDL_DisplayMode thisMode;
        if (SDL_GetDisplayMode(displayIndex, i, &thisMode) == 0) {
            if (thisMode.w == mode->w && thisMode.h == mode->h &&
                    thisMode.refresh_rate >= mode->refresh_rate) {
                *mode = thisMode;
                break;
            }
        }
    }
#else
    // We need to get the true display resolution without DPI scaling (since we use High DPI).
    // Windows returns the real display resolution here, even if DPI scaling is enabled.
    // macOS and Wayland report a resolution that includes the DPI scaling factor. Picking
    // the first mode on Wayland will get the native resolution without the scaling factor
    // (and macOS is handled in the #ifdef above).
    if (!strcmp(SDL_GetCurrentVideoDriver(), "wayland")) {
        if (SDL_GetDisplayMode(displayIndex, 0, mode) != 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_GetDisplayMode() failed: %s",
                         SDL_GetError());
            return false;
        }
    }
    else {
        if (SDL_GetDesktopDisplayMode(displayIndex, mode) != 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_GetDesktopDisplayMode() failed: %s",
                         SDL_GetError());
            return false;
        }
    }
#endif

    return true;
}
