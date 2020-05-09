#include "streamutils.h"

#include <Qt>

#ifdef Q_OS_DARWIN
#include <ApplicationServices/ApplicationServices.h>
#endif

void StreamUtils::scaleSourceToDestinationSurface(SDL_Rect* src, SDL_Rect* dst)
{
    int dstH = dst->w * src->h / src->w;
    int dstW = dst->h * src->w / src->h;

    if (dstH > dst->h) {
        dst->x += (dst->w - dstW) / 2;
        dst->w = dstW;
        SDL_assert(dst->w * src->h / src->w <= dst->h);
    }
    else {
        dst->y += (dst->h - dstH) / 2;
        dst->h = dstH;
        SDL_assert(dst->h * src->w / src->h <= dst->w);
    }
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

bool StreamUtils::getRealDesktopMode(int displayIndex, SDL_DisplayMode* mode)
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

    // Grab the (possibly scaled) desktop resolution
    if (SDL_GetDesktopDisplayMode(displayIndex, mode) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetDesktopDisplayMode() failed: %s",
                     SDL_GetError());
        return false;
    }

    // This is required to ensure we get the high DPI display modes
    const CFStringRef dictKeys[] = {kCGDisplayShowDuplicateLowResolutionModes};
    const CFBooleanRef dictValues[] = {kCFBooleanTrue};
    CFDictionaryRef dict = CFDictionaryCreate(NULL,
                                              (const void**)dictKeys,
                                              (const void**)dictValues,
                                              SDL_arraysize(dictValues),
                                              &kCFCopyStringDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);

    // Retina displays have non-native resolutions both below and above (!) their
    // native resolution, so it's impossible for us to figure out what's actually
    // native on macOS using the SDL API alone. We'll talk to CoreGraphics to
    // find the correct resolution and match it in our SDL list.
    CFArrayRef modeList = CGDisplayCopyAllDisplayModes(displayIds[displayIndex], dict);
    CFRelease(dict);

    // Get the physical size of the matching logical resolution
    CFIndex count = CFArrayGetCount(modeList);
    for (CFIndex i = 0; i < count; i++) {
        auto cgMode = (CGDisplayModeRef)(CFArrayGetValueAtIndex(modeList, i));

        auto modeWidth = static_cast<int>(CGDisplayModeGetWidth(cgMode));
        auto modeHeight = static_cast<int>(CGDisplayModeGetHeight(cgMode));
        if (mode->w == modeWidth && mode->h == modeHeight) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Matching desktop resolution is: %zux%zu (scaled size: %zux%zu) (flags: %x)",
                        CGDisplayModeGetPixelWidth(cgMode),
                        CGDisplayModeGetPixelHeight(cgMode),
                        CGDisplayModeGetWidth(cgMode),
                        CGDisplayModeGetHeight(cgMode),
                        CGDisplayModeGetIOFlags(cgMode));

            mode->w = static_cast<int>(CGDisplayModeGetPixelWidth(cgMode));
            mode->h = static_cast<int>(CGDisplayModeGetPixelHeight(cgMode));
            break;
        }
    }

    CFRelease(modeList);
#else
    if (SDL_GetDesktopDisplayMode(displayIndex, mode) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetDesktopDisplayMode() failed: %s",
                     SDL_GetError());
        return false;
    }
#endif

    return true;
}
