#include "systemproperties.h"
#include "utils.h"

#include <QGuiApplication>
#include <QLibraryInfo>

#include "streaming/session.h"
#include "streaming/streamutils.h"

#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

SystemProperties::SystemProperties()
{
    versionString = QString(VERSION_STR);
    hasDesktopEnvironment = WMUtils::isRunningDesktopEnvironment();
    isRunningWayland = WMUtils::isRunningWayland();
    isRunningXWayland = isRunningWayland && QGuiApplication::platformName() == "xcb";
    usesMaterial3Theme = QLibraryInfo::version() >= QVersionNumber(6, 5, 0);
    QString nativeArch = QSysInfo::currentCpuArchitecture();

#ifdef Q_OS_WIN32
    {
        USHORT processArch, machineArch;

        // Use IsWow64Process2 on TH2 and later, because it supports ARM64
        auto fnIsWow64Process2 = (decltype(IsWow64Process2)*)GetProcAddress(GetModuleHandleA("kernel32.dll"), "IsWow64Process2");
        if (fnIsWow64Process2 != nullptr && fnIsWow64Process2(GetCurrentProcess(), &processArch, &machineArch)) {
            switch (machineArch) {
            case IMAGE_FILE_MACHINE_I386:
                nativeArch = "i386";
                break;
            case IMAGE_FILE_MACHINE_AMD64:
                nativeArch = "x86_64";
                break;
            case IMAGE_FILE_MACHINE_ARM64:
                nativeArch = "arm64";
                break;
            }
        }

        isWow64 = nativeArch != QSysInfo::buildCpuArchitecture();
    }
#else
    isWow64 = false;
#endif

    if (nativeArch == "i386") {
        friendlyNativeArchName = "x86";
    }
    else if (nativeArch == "x86_64") {
        friendlyNativeArchName = "x64";
    }
    else {
        friendlyNativeArchName = nativeArch.toUpper();
    }

    // Assume we can probably launch a browser if we're in a GUI environment
    hasBrowser = hasDesktopEnvironment;

#ifdef HAVE_DISCORD
    hasDiscordIntegration = true;
#else
    hasDiscordIntegration = false;
#endif

    unmappedGamepads = SdlInputHandler::getUnmappedGamepads();

    // Populate data that requires talking to SDL. We do it all in one shot
    // and cache the results to speed up future queries on this data.
    querySdlVideoInfo();

    Q_ASSERT(!monitorRefreshRates.isEmpty());
    Q_ASSERT(!monitorNativeResolutions.isEmpty());
}

QRect SystemProperties::getNativeResolution(int displayIndex)
{
    // Returns default constructed QRect if out of bounds
    return monitorNativeResolutions.value(displayIndex);
}

int SystemProperties::getRefreshRate(int displayIndex)
{
    // Returns 0 if out of bounds
    return monitorRefreshRates.value(displayIndex);
}

class QuerySdlVideoThread : public QThread
{
public:
    QuerySdlVideoThread(SystemProperties* me) :
        QThread(nullptr),
        m_Me(me) {}

    void run() override
    {
        m_Me->querySdlVideoInfoInternal();
    }

    SystemProperties* m_Me;
};

void SystemProperties::querySdlVideoInfo()
{
    if (WMUtils::isRunningX11() || WMUtils::isRunningWayland()) {
        // Use a separate thread to temporarily initialize SDL
        // video to avoid stomping on Qt's X11 and OGL state.
        QuerySdlVideoThread thread(this);
        thread.start();
        thread.wait();
    }
    else {
        querySdlVideoInfoInternal();
    }
}

void SystemProperties::querySdlVideoInfoInternal()
{
    hasHardwareAcceleration = false;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return;
    }

    // Update display related attributes (max FPS, native resolution, etc).
    // We call the internal variant because we're already in a safe thread context.
    refreshDisplaysInternal();

