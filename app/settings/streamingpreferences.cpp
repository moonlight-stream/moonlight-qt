#include "streamingpreferences.h"
#include "streaming/session.hpp"
#include "streaming/streamutils.h"

#include <QSettings>

#define SER_STREAMSETTINGS "streamsettings"
#define SER_WIDTH "width"
#define SER_HEIGHT "height"
#define SER_FPS "fps"
#define SER_BITRATE "bitrate"
#define SER_FULLSCREEN "fullscreen"
#define SER_VSYNC "vsync"
#define SER_GAMEOPTS "gameopts"
#define SER_HOSTAUDIO "hostaudio"
#define SER_MULTICONT "multicontroller"
#define SER_AUDIOCFG "audiocfg"
#define SER_VIDEOCFG "videocfg"
#define SER_VIDEODEC "videodec"
#define SER_WINDOWMODE "windowmode"
#define SER_UNSUPPORTEDFPS "unsupportedfps"
#define SER_MDNS "mdns"
#define SER_MOUSEACCELERATION "mouseacceleration"

StreamingPreferences::StreamingPreferences()
{
    reload();
}

void StreamingPreferences::reload()
{
    QSettings settings;

    width = settings.value(SER_WIDTH, 1280).toInt();
    height = settings.value(SER_HEIGHT, 720).toInt();
    fps = settings.value(SER_FPS, 60).toInt();
    bitrateKbps = settings.value(SER_BITRATE, getDefaultBitrate(width, height, fps)).toInt();
    enableVsync = settings.value(SER_VSYNC, true).toBool();
    gameOptimizations = settings.value(SER_GAMEOPTS, true).toBool();
    playAudioOnHost = settings.value(SER_HOSTAUDIO, false).toBool();
    multiController = settings.value(SER_MULTICONT, true).toBool();
    unsupportedFps = settings.value(SER_UNSUPPORTEDFPS, false).toBool();
    enableMdns = settings.value(SER_MDNS, true).toBool();
    mouseAcceleration = settings.value(SER_MOUSEACCELERATION, false).toBool();
    audioConfig = static_cast<AudioConfig>(settings.value(SER_AUDIOCFG,
                                                  static_cast<int>(AudioConfig::AC_AUTO)).toInt());
    videoCodecConfig = static_cast<VideoCodecConfig>(settings.value(SER_VIDEOCFG,
                                                  static_cast<int>(VideoCodecConfig::VCC_AUTO)).toInt());
    videoDecoderSelection = static_cast<VideoDecoderSelection>(settings.value(SER_VIDEODEC,
                                                  static_cast<int>(VideoDecoderSelection::VDS_AUTO)).toInt());
    windowMode = static_cast<WindowMode>(settings.value(SER_WINDOWMODE,
                                                        // Try to load from the old preference value too
                                                        static_cast<int>(settings.value(SER_FULLSCREEN, true).toBool() ?
                                                                             WindowMode::WM_FULLSCREEN : WindowMode::WM_WINDOWED)).toInt());
}

void StreamingPreferences::save()
{
    QSettings settings;

    settings.setValue(SER_WIDTH, width);
    settings.setValue(SER_HEIGHT, height);
    settings.setValue(SER_FPS, fps);
    settings.setValue(SER_BITRATE, bitrateKbps);
    settings.setValue(SER_VSYNC, enableVsync);
    settings.setValue(SER_GAMEOPTS, gameOptimizations);
    settings.setValue(SER_HOSTAUDIO, playAudioOnHost);
    settings.setValue(SER_MULTICONT, multiController);
    settings.setValue(SER_UNSUPPORTEDFPS, unsupportedFps);
    settings.setValue(SER_MDNS, enableMdns);
    settings.setValue(SER_MOUSEACCELERATION, mouseAcceleration);
    settings.setValue(SER_AUDIOCFG, static_cast<int>(audioConfig));
    settings.setValue(SER_VIDEOCFG, static_cast<int>(videoCodecConfig));
    settings.setValue(SER_VIDEODEC, static_cast<int>(videoDecoderSelection));
    settings.setValue(SER_WINDOWMODE, static_cast<int>(windowMode));
}

bool StreamingPreferences::hasAnyHardwareAcceleration()
{
    // Always use VDS_AUTO to avoid spamming the user with warnings
    // if they've forced software decoding.
    return Session::isHardwareDecodeAvailable(VDS_AUTO,
                                              VIDEO_FORMAT_H264,
                                              1920, 1080, 60);
}

bool StreamingPreferences::isRunningWayland()
{
    return qgetenv("XDG_SESSION_TYPE") == QByteArray("wayland");
}

int StreamingPreferences::getMaximumStreamingFrameRate()
{
    // Never let the maximum drop below 60 FPS
    int maxFrameRate = 60;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return maxFrameRate;
    }

    SDL_DisplayMode mode, bestMode, desktopMode;
    for (int displayIndex = 0; displayIndex < SDL_GetNumVideoDisplays(); displayIndex++) {
        // Get the native desktop resolution
        if (StreamUtils::getRealDesktopMode(displayIndex, &desktopMode)) {

            // Start at desktop mode and work our way up
            bestMode = desktopMode;
            for (int i = 0; i < SDL_GetNumDisplayModes(displayIndex); i++) {
                if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0) {
                    if (mode.w == desktopMode.w && mode.h == desktopMode.h) {
                        if (mode.refresh_rate > bestMode.refresh_rate) {
                            bestMode = mode;
                        }
                    }
                }
            }

            // Cap the frame rate at 120 FPS. Past this, the encoders start
            // to max out and drop frames.
            maxFrameRate = qMax(maxFrameRate, qMin(120, bestMode.refresh_rate));
        }
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    return maxFrameRate;
}

QRect StreamingPreferences::getDesktopResolution(int displayIndex)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return QRect();
    }

    if (displayIndex >= SDL_GetNumVideoDisplays()) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return QRect();
    }

    SDL_DisplayMode mode;
    int err = SDL_GetDesktopDisplayMode(displayIndex, &mode);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    if (err == 0) {
        return QRect(0, 0, mode.w, mode.h);
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetDesktopDisplayMode() failed: %s",
                     SDL_GetError());
        return QRect();
    }
}

QRect StreamingPreferences::getNativeResolution(int displayIndex)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return QRect();
    }

    if (displayIndex >= SDL_GetNumVideoDisplays()) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return QRect();
    }

    SDL_DisplayMode mode;
    bool success = StreamUtils::getRealDesktopMode(displayIndex, &mode);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    if (success) {
        return QRect(0, 0, mode.w, mode.h);
    }
    else {
        return QRect();
    }
}

int StreamingPreferences::getDefaultBitrate(int width, int height, int fps)
{
    // This table prefers 16:10 resolutions because they are
    // only slightly more pixels than the 16:9 equivalents, so
    // we don't want to bump those 16:10 resolutions up to the
    // next 16:9 slot.

    // This covers 1280x720 and 1280x800 too
    if (width * height <= 1366 * 768) {
        return static_cast<int>(5000 * (fps / 30.0));
    }
    else if (width * height <= 1920 * 1200) {
        return static_cast<int>(10000 * (fps / 30.0));
    }
    else if (width * height <= 2560 * 1600) {
        return static_cast<int>(20000 * (fps / 30.0));
    }
    else /* if (width * height <= 3840 * 2160) */ {
        return static_cast<int>(40000 * (fps / 30.0));
    }
}
