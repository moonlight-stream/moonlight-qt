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
        dst->y = 0;
        dst->x = (dst->w - dstW) / 2;
        dst->w = dstW;
        SDL_assert(dst->w * src->h / src->w <= dst->h);
    }
    else {
        dst->x = 0;
        dst->y = (dst->h - dstH) / 2;
        dst->h = dstH;
        SDL_assert(dst->h * src->w / src->h <= dst->w);
    }
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
    if (SDL_GetDesktopDisplayMode(displayIndex, mode) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetDesktopDisplayMode() failed: %s",
                     SDL_GetError());
        return false;
    }
#endif

    return true;
}
