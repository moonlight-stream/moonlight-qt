#include "systemproperties.h"
#include "utils.h"

#include <QGuiApplication>

#include "streaming/session.h"
#include "streaming/streamutils.h"

SystemProperties::SystemProperties()
{
    isRunningWayland = WMUtils::isRunningWayland();
    isRunningXWayland = isRunningWayland && QGuiApplication::platformName() == "xcb";

#ifdef Q_OS_WIN32
    isWow64 = QSysInfo::currentCpuArchitecture() != QSysInfo::buildCpuArchitecture();
#else
    isWow64 = false;
#endif

#ifndef STEAM_LINK
    hasBrowser = true;
#else
    hasBrowser = false;
#endif

#ifdef HAVE_DISCORD
    hasDiscordIntegration = true;
#else
    hasDiscordIntegration = false;
#endif

    hasWindowManager = WMUtils::isRunningWindowManager();

    unmappedGamepads = SdlInputHandler::getUnmappedGamepads();

    // Populate data that requires talking to SDL. We do it all in one shot
    // and cache the results to speed up future queries on this data.
    querySdlVideoInfo();

    Q_ASSERT(maximumStreamingFrameRate >= 60);
    Q_ASSERT(!monitorDesktopResolutions.isEmpty());
    Q_ASSERT(!monitorNativeResolutions.isEmpty());
}

QRect SystemProperties::getDesktopResolution(int displayIndex)
{
    // Returns default constructed QRect if out of bounds
    return monitorDesktopResolutions.value(displayIndex);
}

QRect SystemProperties::getNativeResolution(int displayIndex)
{
    // Returns default constructed QRect if out of bounds
    return monitorNativeResolutions.value(displayIndex);
}

void SystemProperties::querySdlVideoInfo()
{
    monitorDesktopResolutions.clear();
    monitorNativeResolutions.clear();
    hasHardwareAcceleration = false;

    // Never let the maximum drop below 60 FPS
    maximumStreamingFrameRate = 60;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return;
    }

    SDL_DisplayMode bestMode;
    for (int displayIndex = 0; displayIndex < SDL_GetNumVideoDisplays(); displayIndex++) {
        SDL_DisplayMode desktopMode;
        int err;

        err = SDL_GetDesktopDisplayMode(displayIndex, &desktopMode);
        if (err == 0) {
            monitorDesktopResolutions.insert(displayIndex, QRect(0, 0, desktopMode.w, desktopMode.h));
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_GetDesktopDisplayMode() failed: %s",
                         SDL_GetError());
        }

        if (StreamUtils::getRealDesktopMode(displayIndex, &desktopMode)) {
            monitorNativeResolutions.insert(displayIndex, QRect(0, 0, desktopMode.w, desktopMode.h));

            // Start at desktop mode and work our way up
            bestMode = desktopMode;
            for (int i = 0; i < SDL_GetNumDisplayModes(displayIndex); i++) {
                SDL_DisplayMode mode;
                if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0) {
                    if (mode.w == desktopMode.w && mode.h == desktopMode.h) {
                        if (mode.refresh_rate > bestMode.refresh_rate) {
                            bestMode = mode;
                        }
                    }
                }
            }

            maximumStreamingFrameRate = qMax(maximumStreamingFrameRate, bestMode.refresh_rate);
        }
    }

    SDL_Window* testWindow = SDL_CreateWindow("", 0, 0, 1280, 720, SDL_WINDOW_HIDDEN);
    if (!testWindow) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create window for hardware decode test: %s",
                     SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    Session::getDecoderInfo(testWindow, hasHardwareAcceleration, rendererAlwaysFullScreen);

    SDL_DestroyWindow(testWindow);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
