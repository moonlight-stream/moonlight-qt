#include "session.hpp"
#include "settings/streamingpreferences.h"

#include <Limelight.h>
#include <SDL.h>
#include "utils.h"

#ifdef HAVE_FFMPEG
#include "video/ffmpeg.h"
#endif

#ifdef HAVE_SLVIDEO
#include "video/sl.h"
#endif

#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN)
// Using full-screen desktop allows us to avoid needing to enable V-sync on Windows
// and it also avoids some strange flickering issues on my Win7 test machine
// with Intel HD 5500 graphics. It also allows Spaces to work on macOS.
#define SDL_OS_FULLSCREEN_FLAG SDL_WINDOW_FULLSCREEN_DESKTOP
#else
#define SDL_OS_FULLSCREEN_FLAG SDL_WINDOW_FULLSCREEN
#endif

#ifdef Q_OS_WIN32
// Scaling the icon down on Win32 looks dreadful, so render at lower res
#define ICON_SIZE 32
#else
#define ICON_SIZE 64
#endif

#include <openssl/rand.h>

#include <QtEndian>
#include <QCoreApplication>
#include <QThreadPool>
#include <QSvgRenderer>
#include <QPainter>
#include <QImage>

CONNECTION_LISTENER_CALLBACKS Session::k_ConnCallbacks = {
    Session::clStageStarting,
    nullptr,
    Session::clStageFailed,
    nullptr,
    Session::clConnectionTerminated,
    nullptr,
    nullptr,
    Session::clLogMessage
};

AUDIO_RENDERER_CALLBACKS Session::k_AudioCallbacks = {
    Session::sdlAudioInit,
    Session::sdlAudioStart,
    Session::sdlAudioStop,
    Session::sdlAudioCleanup,
    Session::sdlAudioDecodeAndPlaySample,
    CAPABILITY_DIRECT_SUBMIT
};

Session* Session::s_ActiveSession;
QSemaphore Session::s_ActiveSessionSemaphore(1);

void Session::clStageStarting(int stage)
{
    // We know this is called on the same thread as LiStartConnection()
    // which happens to be the main thread, so it's cool to interact
    // with the GUI in these callbacks.
    emit s_ActiveSession->stageStarting(QString::fromLocal8Bit(LiGetStageName(stage)));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void Session::clStageFailed(int stage, long errorCode)
{
    // We know this is called on the same thread as LiStartConnection()
    // which happens to be the main thread, so it's cool to interact
    // with the GUI in these callbacks.
    emit s_ActiveSession->stageFailed(QString::fromLocal8Bit(LiGetStageName(stage)), errorCode);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void Session::clConnectionTerminated(long errorCode)
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Connection terminated: %ld",
                 errorCode);

    // Push a quit event to the main loop
    SDL_Event event;
    event.type = SDL_QUIT;
    event.quit.timestamp = SDL_GetTicks();
    SDL_PushEvent(&event);
}

void Session::clLogMessage(const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION,
                    SDL_LOG_PRIORITY_INFO,
                    format,
                    ap);
    va_end(ap);
}

bool Session::chooseDecoder(StreamingPreferences::VideoDecoderSelection vds,
                            SDL_Window* window, int videoFormat, int width, int height,
                            int frameRate, IVideoDecoder*& chosenDecoder)
{
#ifdef HAVE_SLVIDEO
    chosenDecoder = new SLVideoDecoder();
    if (chosenDecoder->initialize(vds, window, videoFormat, width, height, frameRate)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "SLVideo video decoder chosen");
        return true;
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to load SLVideo decoder");
        delete chosenDecoder;
        chosenDecoder = nullptr;
    }
#endif

#ifdef HAVE_FFMPEG
    chosenDecoder = new FFmpegVideoDecoder();
    if (chosenDecoder->initialize(vds, window, videoFormat, width, height, frameRate)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "FFmpeg-based video decoder chosen");
        return true;
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to load FFmpeg decoder");
        delete chosenDecoder;
        chosenDecoder = nullptr;
    }
#endif

#if !defined(HAVE_FFMPEG) && !defined(HAVE_SLVIDEO)
#error No video decoding libraries available!
#endif

    // If we reach this, we didn't initialize any decoders successfully
    return false;
}

