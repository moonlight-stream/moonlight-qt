#include "session.hpp"
#include "settings/streamingpreferences.h"
#include "streaming/streamutils.h"

#include <Limelight.h>
#include <SDL.h>
#include "utils.h"

// HACK: Need to call SetThreadExecutionState() because SDL doesn't
#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#ifdef HAVE_FFMPEG
#include "video/ffmpeg.h"
#endif

#ifdef HAVE_SLVIDEO
#include "video/sl.h"
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
    Session::arInit,
    nullptr,
    nullptr,
    Session::arCleanup,
    Session::arDecodeAndPlaySample,
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
    emit s_ActiveSession->displayLaunchError("Connection terminated");

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

#define CALL_INITIALIZE(dec) (dec)->initialize(vds, window, videoFormat, width, height, frameRate, enableVsync)

bool Session::chooseDecoder(StreamingPreferences::VideoDecoderSelection vds,
                            SDL_Window* window, int videoFormat, int width, int height,
                            int frameRate, bool enableVsync, IVideoDecoder*& chosenDecoder)
{
#ifdef HAVE_SLVIDEO
    chosenDecoder = new SLVideoDecoder();
    if (CALL_INITIALIZE(chosenDecoder)) {
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
    if (CALL_INITIALIZE(chosenDecoder)) {
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
    // some API constraints (like DXVA2). If we can't acquire it,
    // that means the decoder is about to be destroyed, so we can
    // safely return DR_OK and wait for m_NeedsIdr to be set by
    // the decoder reinitialization code.

    if (SDL_AtomicTryLock(&s_ActiveSession->m_DecoderLock)) {
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
    else {
        // Decoder is going away. Ignore anything coming in until
        // the lock is released.
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

    if (!chooseDecoder(vds, window, videoFormat, width, height, frameRate, true, decoder)) {
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

int Session::getDecoderCapabilities(StreamingPreferences::VideoDecoderSelection vds,
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

    if (!chooseDecoder(vds, window, videoFormat, width, height, frameRate, true, decoder)) {
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    SDL_DestroyWindow(window);

    int caps = decoder->getDecoderCapabilities();

    delete decoder;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    return caps;
}

Session::Session(NvComputer* computer, NvApp& app, StreamingPreferences *preferences)
    : m_Preferences(preferences ? preferences : new StreamingPreferences(this)),
      m_Computer(computer),
      m_App(app),
      m_Window(nullptr),
      m_VideoDecoder(nullptr),
      m_DecoderLock(0),
      m_NeedsIdr(false),
      m_AudioDisabled(false),
      m_DisplayOriginX(0),
      m_DisplayOriginY(0),
      m_PendingWindowedTransition(false),
      m_OpusDecoder(nullptr),
      m_AudioRenderer(nullptr),
      m_AudioRendererLock(0)
{
}

void Session::initialize()
{
    qInfo() << "Server GPU:" << m_Computer->gpuModel;
    qInfo() << "Server GFE version:" << m_Computer->gfeVersion;

    LiInitializeVideoCallbacks(&m_VideoCallbacks);
    m_VideoCallbacks.setup = drSetup;
    m_VideoCallbacks.submitDecodeUnit = drSubmitDecodeUnit;

    // Slice up to 4 times for parallel decode, once slice per core
    int slices = qMin(MAX_SLICES, SDL_GetCPUCount());
    m_VideoCallbacks.capabilities |= CAPABILITY_SLICES_PER_FRAME(slices);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Encoder configured for %d slices per frame",
                slices);

    LiInitializeStreamConfiguration(&m_StreamConfig);
    m_StreamConfig.width = m_Preferences->width;
    m_StreamConfig.height = m_Preferences->height;
    m_StreamConfig.fps = m_Preferences->fps;
    m_StreamConfig.bitrate = m_Preferences->bitrateKbps;
    m_StreamConfig.hevcBitratePercentageMultiplier = 75;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Video bitrate: %d kbps",
                m_StreamConfig.bitrate);

    RAND_bytes(reinterpret_cast<unsigned char*>(m_StreamConfig.remoteInputAesKey),
               sizeof(m_StreamConfig.remoteInputAesKey));

    // Only the first 4 bytes are populated in the RI key IV
    RAND_bytes(reinterpret_cast<unsigned char*>(m_StreamConfig.remoteInputAesIv), 4);

    switch (m_Preferences->audioConfig)
    {
    case StreamingPreferences::AC_AUTO:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Autodetecting audio configuration");
        m_StreamConfig.audioConfiguration = detectAudioConfiguration();
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

    switch (m_Preferences->videoCodecConfig)
    {
    case StreamingPreferences::VCC_AUTO:
        // TODO: Determine if HEVC is better depending on the decoder
        m_StreamConfig.supportsHevc =
                isHardwareDecodeAvailable(m_Preferences->videoDecoderSelection,
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

    if (m_Computer->activeAddress == m_Computer->remoteAddress) {
        m_StreamConfig.streamingRemotely = 1;
    }
    else {
        m_StreamConfig.streamingRemotely = 0;
    }

    if (m_Computer->activeAddress == m_Computer->localAddress) {
        m_StreamConfig.packetSize = 1392;
    }
    else {
        m_StreamConfig.packetSize = 1024;
    }

    switch (m_Preferences->windowMode)
    {
    case StreamingPreferences::WM_FULLSCREEN_DESKTOP:
        m_FullScreenFlag = SDL_WINDOW_FULLSCREEN_DESKTOP;
        break;
    case StreamingPreferences::WM_FULLSCREEN:
    default:
        m_FullScreenFlag = SDL_WINDOW_FULLSCREEN;
        break;
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

    if (m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_FORCE_SOFTWARE) {
        emitLaunchWarning("Your settings selection to force software decoding may cause poor streaming performance.");
    }

    if (m_Preferences->unsupportedFps && m_StreamConfig.fps > 60) {
        emitLaunchWarning("Using unsupported FPS options may cause stuttering or lag.");

        if (m_Preferences->enableVsync) {
            emitLaunchWarning("V-sync will be disabled when streaming at a higher frame rate than the display.");
        }
    }

    if (m_StreamConfig.supportsHevc) {
        bool hevcForced = m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC ||
                m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC_HDR;

        if (!isHardwareDecodeAvailable(m_Preferences->videoDecoderSelection,
                                       VIDEO_FORMAT_H265,
                                       m_StreamConfig.width,
                                       m_StreamConfig.height,
                                       m_StreamConfig.fps) &&
                m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_AUTO) {
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
        else if (!isHardwareDecodeAvailable(m_Preferences->videoDecoderSelection,
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
        if (m_Computer->gfeVersion.isEmpty() || m_Computer->gfeVersion.startsWith("2.")) {
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

    // Test if audio works at the specified audio configuration
    bool audioTestPassed = testAudio(m_StreamConfig.audioConfiguration);

    // Gracefully degrade to stereo if 5.1 doesn't work
    if (!audioTestPassed && m_StreamConfig.audioConfiguration == AUDIO_CONFIGURATION_51_SURROUND) {
        audioTestPassed = testAudio(AUDIO_CONFIGURATION_STEREO);
        if (audioTestPassed) {
            m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
            emitLaunchWarning("5.1 surround sound is not supported by the current audio device.");
        }
    }

    // If nothing worked, warn the user that audio will not work
    m_AudioDisabled = !audioTestPassed;
    if (m_AudioDisabled) {
        emitLaunchWarning("Failed to open audio device. Audio will be unavailable during this session.");
    }

    if (m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_FORCE_HARDWARE &&
            !isHardwareDecodeAvailable(m_Preferences->videoDecoderSelection,
                                       m_StreamConfig.supportsHevc ? VIDEO_FORMAT_H265 : VIDEO_FORMAT_H264,
                                       m_StreamConfig.width,
                                       m_StreamConfig.height,
                                       m_StreamConfig.fps)) {
        if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_AUTO) {
            emit displayLaunchError("Your selection to force hardware decoding cannot be satisfied due to missing hardware decoding support on this PC's GPU.");
        }
        else {
            emit displayLaunchError("Your codec selection and force hardware decoding setting are not compatible. This PC's GPU lacks support for decoding your chosen codec.");
        }

        // Fail the launch, because we won't manage to get a decoder for the actual stream
        return false;
    }

    // Add the capability flags from the chosen decoder/renderer
    m_VideoCallbacks.capabilities |= getDecoderCapabilities(m_Preferences->videoDecoderSelection,
                                                            m_StreamConfig.supportsHevc ? VIDEO_FORMAT_H265 : VIDEO_FORMAT_H264,
                                                            m_StreamConfig.width,
                                                            m_StreamConfig.height,
                                                            m_StreamConfig.fps);

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

void Session::getWindowDimensions(int& x, int& y,
                                  int& width, int& height)
{
    int displayIndex = 0;

    if (m_Window != nullptr) {
        displayIndex = SDL_GetWindowDisplayIndex(m_Window);
        SDL_assert(displayIndex >= 0);
    }
    // Create our window on the same display that Qt's UI
    // was being displayed on.
    else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Qt UI screen is at (%d,%d)",
                    m_DisplayOriginX, m_DisplayOriginY);
        for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
            SDL_Rect displayBounds;

            if (SDL_GetDisplayBounds(i, &displayBounds) == 0) {
                if (displayBounds.x == m_DisplayOriginX &&
                        displayBounds.y == m_DisplayOriginY) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "SDL found matching display %d",
                                i);
                    displayIndex = i;
                    break;
                }
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "SDL_GetDisplayBounds(%d) failed: %s",
                            i, SDL_GetError());
            }
        }
    }

    SDL_Rect usableBounds;
    if (SDL_GetDisplayUsableBounds(displayIndex, &usableBounds) == 0) {
        width = usableBounds.w;
        height = usableBounds.h;

        if (m_Window != nullptr) {
            int top, left, bottom, right;

            if (SDL_GetWindowBordersSize(m_Window, &top, &left, &bottom, &right) == 0) {
                width -= left + right;
                height -= top + bottom;
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Unable to get window border size: %s",
                            SDL_GetError());
            }

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
    }

    x = y = SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex);
}

void Session::updateOptimalWindowDisplayMode()
{
    SDL_DisplayMode desktopMode, bestMode, mode;
    int displayIndex = SDL_GetWindowDisplayIndex(m_Window);

    // Try the current display mode first. On macOS, this will be the normal
    // scaled desktop resolution setting.
    if (SDL_GetDesktopDisplayMode(displayIndex, &desktopMode) == 0) {
        // If this doesn't fit the selected resolution, use the native
        // resolution of the panel (unscaled).
        if (desktopMode.w < m_ActiveVideoWidth || desktopMode.h < m_ActiveVideoHeight) {
            if (!StreamUtils::getRealDesktopMode(displayIndex, &desktopMode)) {
                return;
            }
        }
    }
    else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL_GetDesktopDisplayMode() failed: %s",
                    SDL_GetError());
        return;
    }

    // Start with the native desktop resolution and try to find
    // the highest refresh rate that our stream FPS evenly divides.
    bestMode = desktopMode;
    bestMode.refresh_rate = 0;
    for (int i = 0; i < SDL_GetNumDisplayModes(displayIndex); i++) {
        if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0) {
            if (mode.w == desktopMode.w && mode.h == desktopMode.h &&
                    mode.refresh_rate % m_StreamConfig.fps == 0) {
                if (mode.refresh_rate > bestMode.refresh_rate) {
                    bestMode = mode;
                }
            }
        }
    }

    if (bestMode.refresh_rate == 0) {
        // We may find no match if the user has moved a 120 FPS
        // stream onto a 60 Hz monitor (since no refresh rate can
        // divide our FPS setting). We'll stick to the default in
        // this case.
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "No matching refresh rate found; using desktop mode");
        bestMode = desktopMode;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Chosen best display mode: %dx%dx%d",
                bestMode.w, bestMode.h, bestMode.refresh_rate);

    SDL_SetWindowDisplayMode(m_Window, &bestMode);
}

void Session::toggleFullscreen()
{
    bool fullScreen = !(SDL_GetWindowFlags(m_Window) & m_FullScreenFlag);

    if (fullScreen) {
        SDL_SetWindowResizable(m_Window, SDL_FALSE);
        SDL_SetWindowFullscreen(m_Window, m_FullScreenFlag);
    }
    else {
        SDL_SetWindowFullscreen(m_Window, 0);
        SDL_SetWindowResizable(m_Window, SDL_TRUE);

        // Reposition the window when the resize is complete
        m_PendingWindowedTransition = true;
    }
}

void Session::exec(int displayOriginX, int displayOriginY)
{
    m_DisplayOriginX = displayOriginX;
    m_DisplayOriginY = displayOriginY;

    // Complete initialization in this deferred context to avoid
    // calling expensive functions in the constructor (during the
    // process of loading the StreamSegue).
    initialize();

    // Check for validation errors/warnings and emit
    // signals for them, if appropriate
    if (!validateLaunch()) {
        return;
    }

    // Wait 1.5 seconds before connecting to let the user
    // have time to read any messages present on the segue
    uint32_t start = SDL_GetTicks();
    while (!SDL_TICKS_PASSED(SDL_GetTicks(), start + 1500)) {
        // Pump the UI loop while we wait
        SDL_Delay(5);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // Wait for any old session to finish cleanup
    s_ActiveSessionSemaphore.acquire();

    // We're now active
    s_ActiveSession = this;

    // Initialize the gamepad code with our preferences
    StreamingPreferences prefs;
    SdlInputHandler inputHandler(prefs, m_Computer);

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
    } catch (const QtNetworkReplyException& e) {
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
    if (!m_Computer->gfeVersion.isEmpty()) {
        siGfeVersion = m_Computer->gfeVersion.toLatin1();
    }
    if (!siGfeVersion.isEmpty()) {
        hostInfo.serverInfoGfeVersion = siGfeVersion.data();
    }

    int err = LiStartConnection(&hostInfo, &m_StreamConfig, &k_ConnCallbacks,
                                &m_VideoCallbacks,
                                m_AudioDisabled ? nullptr : &k_AudioCallbacks,
                                NULL, 0, NULL, 0);
    if (err != 0) {
        // We already displayed an error dialog in the stage failure
        // listener.
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        s_ActiveSessionSemaphore.release();
        return;
    }

    // Pump the message loop to update the UI
    emit connectionStarted();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    int x, y, width, height;
    getWindowDimensions(x, y, width, height);

    m_Window = SDL_CreateWindow("Moonlight",
                                x,
                                y,
                                width,
                                height,
                                SDL_WINDOW_ALLOW_HIGHDPI);
    if (!m_Window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateWindow() failed: %s",
                     SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        s_ActiveSessionSemaphore.release();
        return;
    }

    // For non-full screen windows, call getWindowDimensions()
    // again after creating a window to allow it to account
    // for window chrome size.
    if (m_Preferences->windowMode == StreamingPreferences::WM_WINDOWED) {
        getWindowDimensions(x, y, width, height);

        // We must set the size before the position because centering
        // won't work unless it knows the final size of the window.
        SDL_SetWindowSize(m_Window, width, height);
        SDL_SetWindowPosition(m_Window, x, y);

        // Passing SDL_WINDOW_RESIZABLE to set this during window
        // creation causes our window to be full screen for some reason
        SDL_SetWindowResizable(m_Window, SDL_TRUE);
    }
    else {
        // Update the window display mode based on our current monitor
        updateOptimalWindowDisplayMode();

        // Enter full screen
        SDL_SetWindowFullscreen(m_Window, m_FullScreenFlag);
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
    if (m_Preferences->windowMode != StreamingPreferences::WM_WINDOWED) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
#endif

    // Stop text input. SDL enables it by default
    // when we initialize the video subsystem, but this
    // causes an IME popup when certain keys are held down
    // on macOS.
    SDL_StopTextInput();

    // Disable the screen saver
    SDL_DisableScreenSaver();

#ifdef Q_OS_WIN32
    // HACK: SDL doesn't call this, so we must do so to disable the
    // screensaver when we're in windowed mode. DirectX will disable
    // it for us when we're in full-screen exclusive mode.
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);
#endif

    // Set timer resolution to 1 ms on Windows for greater
    // sleep precision and more accurate callback timing.
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    // Raise the priority of the main thread, since it handles
    // time-sensitive video rendering
    if (SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH) < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Unable to set main thread to high priority: %s",
                    SDL_GetError());
    }

    int currentDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);

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
            // Capture mouse cursor when user actives the window by clicking on
            // window's client area (borders and title bar excluded).
            // Without this you would have to click the window twice (once to
            // activate it, second time to enable capture). With this you need to
            // click it only once.
            // On Linux, the button press event is delivered after the focus gain
            // so this is not neccessary (and leads to a click sent to the host
            // when focusing the window by clicking).
            // By excluding window's borders and title bar out, lets user still
            // interact with them without mouse capture kicking in.
#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN)
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                int mouseX, mouseY;
                Uint32 mouseState = SDL_GetGlobalMouseState(&mouseX, &mouseY);
                if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                    int x, y, width, height;
                    SDL_GetWindowPosition(m_Window, &x, &y);
                    SDL_GetWindowSize(m_Window, &width, &height);
                    if (mouseX > x && mouseX < x+width && mouseY > y && mouseY < y+height) {
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                    }
                }
            }