#if SDL_VERSION_ATLEAST(3, 0, 0)
    SDL_Window* testWindow = SDL_CreateWindow("", 1280, 720,
#else
    SDL_Window* testWindow = SDL_CreateWindow("", 0, 0, 1280, 720,
#endif
                                              SDL_WINDOW_HIDDEN | StreamUtils::getPlatformWindowFlags());
    if (!testWindow) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to create test window with platform flags: %s",
                    SDL_GetError());

#if SDL_VERSION_ATLEAST(3, 0, 0)
        testWindow = SDL_CreateWindow("", 1280, 720, SDL_WINDOW_HIDDEN);
#else
        testWindow = SDL_CreateWindow("", 0, 0, 1280, 720, SDL_WINDOW_HIDDEN);
#endif
        if (!testWindow) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create window for hardware decode test: %s",
                         SDL_GetError());
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            return;
        }
    }

    Session::getDecoderInfo(testWindow, hasHardwareAcceleration, rendererAlwaysFullScreen, supportsHdr, maximumResolution);

    SDL_DestroyWindow(testWindow);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

class RefreshDisplaysThread : public QThread
{
public:
    RefreshDisplaysThread(SystemProperties* me) :
        QThread(nullptr),
        m_Me(me) {}

    void run() override
    {
        m_Me->refreshDisplaysInternal();
    }

    SystemProperties* m_Me;
};

void SystemProperties::refreshDisplays()
{
    if (WMUtils::isRunningX11() || WMUtils::isRunningWayland()) {
        // Use a separate thread to temporarily initialize SDL
        // video to avoid stomping on Qt's X11 and OGL state.
        RefreshDisplaysThread thread(this);
        thread.start();
        thread.wait();
    }
    else {
        refreshDisplaysInternal();
    }
}

void SystemProperties::refreshDisplaysInternal()
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return;
    }

    monitorNativeResolutions.clear();

    SDL_DisplayMode bestMode;
#if SDL_VERSION_ATLEAST(3, 0, 0)
    int numDisplays = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&numDisplays);
    for (int displayIndex = 0; displayIndex < numDisplays; displayIndex++) {
#else
    for (int displayIndex = 0; displayIndex < SDL_GetNumVideoDisplays(); displayIndex++) {
#endif
        SDL_DisplayMode desktopMode;

        if (StreamUtils::getNativeDesktopMode(displayIndex, &desktopMode)) {
            if (desktopMode.w <= 8192 && desktopMode.h <= 8192) {
                monitorNativeResolutions.insert(displayIndex, QRect(0, 0, desktopMode.w, desktopMode.h));
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Skipping resolution over 8K: %dx%d",
                            desktopMode.w, desktopMode.h);
            }

            // Start at desktop mode and work our way up
            bestMode = desktopMode;
#if SDL_VERSION_ATLEAST(3, 0, 0)
            int numModes = 0;
            const SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(displayIndex, &numModes);
            for (int i = 0; i < numModes; i++) {
                if (modes[i]->w == desktopMode.w && modes[i]->h == desktopMode.h) {
                    if (modes[i]->refresh_rate > bestMode.refresh_rate) {
                        bestMode = *modes[i];
                    }
                }
            }
            if (modes) {
                SDL_free(modes);
            }
#else
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
#endif

            // Try to normalize values around our our standard refresh rates.
            // Some displays/OSes report values that are slightly off.
            if (bestMode.refresh_rate >= 58 && bestMode.refresh_rate <= 62) {
                monitorRefreshRates.append(60);
            }
            else if (bestMode.refresh_rate >= 28 && bestMode.refresh_rate <= 32) {
                monitorRefreshRates.append(30);
            }
            else {
                monitorRefreshRates.append(bestMode.refresh_rate);
            }
        }
    }
#if SDL_VERSION_ATLEAST(3, 0, 0)
    if (displays) {
        SDL_free((void*)displays);
    }
#endif

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