int Session::drSetup(int videoFormat, int width, int height, int frameRate, void *, int)
{
    s_ActiveSession->m_ActiveVideoFormat = videoFormat;
    s_ActiveSession->m_ActiveVideoWidth = width;
    s_ActiveSession->m_ActiveVideoHeight = height;
    s_ActiveSession->m_ActiveVideoFrameRate = frameRate;

    // Defer decoder setup until we've started streaming so we
    // don't have to hide and show the SDL window (which seems to
    // cause pointer hiding to break on Windows).

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Video stream is %dx%dx%d (format 0x%x)",
                width, height, frameRate, videoFormat);

    return 0;
}

int Session::drSubmitDecodeUnit(PDECODE_UNIT du)
{
    // Use a lock since we'll be yanking this decoder out
    // from underneath the session when we initiate destruction.
    // We need to destroy the decoder on the main thread to satisfy
    // some API constraints (like DXVA2).

    SDL_AtomicLock(&s_ActiveSession->m_DecoderLock);

    if (s_ActiveSession->m_NeedsIdr) {
        // If we reset our decoder, we'll need to request an IDR frame
        s_ActiveSession->m_NeedsIdr = false;
        SDL_AtomicUnlock(&s_ActiveSession->m_DecoderLock);
        return DR_NEED_IDR;
    }

    IVideoDecoder* decoder = s_ActiveSession->m_VideoDecoder;
    if (decoder != nullptr) {
        int ret = decoder->submitDecodeUnit(du);
        SDL_AtomicUnlock(&s_ActiveSession->m_DecoderLock);
        return ret;
    }
    else {
        SDL_AtomicUnlock(&s_ActiveSession->m_DecoderLock);
        return DR_OK;
    }
}

bool Session::isHardwareDecodeAvailable(StreamingPreferences::VideoDecoderSelection vds,
                                        int videoFormat, int width, int height, int frameRate)
{
    IVideoDecoder* decoder;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return false;
    }

    SDL_Window* window = SDL_CreateWindow("", 0, 0, width, height, SDL_WINDOW_HIDDEN);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create window for hardware decode test: %s",
                     SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    if (!chooseDecoder(vds, window, videoFormat, width, height, frameRate, decoder)) {
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    SDL_DestroyWindow(window);

    bool ret = decoder->isHardwareAccelerated();

    delete decoder;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    return ret;
}

