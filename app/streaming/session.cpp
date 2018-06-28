#include "session.hpp"
#include "settings/streamingpreferences.h"

#include <Limelight.h>
#include <SDL.h>

#include <QRandomGenerator>
#include <QMessageBox>
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

DECODER_RENDERER_CALLBACKS Session::k_VideoCallbacks;

Session* Session::s_ActiveSession;

void Session::clStageStarting(int stage)
{
    char buffer[512];
    sprintf(buffer, "Starting %s...", LiGetStageName(stage));

    // We know this is called on the same thread as LiStartConnection()
    // which happens to be the main thread, so it's cool to interact
    // with the GUI in these callbacks.
    s_ActiveSession->m_ProgressBox.setText(buffer);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void Session::clStageFailed(int stage, long errorCode)
{
    s_ActiveSession->m_ProgressBox.close();

    // TODO: Open error dialog
    char buffer[512];
    sprintf(buffer, "Failed %s with error: %ld",
            LiGetStageName(stage), errorCode);

    // We know this is called on the same thread as LiStartConnection()
    // which happens to be the main thread, so it's cool to interact
    // with the GUI in these callbacks.
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
      m_App(app),
      m_ProgressBox(nullptr)
{
    StreamingPreferences prefs;

    LiInitializeStreamConfiguration(&m_StreamConfig);
    m_StreamConfig.width = prefs.width;
    m_StreamConfig.height = prefs.height;
    m_StreamConfig.fps = prefs.fps;
    m_StreamConfig.bitrate = prefs.bitrateKbps;
    m_StreamConfig.packetSize = 1024;
    m_StreamConfig.hevcBitratePercentageMultiplier = 75;
    for (int i = 0; i < sizeof(m_StreamConfig.remoteInputAesKey); i++) {
        m_StreamConfig.remoteInputAesKey[i] =
                (char)(QRandomGenerator::global()->generate() % 256);
    }
    *(int*)m_StreamConfig.remoteInputAesIv = qToBigEndian(QRandomGenerator::global()->generate());
    switch (prefs.audioConfig)
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
    switch (prefs.videoCodecConfig)
    {
    case StreamingPreferences::VCC_AUTO:
        // TODO: Determine if HEVC is better depending on the decoder
        m_StreamConfig.supportsHevc = true;
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

QString Session::checkForFatalValidationError()
{
    // Nothing here yet
    return nullptr;
}

QStringList Session::checkForAdvisoryValidationError()
{
    NvHTTP http(m_Computer->activeAddress);
    QStringList warningList;

    if (m_StreamConfig.enableHdr) {
        // Turn HDR back off unless all criteria are met.
        m_StreamConfig.enableHdr = false;

        // Check that the app supports HDR
        if (!m_App.hdrSupported) {
            warningList.append(m_App.name + " doesn't support HDR10.");
        }
        // Check that the server GPU supports HDR
        else if (!(m_Computer->serverCodecModeSupport & 0x200)) {
            warningList.append("Your host PC GPU doesn't support HDR streaming. "
                               "A GeForce GTX 1000-series (Pascal) or later GPU is required for HDR streaming.");
        }
        else {
            // TODO: Also validate client decoder and display capabilites

            // Validation successful so HDR is good to go
            m_StreamConfig.enableHdr = true;
        }
    }

    if (m_StreamConfig.width >= 3840) {
        if (m_StreamConfig.fps >= 60) {
            // TODO: Validate 4K60 support based on serverinfo
        }
        else {
            // TODO: Validate 4K30 support based on serverinfo
        }
    }

    if (m_StreamConfig.supportsHevc) {
        // TODO: Validate HEVC support based on decoder caps
    }

    return warningList;
}

void Session::exec()
{
    // We're now active
    s_ActiveSession = this;

    // Initialize the gamepad code with our preferences
    StreamingPreferences prefs;
    SdlInputHandler inputHandler(prefs.multiController);

    m_ProgressBox.setStandardButtons(QMessageBox::Cancel);
    m_ProgressBox.setText("Launching "+m_App.name+"...");
    m_ProgressBox.open();

    // Ensure the progress box is immediately visible
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    NvHTTP http(m_Computer->activeAddress);

    // The UI should have ensured the old game was already quit
    // if we decide to stream a different game.
    Q_ASSERT(m_Computer->currentGameId == 0 ||
             m_Computer->currentGameId == m_App.id);

    if (m_Computer->currentGameId != 0) {
        http.resumeApp(&m_StreamConfig);
    }
    else {
        http.launchApp(m_App.id, &m_StreamConfig,
                       prefs.enableGameOptimizations,
                       prefs.playAudioOnHost,
                       inputHandler.getAttachedGamepadMask());
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
                                &k_VideoCallbacks, &k_AudioCallbacks,
                                NULL, 0, NULL, 0);
    if (err != 0) {
        // We already displayed an error dialog in the stage failure
        // listener.
        return;
    }

    // Before we get into our SDL loop, close the message box used to
    // display progress
    m_ProgressBox.close();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    //SDL_CreateWindow("SDL", 0, 0, 1280, 720, SDL_WINDOW_INPUT_GRABBED);

    // Hijack this thread to be the SDL main thread. We have to do this
    // because we want to suspend all Qt processing until the stream is over.
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Quit event received");
            goto Exit;
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
}
