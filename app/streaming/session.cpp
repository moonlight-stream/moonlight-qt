#include "session.hpp"
#include "settings/streamingpreferences.h"

#include <Limelight.h>
#include <SDL.h>
#include "utils.h"

#include <QRandomGenerator>
#include <QtEndian>
#include <QCoreApplication>

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

Session::Session(NvComputer* computer, NvApp& app)
    : m_Computer(computer),
      m_App(app)
{
    LiInitializeVideoCallbacks(&m_VideoCallbacks);
    m_VideoCallbacks.setup = drSetup;
    m_VideoCallbacks.cleanup = drCleanup;
    m_VideoCallbacks.submitDecodeUnit = drSubmitDecodeUnit;
    m_VideoCallbacks.capabilities = getDecoderCapabilities();

    LiInitializeStreamConfiguration(&m_StreamConfig);
    m_StreamConfig.width = m_Preferences.width;
    m_StreamConfig.height = m_Preferences.height;
    m_StreamConfig.fps = m_Preferences.fps;
    m_StreamConfig.bitrate = m_Preferences.bitrateKbps;
    m_StreamConfig.packetSize = 1024;
    m_StreamConfig.hevcBitratePercentageMultiplier = 75;
    for (unsigned int i = 0; i < sizeof(m_StreamConfig.remoteInputAesKey); i++) {
        m_StreamConfig.remoteInputAesKey[i] =
                (char)(QRandomGenerator::global()->generate() % 256);
    }
    *(int*)m_StreamConfig.remoteInputAesIv = qToBigEndian(QRandomGenerator::global()->generate());
    switch (m_Preferences.audioConfig)
    {
    case StreamingPreferences::AC_AUTO:
        m_StreamConfig.audioConfiguration = sdlDetermineAudioConfiguration();
        break;
    case StreamingPreferences::AC_FORCE_STEREO:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
        break;
    case StreamingPreferences::AC_FORCE_SURROUND:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_51_SURROUND;
        break;
    }
    switch (m_Preferences.videoCodecConfig)
    {
    case StreamingPreferences::VCC_AUTO:
        // TODO: Determine if HEVC is better depending on the decoder
        // NOTE: HEVC currently uses only 1 slice regardless of what
        // we provide in CAPABILITY_SLICES_PER_FRAME(), so we should
        // never use it for software decoding (unless common-c starts
        // respecting it for HEVC).
        m_StreamConfig.supportsHevc = false;
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
}

bool Session::validateLaunch()
{
    QStringList warningList;

    if (m_StreamConfig.supportsHevc) {
        if (m_Preferences.videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC ||
                m_Preferences.videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC_HDR) {
            if (m_Computer->maxLumaPixelsHEVC == 0) {
                emit displayLaunchWarning("Your host PC GPU doesn't support HEVC. "
                                          "A GeForce GTX 900-series (Maxwell) or later GPU is required for HEVC streaming.");
            }
        }

        // TODO: Validate HEVC support based on decoder caps
    }

    if (m_StreamConfig.enableHdr) {
        // Turn HDR back off unless all criteria are met.
        m_StreamConfig.enableHdr = false;

        // Check that the app supports HDR
        if (!m_App.hdrSupported) {
            emit displayLaunchWarning(m_App.name + " doesn't support HDR10.");
        }
        // Check that the server GPU supports HDR
        else if (!(m_Computer->serverCodecModeSupport & 0x200)) {
            emit displayLaunchWarning("Your host PC GPU doesn't support HDR streaming. "
                                      "A GeForce GTX 1000-series (Pascal) or later GPU is required for HDR streaming.");
        }
        else {
            // TODO: Also validate client decoder and display capabilites

            // Validation successful so HDR is good to go
            m_StreamConfig.enableHdr = true;
        }
    }

    if (m_StreamConfig.width >= 3840) {
        // Only allow 4K on GFE 3.x+
        if (m_Computer->gfeVersion.isNull() || m_Computer->gfeVersion.startsWith("2.")) {
            emit displayLaunchWarning("GeForce Experience 3.0 or higher is required for 4K streaming.");

            m_StreamConfig.width = 1920;
            m_StreamConfig.height = 1080;
        }
        // This list is sorted from least to greatest
        else if (m_Computer->displayModes.last().width < 3840 ||
                 (m_Computer->displayModes.last().refreshRate < 60 && m_StreamConfig.fps >= 60)) {
            emit displayLaunchWarning("Your host PC GPU doesn't support 4K streaming. "
                                      "A GeForce GTX 900-series (Maxwell) or later GPU is required for 4K streaming.");

            m_StreamConfig.width = 1920;
            m_StreamConfig.height = 1080;
        }
    }

    // Always allow the launch to proceed for now
    return true;
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

    // We're now active
    s_ActiveSession = this;

    // Initialize the gamepad code with our preferences
    StreamingPreferences prefs;
    SdlInputHandler inputHandler(prefs.multiController);

    // The UI should have ensured the old game was already quit
    // if we decide to stream a different game.
    Q_ASSERT(m_Computer->currentGameId == 0 ||
             m_Computer->currentGameId == m_App.id);

    try {
        NvHTTP http(m_Computer->activeAddress);
        if (m_Computer->currentGameId != 0) {
            http.resumeApp(&m_StreamConfig);
        }
        else {
            http.launchApp(m_App.id, &m_StreamConfig,
                           prefs.gameOptimizations,
                           prefs.playAudioOnHost,
                           inputHandler.getAttachedGamepadMask());
        }
    } catch (const GfeHttpResponseException& e) {
        emit displayLaunchError(e.toQString());
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
        return;
    }

    // Pump the message loop to update the UI
    emit connectionStarted();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    m_Window = SDL_CreateWindow("Moonlight",
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                m_StreamConfig.width,
                                m_StreamConfig.height,
                                (m_Preferences.fullScreen ?
                                    SDL_WINDOW_FULLSCREEN :
                                    0));
    if (!m_Window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateWindow() failed: %s",
                     SDL_GetError());
        LiStopConnection();
        return;
    }

    m_Renderer = SDL_CreateRenderer(m_Window, -1,
                                    SDL_RENDERER_ACCELERATED);
    if (!m_Renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
        LiStopConnection();
        SDL_DestroyWindow(m_Window);
        return;
    }

    m_Texture = SDL_CreateTexture(m_Renderer,
                                  SDL_PIXELFORMAT_YV12,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  m_StreamConfig.width,
                                  m_StreamConfig.height);
    if (!m_Texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
        LiStopConnection();
        SDL_DestroyRenderer(m_Renderer);
        SDL_DestroyWindow(m_Window);
        return;
    }

    // Capture the mouse
    SDL_SetRelativeMouseMode(SDL_TRUE);

    // Hijack this thread to be the SDL main thread. We have to do this
    // because we want to suspend all Qt processing until the stream is over.
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Quit event received");
            goto Exit;
        case SDL_USEREVENT: {
            SDL_Event nextEvent;

            SDL_assert(event.user.code == SDL_CODE_FRAME_READY);

            // Drop any earlier frames
            while (SDL_PeepEvents(&nextEvent,
                                  1,
                                  SDL_GETEVENT,
                                  SDL_USEREVENT,
                                  SDL_USEREVENT) == 1) {
                dropFrame(&event.user);
                event = nextEvent;
            }

            // Render the last frame
            renderFrame(&event.user);
            break;
        }
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

Exit:
    s_ActiveSession = nullptr;
    LiStopConnection();
    SDL_DestroyTexture(m_Texture);
    SDL_DestroyRenderer(m_Renderer);
    SDL_DestroyWindow(m_Window);
}