Session::Session(NvComputer* computer, NvApp& app)
    : m_Computer(computer),
      m_App(app),
      m_Window(nullptr),
      m_VideoDecoder(nullptr),
      m_DecoderLock(0),
      m_NeedsIdr(false)
{
    qDebug() << "Server GPU:" << m_Computer->gpuModel;

    LiInitializeVideoCallbacks(&m_VideoCallbacks);
    m_VideoCallbacks.setup = drSetup;
    m_VideoCallbacks.submitDecodeUnit = drSubmitDecodeUnit;

    // Submit for decode without using a separate thread
    m_VideoCallbacks.capabilities |= CAPABILITY_DIRECT_SUBMIT;

    // Slice up to 4 times for parallel decode, once slice per core
    int slices = qMin(MAX_SLICES, SDL_GetCPUCount());
    m_VideoCallbacks.capabilities |= CAPABILITY_SLICES_PER_FRAME(slices);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Encoder configured for %d slices per frame",
                slices);

    LiInitializeStreamConfiguration(&m_StreamConfig);
    m_StreamConfig.width = m_Preferences.width;
    m_StreamConfig.height = m_Preferences.height;
    m_StreamConfig.fps = m_Preferences.fps;
    m_StreamConfig.bitrate = m_Preferences.bitrateKbps;
    m_StreamConfig.hevcBitratePercentageMultiplier = 75;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Video bitrate: %d kbps",
                m_StreamConfig.bitrate);

    RAND_bytes(reinterpret_cast<unsigned char*>(m_StreamConfig.remoteInputAesKey),
               sizeof(m_StreamConfig.remoteInputAesKey));

    // Only the first 4 bytes are populated in the RI key IV
    RAND_bytes(reinterpret_cast<unsigned char*>(m_StreamConfig.remoteInputAesIv), 4);

    switch (m_Preferences.audioConfig)
    {
    case StreamingPreferences::AC_AUTO:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Autodetecting audio configuration");
        m_StreamConfig.audioConfiguration = sdlDetermineAudioConfiguration();
        break;
    case StreamingPreferences::AC_FORCE_STEREO:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
        break;
    case StreamingPreferences::AC_FORCE_SURROUND:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_51_SURROUND;
        break;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio configuration: %d",
                m_StreamConfig.audioConfiguration);

    switch (m_Preferences.videoCodecConfig)
    {
    case StreamingPreferences::VCC_AUTO:
        // TODO: Determine if HEVC is better depending on the decoder
        m_StreamConfig.supportsHevc =
                isHardwareDecodeAvailable(m_Preferences.videoDecoderSelection,
                                          VIDEO_FORMAT_H265,
                                          m_StreamConfig.width,
                                          m_StreamConfig.height,
                                          m_StreamConfig.fps);
        m_StreamConfig.enableHdr = false;
        break;
    case StreamingPreferences::VCC_FORCE_H264:
        m_StreamConfig.supportsHevc = false;
        m_StreamConfig.enableHdr = false;
        break;
    case StreamingPreferences::VCC_FORCE_HEVC:
        m_StreamConfig.supportsHevc = true;
        m_StreamConfig.enableHdr = false;
        break;
    case StreamingPreferences::VCC_FORCE_HEVC_HDR:
        m_StreamConfig.supportsHevc = true;
        m_StreamConfig.enableHdr = true;
        break;
    }

    if (computer->activeAddress == computer->remoteAddress) {
        m_StreamConfig.streamingRemotely = 1;
    }
    else {
        m_StreamConfig.streamingRemotely = 0;
    }

    if (computer->activeAddress == computer->localAddress) {
        m_StreamConfig.packetSize = 1392;
    }
    else {
        m_StreamConfig.packetSize = 1024;
    }
}