#endif

            // Release mouse cursor when another window is activated (e.g. by using ALT+TAB).
            // This lets user to interact with our window's title bar and with the buttons in it.
            // Doing this while the window is full-screen breaks the transition out of FS
            // (desktop and exclusive), so we must check for that before releasing mouse capture.
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST &&
                    !(SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN)) {
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }

            // We want to recreate the decoder for resizes (full-screen toggles) and the initial shown event.
            // We use SDL_WINDOWEVENT_SIZE_CHANGED rather than SDL_WINDOWEVENT_RESIZED because the latter doesn't
            // seem to fire when switching from windowed to full-screen on X11.
            if (event.window.event != SDL_WINDOWEVENT_SIZE_CHANGED && event.window.event != SDL_WINDOWEVENT_SHOWN) {
                // Check that the window display hasn't changed. If it has, we want
                // to recreate the decoder to allow it to adapt to the new display.
                // This will allow Pacer to pull the new display refresh rate.
                if (SDL_GetWindowDisplayIndex(m_Window) == currentDisplayIndex) {
                    break;
                }
            }

            // Complete any repositioning that was deferred until
            // the resize from full-screen to windowed had completed.
            // If we try to do this immediately, the resize won't take effect
            // properly on Windows.
            if (m_PendingWindowedTransition) {
                m_PendingWindowedTransition = false;

                int x, y, width, height;
                getWindowDimensions(x, y, width, height);

                SDL_SetWindowSize(m_Window, width, height);
                SDL_SetWindowPosition(m_Window, x, y);
            }

            // Fall through
        case SDL_RENDER_DEVICE_RESET:
        case SDL_RENDER_TARGETS_RESET:

            SDL_AtomicLock(&m_DecoderLock);

            // Destroy the old decoder
            delete m_VideoDecoder;

            // Flush any other pending window events that could
            // send us back here immediately
            SDL_PumpEvents();
            SDL_FlushEvent(SDL_WINDOWEVENT);

            // Update the window display mode based on our current monitor
            currentDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);
            updateOptimalWindowDisplayMode();

            // Now that the old decoder is dead, flush any events it may
            // have queued to reset itself (if this reset was the result
            // of state loss).
            SDL_PumpEvents();
            SDL_FlushEvent(SDL_RENDER_DEVICE_RESET);
            SDL_FlushEvent(SDL_RENDER_TARGETS_RESET);

            {
                // If the stream exceeds the display refresh rate (plus some slack),
                // forcefully disable V-sync to allow the stream to render faster
                // than the display.
                int displayHz = StreamUtils::getDisplayRefreshRate(m_Window);
                bool enableVsync = m_Preferences->enableVsync;
                if (displayHz + 5 < m_StreamConfig.fps) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Disabling V-sync because refresh rate limit exceeded");
                    enableVsync = false;
                }

                // Choose a new decoder (hopefully the same one, but possibly
                // not if a GPU was removed or something).
                if (!chooseDecoder(m_Preferences->videoDecoderSelection,
                                   m_Window, m_ActiveVideoFormat, m_ActiveVideoWidth,
                                   m_ActiveVideoHeight, m_ActiveVideoFrameRate,
                                   enableVsync,
                                   s_ActiveSession->m_VideoDecoder)) {
                    SDL_AtomicUnlock(&m_DecoderLock);
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "Failed to recreate decoder after reset");
                    emit displayLaunchError("Unable to initialize video decoder. Please check your streaming settings and try again.");
                    goto DispatchDeferredCleanup;
                }
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
#ifdef Q_OS_WIN32
    // HACK: See comment above
    SetThreadExecutionState(ES_CONTINUOUS);
#endif

    // Uncapture the mouse and hide the window immediately,
    // so we can return to the Qt GUI ASAP.
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_EnableScreenSaver();
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "0");

    // Destroy the decoder, since this must be done on the main thread
    SDL_AtomicLock(&m_DecoderLock);
    delete m_VideoDecoder;
    m_VideoDecoder = nullptr;
    SDL_AtomicUnlock(&m_DecoderLock);

    // Destroy the audio renderer which must also be done on the main thread
    cleanupAudioRendererOnMainThread();

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