void Session::emitLaunchWarning(QString text)
{
    // Emit the warning to the UI
    emit displayLaunchWarning(text);

    // Wait a little bit so the user can actually read what we just said.
    // This wait is a little longer than the actual toast timeout (3 seconds)
    // to allow it to transition off the screen before continuing.
    uint32_t start = SDL_GetTicks();
    while (!SDL_TICKS_PASSED(SDL_GetTicks(), start + 3500)) {
        // Pump the UI loop while we wait
        SDL_Delay(5);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

bool Session::validateLaunch()
{
    QStringList warningList;

    if (m_Preferences.videoDecoderSelection == StreamingPreferences::VDS_FORCE_SOFTWARE) {
        emitLaunchWarning("Your settings selection to force software decoding may cause poor streaming performance.");
    }

    if (m_StreamConfig.supportsHevc) {
        bool hevcForced = m_Preferences.videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC ||
                m_Preferences.videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC_HDR;

        if (!isHardwareDecodeAvailable(m_Preferences.videoDecoderSelection,
                                       VIDEO_FORMAT_H265,
                                       m_StreamConfig.width,
                                       m_StreamConfig.height,
                                       m_StreamConfig.fps) &&
                m_Preferences.videoDecoderSelection == StreamingPreferences::VDS_AUTO) {
            if (hevcForced) {
                emitLaunchWarning("Using software decoding due to your selection to force HEVC without GPU support. This may cause poor streaming performance.");
            }
            else {
                emitLaunchWarning("This PC's GPU doesn't support HEVC decoding.");
                m_StreamConfig.supportsHevc = false;
            }
        }

        if (hevcForced) {
            if (m_Computer->maxLumaPixelsHEVC == 0) {
                emitLaunchWarning("Your host PC GPU doesn't support HEVC. "
                                  "A GeForce GTX 900-series (Maxwell) or later GPU is required for HEVC streaming.");

                // Moonlight-common-c will handle this case already, but we want
                // to set this explicitly here so we can do our hardware acceleration
                // check below.
                m_StreamConfig.supportsHevc = false;
            }
        }
    }

    if (m_StreamConfig.enableHdr) {
        // Turn HDR back off unless all criteria are met.
        m_StreamConfig.enableHdr = false;

        // Check that the app supports HDR
        if (!m_App.hdrSupported) {
            emitLaunchWarning(m_App.name + " doesn't support HDR10.");
        }
        // Check that the server GPU supports HDR
        else if (!(m_Computer->serverCodecModeSupport & 0x200)) {
            emitLaunchWarning("Your host PC GPU doesn't support HDR streaming. "
                              "A GeForce GTX 1000-series (Pascal) or later GPU is required for HDR streaming.");
        }
        else if (!isHardwareDecodeAvailable(m_Preferences.videoDecoderSelection,
                                            VIDEO_FORMAT_H265_MAIN10,
                                            m_StreamConfig.width,
                                            m_StreamConfig.height,
                                            m_StreamConfig.fps)) {
            emitLaunchWarning("This PC's GPU doesn't support HEVC Main10 decoding for HDR streaming.");
        }
        else {
            // TODO: Also validate display capabilites

            // Validation successful so HDR is good to go
            m_StreamConfig.enableHdr = true;
        }
    }

    if (m_StreamConfig.width >= 3840) {
        // Only allow 4K on GFE 3.x+
        if (m_Computer->gfeVersion.isNull() || m_Computer->gfeVersion.startsWith("2.")) {
            emitLaunchWarning("GeForce Experience 3.0 or higher is required for 4K streaming.");

            m_StreamConfig.width = 1920;
            m_StreamConfig.height = 1080;
        }
        // This list is sorted from least to greatest
        else if (m_Computer->displayModes.last().width < 3840 ||
                 (m_Computer->displayModes.last().refreshRate < 60 && m_StreamConfig.fps >= 60)) {
            emitLaunchWarning("Your host PC GPU doesn't support 4K streaming. "
                              "A GeForce GTX 900-series (Maxwell) or later GPU is required for 4K streaming.");

            m_StreamConfig.width = 1920;
            m_StreamConfig.height = 1080;
        }
    }

    if (m_Preferences.videoDecoderSelection == StreamingPreferences::VDS_FORCE_HARDWARE &&
            !isHardwareDecodeAvailable(m_Preferences.videoDecoderSelection,
                                       m_StreamConfig.supportsHevc ? VIDEO_FORMAT_H265 : VIDEO_FORMAT_H264,
                                       m_StreamConfig.width,
                                       m_StreamConfig.height,
                                       m_StreamConfig.fps)) {
        if (m_Preferences.videoCodecConfig == StreamingPreferences::VCC_AUTO) {
            emit displayLaunchError("Your selection to force hardware decoding cannot be satisfied due to missing hardware decoding support on this PC's GPU.");
        }
        else {
            emit displayLaunchError("Your codec selection and force hardware decoding setting are not compatible. This PC's GPU lacks support for decoding your chosen codec.");
        }

        // Fail the launch, because we won't manage to get a decoder for the actual stream
        return false;
    }

    return true;
}


class DeferredSessionCleanupTask : public QRunnable
{
    void run() override
    {
        // Finish cleanup of the connection state
        LiStopConnection();

        // Allow another session to start now that we're cleaned up
        Session::s_ActiveSession = nullptr;
        Session::s_ActiveSessionSemaphore.release();
    }
};

void Session::getWindowDimensions(bool fullScreen,
                                  int& x, int& y,
                                  int& width, int& height)
{
    int displayIndex = 0;

    // If there's a display matching this exact resolution, pick that
    // one (for native full-screen streaming). Otherwise, assume
    // display 0 for now. TODO: Default to the screen that the Qt window is on
    if (fullScreen) {
        for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
            SDL_DisplayMode mode;
            if (SDL_GetCurrentDisplayMode(i, &mode) == 0 &&
                    m_ActiveVideoWidth == mode.w &&
                    m_ActiveVideoHeight == mode.h) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Found exact resolution match on display: %d",
                            i);
                displayIndex = i;
                break;
            }
        }
    }

    if (m_Window != nullptr) {
        displayIndex = SDL_GetWindowDisplayIndex(m_Window);
        if (displayIndex < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_GetWindowDisplayIndex() failed: %s",
                         SDL_GetError());

            // Assume display 0
            displayIndex = 0;
        }
    }

    if (fullScreen) {
        SDL_DisplayMode currentMode;

        if (SDL_GetCurrentDisplayMode(displayIndex, &currentMode) == 0) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Using current display mode: %dx%dx%d",
                        currentMode.w, currentMode.h, currentMode.refresh_rate);
            width = currentMode.w;
            height = currentMode.h;
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to get current display mode: %s",
                         SDL_GetError());
            width = m_StreamConfig.width;
            height = m_StreamConfig.height;
        }

        // Full-screen modes always start at the origin
        x = y = SDL_WINDOWPOS_UNDEFINED_DISPLAY(displayIndex);
    }
    else {
        SDL_Rect usableBounds;
        if (SDL_GetDisplayUsableBounds(displayIndex, &usableBounds) == 0) {
            width = usableBounds.w;
            height = usableBounds.h;
            x = usableBounds.x;
            y = usableBounds.y;

            if (m_Window != nullptr) {
                int top, left, bottom, right;

                if (SDL_GetWindowBordersSize(m_Window, &top, &left, &bottom, &right) < 0) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Unable to get window border size");
                    return;
                }

                x += left;
                y += top;

                width -= left + right;
                height -= top + bottom;

                // If the stream window can fit within the usable drawing area with 1:1
                // scaling, do that rather than filling the screen.
                if (m_StreamConfig.width < width && m_StreamConfig.height < height) {
                    width = m_StreamConfig.width;
                    height = m_StreamConfig.height;
                }
            }
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_GetDisplayUsableBounds() failed: %s",
                         SDL_GetError());

            width = m_StreamConfig.width;
            height = m_StreamConfig.height;
            x = y = SDL_WINDOWPOS_UNDEFINED_DISPLAY(displayIndex);
        }
    }
}

void Session::toggleFullscreen()
{
    bool fullScreen = !(SDL_GetWindowFlags(m_Window) & SDL_OS_FULLSCREEN_FLAG);

    int x, y, width, height;

    // If we're leaving full screen, toggle out before setting our size and position
    // that way we have accurate display size metrics (not the size our stream changed it to).
    if (!fullScreen) {
        SDL_SetWindowFullscreen(m_Window, 0);
        SDL_SetWindowResizable(m_Window, SDL_TRUE);
    }

    getWindowDimensions(fullScreen, x, y, width, height);

    SDL_SetWindowPosition(m_Window, x, y);
    SDL_SetWindowSize(m_Window, width, height);

    if (fullScreen) {
        SDL_SetWindowResizable(m_Window, SDL_FALSE);
        SDL_SetWindowFullscreen(m_Window, SDL_OS_FULLSCREEN_FLAG);
    }
}

void Session::exec()
{
    // Check for validation errors/warnings and emit
    // signals for them, if appropriate
    if (!validateLaunch()) {
        return;
    }

    // Manually pump the UI thread for the view
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // Wait for any old session to finish cleanup
    s_ActiveSessionSemaphore.acquire();

    // We're now active
    s_ActiveSession = this;

    // Initialize the gamepad code with our preferences
    StreamingPreferences prefs;
    SdlInputHandler inputHandler(prefs.multiController);

    // The UI should have ensured the old game was already quit
    // if we decide to stream a different game.
    Q_ASSERT(m_Computer->currentGameId == 0 ||
             m_Computer->currentGameId == m_App.id);

    bool enableGameOptimizations = false;
    for (const NvDisplayMode &mode : m_Computer->displayModes) {
        if (mode.width == m_StreamConfig.width &&
                mode.height == m_StreamConfig.height) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Found host supported resolution: %dx%d",
                        mode.width, mode.height);
            enableGameOptimizations = prefs.gameOptimizations;
            break;
        }
    }

    try {
        NvHTTP http(m_Computer->activeAddress);
        if (m_Computer->currentGameId != 0) {
            http.resumeApp(&m_StreamConfig);
        }
        else {
            http.launchApp(m_App.id, &m_StreamConfig,
                           enableGameOptimizations,
                           prefs.playAudioOnHost,
                           inputHandler.getAttachedGamepadMask());
        }
    } catch (const GfeHttpResponseException& e) {
        emit displayLaunchError(e.toQString());
        s_ActiveSessionSemaphore.release();
        return;
    }

    SDL_assert(!SDL_WasInit(SDL_INIT_VIDEO));
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        emit displayLaunchError(QString::fromLocal8Bit(SDL_GetError()));
        s_ActiveSessionSemaphore.release();
        return;
    }

    QByteArray hostnameStr = m_Computer->activeAddress.toLatin1();
    QByteArray siAppVersion = m_Computer->appVersion.toLatin1();

    SERVER_INFORMATION hostInfo;
    hostInfo.address = hostnameStr.data();
    hostInfo.serverInfoAppVersion = siAppVersion.data();

    // Older GFE versions didn't have this field
    QByteArray siGfeVersion;
    if (!m_Computer->gfeVersion.isNull()) {
        siGfeVersion = m_Computer->gfeVersion.toLatin1();
    }
    if (!siGfeVersion.isNull()) {
        hostInfo.serverInfoGfeVersion = siGfeVersion.data();
    }

    int err = LiStartConnection(&hostInfo, &m_StreamConfig, &k_ConnCallbacks,
                                &m_VideoCallbacks, &k_AudioCallbacks,
                                NULL, 0, NULL, 0);
    if (err != 0) {
        // We already displayed an error dialog in the stage failure
        // listener.
        s_ActiveSessionSemaphore.release();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    // Pump the message loop to update the UI
    emit connectionStarted();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    int x, y, width, height;
    getWindowDimensions(m_Preferences.fullScreen,
                        x, y, width, height);

    m_Window = SDL_CreateWindow("Moonlight",
                                x,
                                y,
                                width,
                                height,
                                m_Preferences.fullScreen ?
                                    SDL_OS_FULLSCREEN_FLAG : 0);
    if (!m_Window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateWindow() failed: %s",
                     SDL_GetError());
        s_ActiveSessionSemaphore.release();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    // For non-full screen windows, call getWindowDimensions()
    // again after creating a window to allow it to account
    // for window chrome size.
    if (!m_Preferences.fullScreen) {
        getWindowDimensions(false, x, y, width, height);

        SDL_SetWindowPosition(m_Window, x, y);
        SDL_SetWindowSize(m_Window, width, height);

        // Passing SDL_WINDOW_RESIZABLE to set this during window
        // creation causes our window to be full screen for some reason
        SDL_SetWindowResizable(m_Window, SDL_TRUE);
    }

    QSvgRenderer svgIconRenderer(QString(":/res/moonlight.svg"));
    QImage svgImage(ICON_SIZE, ICON_SIZE, QImage::Format_RGBA8888);
    svgImage.fill(0);

    QPainter svgPainter(&svgImage);
    svgIconRenderer.render(&svgPainter);
    SDL_Surface* iconSurface = SDL_CreateRGBSurfaceWithFormatFrom((void*)svgImage.constBits(),
                                                                  svgImage.width(),
                                                                  svgImage.height(),
                                                                  32,
                                                                  4 * svgImage.width(),
                                                                  SDL_PIXELFORMAT_RGBA32);
#ifndef Q_OS_DARWIN
    // Other platforms seem to preserve our Qt icon when creating a new window
    if (iconSurface != nullptr) {
        SDL_SetWindowIcon(m_Window, iconSurface);
    }
#endif

#ifndef QT_DEBUG
    // Capture the mouse by default on release builds only.
    // This prevents the mouse from becoming trapped inside
    // Moonlight when it's halted at a debug break.
    SDL_SetRelativeMouseMode(SDL_TRUE);
#endif

    // Stop text input. SDL enables it by default
    // when we initialize the video subsystem, but this
    // causes an IME popup when certain keys are held down
    // on macOS.
    SDL_StopTextInput();

    // Disable the screen saver
    SDL_DisableScreenSaver();

    // Raise the priority of the main thread, since it handles
    // time-sensitive video rendering
    if (SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH) < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Unable to set main thread to high priority: %s",
                    SDL_GetError());
    }

    // Hijack this thread to be the SDL main thread. We have to do this
    // because we want to suspend all Qt processing until the stream is over.
    SDL_Event event;
    for (;;) {
        // We explicitly use SDL_PollEvent() and SDL_Delay() because
        // SDL_WaitEvent() has an internal SDL_Delay(10) inside which
        // blocks this thread too long for high polling rate mice and high
        // refresh rate displays.
        if (!SDL_PollEvent(&event)) {
            SDL_Delay(1);
            continue;
        }
        switch (event.type) {
        case SDL_QUIT:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Quit event received");
            goto DispatchDeferredCleanup;
        case SDL_USEREVENT: {
            SDL_Event nextEvent;

            SDL_assert(event.user.code == SDL_CODE_FRAME_READY);

            // Drop any earlier frames
            while (SDL_PeepEvents(&nextEvent,
                                  1,
                                  SDL_GETEVENT,
                                  SDL_USEREVENT,
                                  SDL_USEREVENT) == 1) {
                m_VideoDecoder->dropFrame(&event.user);
                event = nextEvent;
            }

            // Render the last frame
            m_VideoDecoder->renderFrame(&event.user);
            break;
        }

        case SDL_WINDOWEVENT:
            // We want to recreate the decoder for resizes (full-screen toggles) and the initial shown event.
            // We use SDL_WINDOWEVENT_SIZE_CHANGED rather than SDL_WINDOWEVENT_RESIZED because the latter doesn't
            // seem to fire when switching from windowed to full-screen on X11.
            if (event.window.event != SDL_WINDOWEVENT_SIZE_CHANGED && event.window.event != SDL_WINDOWEVENT_SHOWN) {
                break;
            }

            // Fall through
        case SDL_RENDER_DEVICE_RESET:
        case SDL_RENDER_TARGETS_RESET:
            SDL_AtomicLock(&m_DecoderLock);

            // Destroy the old decoder
            delete m_VideoDecoder;

            // Chose a new decoder (hopefully the same one, but possibly
            // not if a GPU was removed or something).
            if (!chooseDecoder(m_Preferences.videoDecoderSelection,
                               m_Window, m_ActiveVideoFormat, m_ActiveVideoWidth,
                               m_ActiveVideoHeight, m_ActiveVideoFrameRate,
                               s_ActiveSession->m_VideoDecoder)) {
                SDL_AtomicUnlock(&m_DecoderLock);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to recreate decoder after reset");
                goto DispatchDeferredCleanup;
            }

            // Request an IDR frame to complete the reset
            m_NeedsIdr = true;

            SDL_AtomicUnlock(&m_DecoderLock);
            break;

        case SDL_KEYUP:
        case SDL_KEYDOWN:
            inputHandler.handleKeyEvent(&event.key);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            inputHandler.handleMouseButtonEvent(&event.button);
            break;
        case SDL_MOUSEMOTION:
            inputHandler.handleMouseMotionEvent(&event.motion);
            break;
        case SDL_MOUSEWHEEL:
            inputHandler.handleMouseWheelEvent(&event.wheel);
            break;
        case SDL_CONTROLLERAXISMOTION:
            inputHandler.handleControllerAxisEvent(&event.caxis);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            inputHandler.handleControllerButtonEvent(&event.cbutton);
            break;
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
            inputHandler.handleControllerDeviceEvent(&event.cdevice);
            break;
        }
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "SDL_WaitEvent() failed: %s",
                 SDL_GetError());

DispatchDeferredCleanup:
    // Uncapture the mouse and hide the window immediately,
    // so we can return to the Qt GUI ASAP.
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_EnableScreenSaver();

    // Destroy the decoder, since this must be done on the main thread
    SDL_AtomicLock(&m_DecoderLock);
    delete m_VideoDecoder;
    m_VideoDecoder = nullptr;
    SDL_AtomicUnlock(&m_DecoderLock);

    SDL_DestroyWindow(m_Window);
    if (iconSurface != nullptr) {
        SDL_FreeSurface(iconSurface);
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_assert(!SDL_WasInit(SDL_INIT_VIDEO));

    // Cleanup can take a while, so dispatch it to a worker thread.
    // When it is complete, it will release our s_ActiveSessionSemaphore
    // reference.
    QThreadPool::globalInstance()->start(new DeferredSessionCleanupTask());
}

