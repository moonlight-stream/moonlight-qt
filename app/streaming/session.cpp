#include "session.h"
#include "clipboardhelperclient.h"
#include "filemappingclient.h"
#include "filemappingux.h"
#include "settings/streamingpreferences.h"
#include "streaming/streamutils.h"
#include "backend/richpresencemanager.h"
#include "backend/nvhttp.h"
#include "backend/identitymanager.h"

#include <Limelight.h>
#include "SDL_compat.h"
#include "network/bandwidth.h"
#include "utils.h"
#include <QCoreApplication>
#include <QHostInfo>

#ifdef HAVE_FFMPEG
#include "video/ffmpeg.h"
#endif

#ifdef HAVE_SLVIDEO
#include "video/slvid.h"
#endif

#ifdef Q_OS_WIN32
// Scaling the icon down on Win32 looks dreadful, so render at lower res
#define ICON_SIZE 32
#include <dxgi1_6.h>
#include <wrl/client.h>
#else
#define ICON_SIZE 64
#endif

#define SDL_CODE_FLUSH_WINDOW_EVENT_BARRIER 100
#define SDL_CODE_GAMECONTROLLER_RUMBLE 101
#define SDL_CODE_GAMECONTROLLER_RUMBLE_TRIGGERS 102
#define SDL_CODE_GAMECONTROLLER_SET_MOTION_EVENT_STATE 103
#define SDL_CODE_GAMECONTROLLER_SET_CONTROLLER_LED 104
#define SDL_CODE_GAMECONTROLLER_SET_ADAPTIVE_TRIGGERS 105
#define SDL_CODE_FLUSH_TOUCHPAD_FRAME 106
#define SDL_CODE_CURSOR_UPDATE 107

#include <openssl/rand.h>

#include <QtEndian>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QThreadPool>
#include <QRunnable>
#include <QReadLocker>
#include <QSvgRenderer>
#include <QPainter>
#include <QImage>
#include <QGuiApplication>
#include <QCursor>
#include <QProcess>
#include <QScreen>
#include <QtGlobal>
#include <QMutexLocker>
#include <QUuid>
#include <QElapsedTimer>
#include <QUrl>

#include <utility>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QQuickOpenGLUtils>
#endif

#define CONN_TEST_SERVER "qt.conntest.moonlight-stream.org"

CONNECTION_LISTENER_CALLBACKS Session::k_ConnCallbacks = {
    Session::clStageStarting,
    nullptr,
    Session::clStageFailed,
    nullptr,
    Session::clConnectionTerminated,
    Session::clLogMessage,
    Session::clRumble,
    Session::clConnectionStatusUpdate,
    Session::clSetHdrMode,
    Session::clRumbleTriggers,
    Session::clSetMotionEventState,
    Session::clSetControllerLED,
    Session::clSetAdaptiveTriggers,
    nullptr, // resolutionChanged (unused on Qt client)
    Session::clClipboardData,
    Session::clCursorUpdate
};

Session* Session::s_ActiveSession;
QSemaphore Session::s_ActiveSessionSemaphore(1);

class AbrFeedbackTask : public QRunnable
{
public:
    AbrFeedbackTask(NvAddress address,
                    uint16_t httpsPort,
                    QSslCertificate serverCert,
                    QString uuid,
                    double packetLoss,
                    double rttMs,
                    double decodeFps,
                    int droppedFrames,
                    std::shared_ptr<std::atomic_bool> inFlight,
                    std::shared_ptr<std::atomic_int> currentBitrateKbps)
        : m_Address(address),
          m_HttpsPort(httpsPort),
          m_ServerCert(serverCert),
          m_Uuid(uuid),
          m_PacketLoss(packetLoss),
          m_RttMs(rttMs),
          m_DecodeFps(decodeFps),
          m_DroppedFrames(droppedFrames),
          m_InFlight(inFlight),
          m_CurrentBitrateKbps(currentBitrateKbps)
    {
    }

    virtual void run() override
    {
        try {
            NvHTTP http(m_Address, m_HttpsPort, m_ServerCert, true, nullptr, m_Uuid);
            QJsonObject response = http.sendAbrFeedback(m_PacketLoss,
                                                        m_RttMs,
                                                        m_DecodeFps,
                                                        m_DroppedFrames,
                                                        m_CurrentBitrateKbps->load(),
                                                        2000);

            int newBitrate = response.value("newBitrate").toInt(0);
            if (newBitrate > 0) {
                m_CurrentBitrateKbps->store(newBitrate);
                qInfo() << "Sunshine ABR adjusted bitrate to" << newBitrate << "Kbps:" << response.value("reason").toString();
            }
        }
        catch (const std::exception& e) {
            qWarning() << "Sunshine ABR feedback failed:" << e.what();
        }

        m_InFlight->store(false);
    }

private:
    NvAddress m_Address;
    uint16_t m_HttpsPort;
    QSslCertificate m_ServerCert;
    QString m_Uuid;
    double m_PacketLoss;
    double m_RttMs;
    double m_DecodeFps;
    int m_DroppedFrames;
    std::shared_ptr<std::atomic_bool> m_InFlight;
    std::shared_ptr<std::atomic_int> m_CurrentBitrateKbps;
};

QString fileMappingStateName(OverlayMenuPanel::FileMappingState state)
{
    return FileMappingUx::stateName(state);
}

QString appendFileMappingDiagnostic(const QString& event,
                                    const QString& detail = QString(),
                                    const QString& hostUuid = QString(),
                                    const QString& sessionId = QString())
{
    return FileMappingUx::appendDiagnostic(event, detail, hostUuid, sessionId);
}

bool isFileMappingPathInsideDirectory(const QString& path, const QString& directory)
{
    const QString target = QDir::cleanPath(QFileInfo(path).canonicalFilePath());
    const QString base = QDir::cleanPath(QFileInfo(directory).canonicalFilePath());
    if (target.isEmpty() || base.isEmpty()) {
        return false;
    }

#if defined(Q_OS_WIN)
    return target.compare(base, Qt::CaseInsensitive) == 0 ||
           target.startsWith(base + QLatin1Char('/'), Qt::CaseInsensitive);
#else
    return target == base || target.startsWith(base + QLatin1Char('/'));
#endif
}

bool isMoonlightGeneratedFileMappingMirrorPath(const QString& path)
{
    if (path.isEmpty() || QFileInfo(path).fileName() == QStringLiteral("Latest")) {
        return false;
    }
    return isFileMappingPathInsideDirectory(path, FileMappingUx::diagnosticsDirectory());
}

class FileMappingSmokeTask : public QRunnable
{
public:
    FileMappingSmokeTask(NvComputer computer,
                         QString mappingId,
                         QString path,
                         quint64 offset,
                         quint32 length,
                         int timeoutMs)
        : m_Computer(std::move(computer)),
          m_MappingId(std::move(mappingId)),
          m_Path(std::move(path)),
          m_Offset(offset),
          m_Length(length),
          m_TimeoutMs(timeoutMs)
    {
    }

    virtual void run() override
    {
        FileMappingClient client(&m_Computer);
        FileMappingClient::SmokeResult result = client.smokeRead(m_MappingId,
                                                                 m_Path,
                                                                 m_Offset,
                                                                 m_Length,
                                                                 m_TimeoutMs);
        if (!result.ok) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "File mapping smoke failed: mapping=%s path=%s error=%s",
                        m_MappingId.toUtf8().constData(),
                        m_Path.toUtf8().constData(),
                        result.error.toUtf8().constData());
            return;
        }

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "File mapping smoke passed: mapping=%s path=%s",
                    m_MappingId.toUtf8().constData(),
                    m_Path.toUtf8().constData());
    }

private:
    NvComputer m_Computer;
    QString m_MappingId;
    QString m_Path;
    quint64 m_Offset;
    quint32 m_Length;
    int m_TimeoutMs;
};

int readBoundedEnvInt(const char* name, int fallback, int minimum, int maximum)
{
    bool ok = false;
    int value = qEnvironmentVariableIntValue(name, &ok);
    if (!ok) {
        return fallback;
    }
    return qBound(minimum, value, maximum);
}

void Session::clStageStarting(int stage)
{
    // We know this is called on the same thread as LiStartConnection()
    // which happens to be the main thread, so it's cool to interact
    // with the GUI in these callbacks.
    emit s_ActiveSession->stageStarting(QString::fromLocal8Bit(LiGetStageName(stage)));
}

void Session::clStageFailed(int stage, int errorCode)
{
    // Perform the port test now, while we're on the async connection thread and not blocking the UI.
    unsigned int portFlags = LiGetPortFlagsFromStage(stage);
    s_ActiveSession->m_PortTestResults = LiTestClientConnectivity(CONN_TEST_SERVER, 443, portFlags);

    char failingPorts[128];
    LiStringifyPortFlags(portFlags, ", ", failingPorts, sizeof(failingPorts));

    // Suppress the failure popup while we're silently retrying a reconnect
    if (s_ActiveSession->m_SuppressConnectionErrorDialog) {
        return;
    }

    emit s_ActiveSession->stageFailed(QString::fromLocal8Bit(LiGetStageName(stage)), errorCode, QString(failingPorts));
}

void Session::clConnectionTerminated(int errorCode)
{
    unsigned int portFlags = LiGetPortFlagsFromTerminationErrorCode(errorCode);
    s_ActiveSession->m_PortTestResults = LiTestClientConnectivity(CONN_TEST_SERVER, 443, portFlags);

    // Decide whether this looks like a transient network interruption that we
    // should try to silently reconnect from, rather than tearing down the
    // session and dropping the user back to the app grid.
    bool recoverable;
    switch (errorCode) {
    case ML_ERROR_GRACEFUL_TERMINATION:
    case ML_ERROR_PROTECTED_CONTENT:
    case ML_ERROR_FRAME_CONVERSION:
        // Graceful quit or host-side fatal errors: don't try to reconnect.
        recoverable = false;
        break;
    default:
        // NO_VIDEO_TRAFFIC / NO_VIDEO_FRAME / UNEXPECTED_EARLY_TERMINATION and
        // unknown codes are treated as recoverable network problems.
        recoverable = true;
        break;
    }

    // Only attempt reconnect if video actually started and the user didn't request
    // to quit. A window alone is not enough here: startup failures can create the
    // SDL window, then terminate before the first frame and otherwise loop forever
    // behind a black screen.
    if (recoverable &&
        s_ActiveSession->m_HasReceivedVideo &&
        !s_ActiveSession->m_ShouldExit &&
        !s_ActiveSession->m_ConnectionInterrupted) {

        s_ActiveSession->m_LastTerminationErrorCode = errorCode;
        s_ActiveSession->m_ConnectionInterrupted = true;

        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Connection interrupted (error %d); attempting to reconnect",
                    errorCode);

        // Wake the main loop so it can begin the reconnect flow
        SDL_Event event;
        event.type = SDL_QUIT;
        event.quit.timestamp = SDL_GetTicks();
        SDL_PushEvent(&event);
        return;
    }

    // Display the termination dialog if this was not intended
    s_ActiveSession->displayTerminationError(errorCode);

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Connection terminated: %d",
                 errorCode);

    // Push a quit event to the main loop
    SDL_Event event;
    event.type = SDL_QUIT;
    event.quit.timestamp = SDL_GetTicks();
    SDL_PushEvent(&event);
}

void Session::displayTerminationError(int errorCode)
{
    unsigned int portFlags = LiGetPortFlagsFromTerminationErrorCode(errorCode);

    switch (errorCode) {
    case ML_ERROR_GRACEFUL_TERMINATION:
        break;

    case ML_ERROR_NO_VIDEO_TRAFFIC:
        m_UnexpectedTermination = true;

        {
            char ports[128];
            SDL_assert(portFlags != 0);
            LiStringifyPortFlags(portFlags, ", ", ports, sizeof(ports));
            emit displayLaunchError(tr("No video received from host.") + "\n\n"+
                                    tr("Check your firewall and port forwarding rules for port(s): %1").arg(ports));
        }
        break;

    case ML_ERROR_NO_VIDEO_FRAME:
        m_UnexpectedTermination = true;
        emit displayLaunchError(tr("Your network connection isn't performing well. Reduce your video bitrate setting or try a faster connection."));
        break;

    case ML_ERROR_PROTECTED_CONTENT:
    case ML_ERROR_UNEXPECTED_EARLY_TERMINATION:
        m_UnexpectedTermination = true;
        emit displayLaunchError(tr("Something went wrong on your host PC when starting the stream.") + "\n\n" +
                                tr("Make sure you don't have any DRM-protected content open on your host PC. You can also try restarting your host PC."));
        break;

    case ML_ERROR_FRAME_CONVERSION:
        m_UnexpectedTermination = true;
        emit displayLaunchError(tr("The host PC reported a fatal video encoding error.") + "\n\n" +
                                tr("Try disabling HDR mode, changing the streaming resolution, or changing your host PC's display resolution."));
        break;

    default:
        m_UnexpectedTermination = true;

        // We'll assume large errors are hex values
        bool hexError = qAbs(errorCode) > 1000;
        emit displayLaunchError(tr("Connection terminated") + "\n\n" +
                                tr("Error code: %1").arg(errorCode, hexError ? 8 : 0, hexError ? 16 : 10, QChar('0')));
        break;
    }
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

bool Session::queueTouchpadFrameFlush()
{
    // Push an event onto the main loop so touchpad contacts already queued by
    // SDL can be collected into a single frame before they are sent.
    SDL_Event flushEvent = {};
    flushEvent.type = SDL_USEREVENT;
    flushEvent.user.code = SDL_CODE_FLUSH_TOUCHPAD_FRAME;
    return SDL_PushEvent(&flushEvent) > 0;
}

void Session::clRumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
    // We push an event for the main thread to handle in order to properly synchronize
    // with the removal of game controllers that could result in our game controller
    // going away during this callback.
    SDL_Event rumbleEvent = {};
    rumbleEvent.type = SDL_USEREVENT;
    rumbleEvent.user.code = SDL_CODE_GAMECONTROLLER_RUMBLE;
    rumbleEvent.user.data1 = (void*)(uintptr_t)controllerNumber;
    rumbleEvent.user.data2 = (void*)(uintptr_t)((lowFreqMotor << 16) | highFreqMotor);
    SDL_PushEvent(&rumbleEvent);
}

void Session::clConnectionStatusUpdate(int connectionStatus)
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Connection status update: %d",
                connectionStatus);

    if (!s_ActiveSession->m_Preferences->connectionWarnings) {
        return;
    }

    if (s_ActiveSession->m_MouseEmulationRefCount > 0) {
        // Don't display the overlay if mouse emulation is already using it
        return;
    }

    switch (connectionStatus)
    {
    case CONN_STATUS_POOR:
        s_ActiveSession->m_OverlayManager.updateOverlayText(Overlay::OverlayStatusUpdate,
                                                            s_ActiveSession->m_StreamConfig.bitrate > 5000 ?
                                                                "Slow connection to PC\nReduce your bitrate" : "Poor connection to PC");
        s_ActiveSession->m_OverlayManager.setOverlayState(Overlay::OverlayStatusUpdate, true);
        break;
    case CONN_STATUS_OKAY:
        s_ActiveSession->m_OverlayManager.setOverlayState(Overlay::OverlayStatusUpdate, false);
        break;
    }
}

void Session::clSetHdrMode(bool enabled, void* hdrMetadata)
{
    // Renderers retrieve this same snapshot through LiGetHdrMetadata(). Keep
    // accepting the callback argument so our listener ABI matches common-c.
    Q_UNUSED(hdrMetadata);

    // If we're in the process of recreating our decoder when we get
    // this callback, we'll drop it. The main thread will make the
    // callback when it finishes creating the new decoder.
    if (SDL_TryLockMutex(s_ActiveSession->m_DecoderLock) == 0) {
        IVideoDecoder* decoder = s_ActiveSession->m_VideoDecoder;
        if (decoder != nullptr) {
            decoder->setHdrMode(enabled);
        }
        SDL_UnlockMutex(s_ActiveSession->m_DecoderLock);
    }
}

void Session::clClipboardData(const char* data, int length)
{
    Session* session = s_ActiveSession;
    if (session == nullptr || session->m_ClipboardHelper == nullptr) {
        return;
    }

    // Queues internally; safe to call from the recv thread.
    session->m_ClipboardHelper->handleIncomingFrame(data, length);
}

void Session::clCursorUpdate(const LI_CURSOR_UPDATE* update)
{
    Session* session = s_ActiveSession;
    if (session == nullptr || update == nullptr) {
        return;
    }

    auto pending = std::make_shared<RemoteCursorUpdate>();
    pending->hasShape = (update->flags & LI_CURSOR_UPDATE_FLAG_SHAPE) != 0;
    pending->visible = (update->flags & LI_CURSOR_UPDATE_FLAG_VISIBLE) != 0;
    pending->shapeId = update->shapeId;
    pending->width = update->width;
    pending->height = update->height;
    pending->hotspotX = update->hotspotX;
    pending->hotspotY = update->hotspotY;
    if (pending->hasShape && update->pixels != nullptr && update->pixelDataLength != 0) {
        pending->bgra = QByteArray(
            reinterpret_cast<const char*>(update->pixels),
            static_cast<int>(update->pixelDataLength));
    }

    bool queueCursorEvent = false;
    {
        std::lock_guard<std::mutex> lock(session->m_CursorUpdateMutex);
        session->m_PendingCursorUpdate = pending;
        if (!session->m_CursorUpdateEventQueued) {
            session->m_CursorUpdateEventQueued = true;
            queueCursorEvent = true;
        }
    }

    if (!queueCursorEvent) {
        return;
    }

    SDL_Event cursorEvent = {};
    cursorEvent.type = SDL_USEREVENT;
    cursorEvent.user.code = SDL_CODE_CURSOR_UPDATE;
    if (SDL_PushEvent(&cursorEvent) <= 0) {
        std::lock_guard<std::mutex> lock(session->m_CursorUpdateMutex);
        // Keep the latest mailbox value. A subsequent callback will enqueue
        // another wakeup after observing the cleared flag.
        session->m_CursorUpdateEventQueued = false;
    }
}

void Session::clRumbleTriggers(uint16_t controllerNumber, uint16_t leftTrigger, uint16_t rightTrigger)
{
    // We push an event for the main thread to handle in order to properly synchronize
    // with the removal of game controllers that could result in our game controller
    // going away during this callback.
    SDL_Event rumbleEvent = {};
    rumbleEvent.type = SDL_USEREVENT;
    rumbleEvent.user.code = SDL_CODE_GAMECONTROLLER_RUMBLE_TRIGGERS;
    rumbleEvent.user.data1 = (void*)(uintptr_t)controllerNumber;
    rumbleEvent.user.data2 = (void*)(uintptr_t)((leftTrigger << 16) | rightTrigger);
    SDL_PushEvent(&rumbleEvent);
}

void Session::clSetMotionEventState(uint16_t controllerNumber, uint8_t motionType, uint16_t reportRateHz)
{
    // We push an event for the main thread to handle in order to properly synchronize
    // with the removal of game controllers that could result in our game controller
    // going away during this callback.
    SDL_Event setMotionEventStateEvent = {};
    setMotionEventStateEvent.type = SDL_USEREVENT;
    setMotionEventStateEvent.user.code = SDL_CODE_GAMECONTROLLER_SET_MOTION_EVENT_STATE;
    setMotionEventStateEvent.user.data1 = (void*)(uintptr_t)controllerNumber;
    setMotionEventStateEvent.user.data2 = (void*)(uintptr_t)((motionType << 16) | reportRateHz);
    SDL_PushEvent(&setMotionEventStateEvent);
}

void Session::clSetControllerLED(uint16_t controllerNumber, uint8_t r, uint8_t g, uint8_t b)
{
    // We push an event for the main thread to handle in order to properly synchronize
    // with the removal of game controllers that could result in our game controller
    // going away during this callback.
    SDL_Event setControllerLEDEvent = {};
    setControllerLEDEvent.type = SDL_USEREVENT;
    setControllerLEDEvent.user.code = SDL_CODE_GAMECONTROLLER_SET_CONTROLLER_LED;
    setControllerLEDEvent.user.data1 = (void*)(uintptr_t)controllerNumber;
    setControllerLEDEvent.user.data2 = (void*)(uintptr_t)(r << 16 | g << 8 | b);
    SDL_PushEvent(&setControllerLEDEvent);
}

void Session::clSetAdaptiveTriggers(uint16_t controllerNumber, uint8_t eventFlags, uint8_t typeLeft, uint8_t typeRight, uint8_t *left, uint8_t *right){
    // We push an event for the main thread to handle in order to properly synchronize
    // with the removal of game controllers that could result in our game controller
    // going away during this callback.
    SDL_Event setControllerLEDEvent = {};
    setControllerLEDEvent.type = SDL_USEREVENT;
    setControllerLEDEvent.user.code = SDL_CODE_GAMECONTROLLER_SET_ADAPTIVE_TRIGGERS;
    setControllerLEDEvent.user.data1 = (void*)(uintptr_t)controllerNumber;

    // Based on the following SDL code:
    // https://github.com/libsdl-org/SDL/blob/120c76c84bbce4c1bfed4e9eb74e10678bd83120/test/testgamecontroller.c#L286-L307
    DualSenseOutputReport *state = (DualSenseOutputReport *) SDL_malloc(sizeof(DualSenseOutputReport));
    SDL_zero(*state);
    state->validFlag0 = (eventFlags & DS_EFFECT_RIGHT_TRIGGER) | (eventFlags & DS_EFFECT_LEFT_TRIGGER);
    state->rightTriggerEffectType = typeRight;
    SDL_memcpy(state->rightTriggerEffect, right, sizeof(state->rightTriggerEffect));
    state->leftTriggerEffectType = typeLeft;
    SDL_memcpy(state->leftTriggerEffect, left, sizeof(state->leftTriggerEffect));

    setControllerLEDEvent.user.data2 = (void *) state;
    SDL_PushEvent(&setControllerLEDEvent);
}


bool Session::chooseDecoder(StreamingPreferences::VideoDecoderSelection vds,
                            StreamingPreferences::RendererSelection renderer,
                            SDL_Window* window, int videoFormat, int width, int height,
                            int frameRate, bool enableVsync, bool enableFramePacing, bool enableVideoEnhancement, bool ignoreAspectRatio, bool testOnly, IVideoDecoder*& chosenDecoder)
{
    DECODER_PARAMETERS params;

    // We should never have vsync enabled for test-mode.
    // It introduces unnecessary delay for renderers that may
    // block while waiting for a backbuffer swap.
    SDL_assert(!enableVsync || !testOnly);

    params.width = width;
    params.height = height;
    params.frameRate = frameRate;
    params.videoFormat = videoFormat;
    params.window = window;
    params.enableVsync = enableVsync;
    params.enableFramePacing = enableFramePacing;
    params.enableVideoEnhancement = enableVideoEnhancement;
    params.ignoreAspectRatio = ignoreAspectRatio;
    params.testOnly = testOnly;
    params.vds = vds;
    params.renderer = renderer;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "V-sync %s",
                enableVsync ? "enabled" : "disabled");

#ifdef HAVE_SLVIDEO
    chosenDecoder = new SLVideoDecoder(testOnly);
    if (chosenDecoder->initialize(&params)) {
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
    chosenDecoder = new FFmpegVideoDecoder(testOnly);
    if (chosenDecoder->initialize(&params)) {
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
    s_ActiveSession->m_HasReceivedVideo = true;

    // Use a lock since we'll be yanking this decoder out
    // from underneath the session when we initiate destruction.
    // We need to destroy the decoder on the main thread to satisfy
    // some API constraints (like DXVA2). If we can't acquire it,
    // that means the decoder is about to be destroyed, so we can
    // safely return DR_OK and wait for the IDR frame request by
    // the decoder reinitialization code.

    if (SDL_TryLockMutex(s_ActiveSession->m_DecoderLock) == 0) {
        IVideoDecoder* decoder = s_ActiveSession->m_VideoDecoder;
        if (decoder != nullptr) {
            int ret = decoder->submitDecodeUnit(du);
            SDL_UnlockMutex(s_ActiveSession->m_DecoderLock);
            return ret;
        }
        else {
            SDL_UnlockMutex(s_ActiveSession->m_DecoderLock);
            return DR_OK;
        }
    }
    else {
        // Decoder is going away. Ignore anything coming in until
        // the lock is released.
        return DR_OK;
    }
}

void Session::getDecoderInfo(SDL_Window* window,
                             bool& isHardwareAccelerated, bool& isFullScreenOnly,
                             bool& isHdrSupported, QSize& maxResolution)
{
    IVideoDecoder* decoder;

    // Since AV1 support on the host side is in its infancy, let's not consider
    // _only_ a working AV1 decoder to be acceptable and still show the warning
    // dialog indicating lack of hardware decoding support.

    // Try an HEVC Main10 decoder first to see if we have HDR support
    if (chooseDecoder(StreamingPreferences::VDS_FORCE_HARDWARE,
                      StreamingPreferences::RS_PROBE_ONLY,
                      window, VIDEO_FORMAT_H265_MAIN10, 1920, 1080, 60,
                      false, false, false, false, true, decoder)) {
        isHardwareAccelerated = decoder->isHardwareAccelerated();
        isFullScreenOnly = decoder->isAlwaysFullScreen();
        isHdrSupported = decoder->isHdrSupported();
        maxResolution = decoder->getDecoderMaxResolution();
        delete decoder;

        return;
    }

    // Try an AV1 Main10 decoder next to see if we have HDR support
    if (chooseDecoder(StreamingPreferences::VDS_FORCE_HARDWARE,
                      StreamingPreferences::RS_PROBE_ONLY,
                      window, VIDEO_FORMAT_AV1_MAIN10, 1920, 1080, 60,
                      false, false, false, false, true, decoder)) {
        // If we've got a working AV1 Main 10-bit decoder, we'll enable the HDR checkbox
        // but we will still continue probing to get other attributes for HEVC or H.264
        // decoders. See the AV1 comment at the top of the function for more info.
        isHdrSupported = decoder->isHdrSupported();
        delete decoder;
    }
    else {
        // If we found no hardware decoders with HDR, check for a renderer
        // that supports HDR rendering with software decoded frames.
        if (chooseDecoder(StreamingPreferences::VDS_FORCE_SOFTWARE,
                          StreamingPreferences::RS_PROBE_ONLY,
                          window, VIDEO_FORMAT_H265_MAIN10, 1920, 1080, 60,
                          false, false, false, false, true, decoder) ||
            chooseDecoder(StreamingPreferences::VDS_FORCE_SOFTWARE,
                          StreamingPreferences::RS_PROBE_ONLY,
                          window, VIDEO_FORMAT_AV1_MAIN10, 1920, 1080, 60,
                          false, false, false, false, true, decoder)) {
            isHdrSupported = decoder->isHdrSupported();
            delete decoder;
        }
        else {
            // We weren't compiled with an HDR-capable renderer or we don't
            // have the required GPU driver support for any HDR renderers.
            isHdrSupported = false;
        }
    }

    // Try a regular hardware accelerated HEVC decoder now
    if (chooseDecoder(StreamingPreferences::VDS_FORCE_HARDWARE,
                      StreamingPreferences::RS_PROBE_ONLY,
                      window, VIDEO_FORMAT_H265, 1920, 1080, 60,
                      false, false, false, false, true, decoder)) {
        isHardwareAccelerated = decoder->isHardwareAccelerated();
        isFullScreenOnly = decoder->isAlwaysFullScreen();
        maxResolution = decoder->getDecoderMaxResolution();
        delete decoder;

        return;
    }


#if 0 // See AV1 comment at the top of this function
    if (chooseDecoder(StreamingPreferences::VDS_FORCE_HARDWARE,
                      StreamingPreferences::RS_PROBE_ONLY,
                      window, VIDEO_FORMAT_AV1_MAIN8, 1920, 1080, 60,
                      false, false, false, false, true, decoder)) {
        isHardwareAccelerated = decoder->isHardwareAccelerated();
        isFullScreenOnly = decoder->isAlwaysFullScreen();
        maxResolution = decoder->getDecoderMaxResolution();
        delete decoder;

        return;
    }
#endif

    // If we still didn't find a hardware decoder, try H.264 now.
    // This will fall back to software decoding, so it should always work.
    if (chooseDecoder(StreamingPreferences::VDS_AUTO,
                      StreamingPreferences::RS_PROBE_ONLY,
                      window, VIDEO_FORMAT_H264, 1920, 1080, 60,
                      false, false, false, false, true, decoder)) {
        isHardwareAccelerated = decoder->isHardwareAccelerated();
        isFullScreenOnly = decoder->isAlwaysFullScreen();
        maxResolution = decoder->getDecoderMaxResolution();
        delete decoder;

        return;
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to find ANY working H.264 or HEVC decoder!");
}

Session::DecoderAvailability
Session::getDecoderAvailability(SDL_Window* window,
                                StreamingPreferences::VideoDecoderSelection vds,
                                int videoFormat, int width, int height, int frameRate)
{
    IVideoDecoder* decoder;

    if (!chooseDecoder(vds,
                       StreamingPreferences::RS_PROBE_ONLY,
                       window, videoFormat, width, height, frameRate,
                       false, false, false, false, true, decoder)) {
        return DecoderAvailability::None;
    }

    bool hw = decoder->isHardwareAccelerated();

    delete decoder;

    return hw ? DecoderAvailability::Hardware : DecoderAvailability::Software;
}

bool Session::populateDecoderProperties(SDL_Window* window)
{
    IVideoDecoder* decoder;

    // NB: We pass the real renderer selection rather than RS_PROBE_ONLY
    // here because this is operating on the real streaming window, and
    // instantiating Metal or AVSBDL renderers can interfere with MoltenVK's
    // attempt to change the window's colorspace, causing washed out colors.
    if (!chooseDecoder(m_Preferences->videoDecoderSelection,
                       m_Preferences->rendererSelection,
                       window,
                       m_SupportedVideoFormats.first(),
                       m_StreamConfig.width,
                       m_StreamConfig.height,
                       m_StreamConfig.fps,
                       false, false, false, false, true, decoder)) {
        return false;
    }

    m_VideoCallbacks.capabilities = decoder->getDecoderCapabilities();
    if (m_VideoCallbacks.capabilities & CAPABILITY_PULL_RENDERER) {
        // It is an error to pass a push callback when in pull mode
        m_VideoCallbacks.submitDecodeUnit = nullptr;
    }
    else {
        m_VideoCallbacks.submitDecodeUnit = drSubmitDecodeUnit;
    }

    if (Utils::getEnvironmentVariableOverride("COLOR_SPACE_OVERRIDE", &m_StreamConfig.colorSpace)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using colorspace override: %d",
                    m_StreamConfig.colorSpace);
    }
    else {
        m_StreamConfig.colorSpace = decoder->getDecoderColorspace();
    }

    if (Utils::getEnvironmentVariableOverride("COLOR_RANGE_OVERRIDE", &m_StreamConfig.colorRange)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using color range override: %d",
                    m_StreamConfig.colorRange);
    }
    else {
        m_StreamConfig.colorRange = decoder->getDecoderColorRange();
    }

    if (decoder->isAlwaysFullScreen()) {
        m_IsFullScreen = true;
    }

    delete decoder;

    return true;
}

Session::Session(NvComputer* computer, NvApp& app, StreamingPreferences *preferences)
    : m_Preferences(preferences ? preferences : StreamingPreferences::get()),
      m_IsFullScreen(m_Preferences->windowMode != StreamingPreferences::WM_WINDOWED || !WMUtils::isRunningDesktopEnvironment()),
      m_Computer(computer),
      m_App(app),
      m_Window(nullptr),
      m_VideoDecoder(nullptr),
      m_DecoderLock(SDL_CreateMutex()),
      m_AudioMuted(false),
      m_QtWindow(nullptr),
      m_UnexpectedTermination(true), // Failure prior to streaming is unexpected
      m_InputHandler(nullptr),
      m_MouseEmulationRefCount(0),
      m_FlushingWindowEventsRef(0),
      m_ShouldExit(false),
      m_ConnectionInterrupted(false),
      m_SuppressConnectionErrorDialog(false),
      m_HasReceivedVideo(false),
      m_LastTerminationErrorCode(0),
      m_AsyncConnectionSuccess(false),
      m_PortTestResults(0),
      m_OpusDecoder(nullptr),
      m_AudioRenderer(nullptr),
      m_AudioSampleCount(0),
      m_DropAudioEndTime(0),
      m_MenuPanel(nullptr),
      m_DeferCaptureRestore(false),
      m_PendingMicToggle(false),
            m_SunshineAbrEnabled(false),
            m_LastAbrFeedbackTicks(0),
            m_AbrFeedbackInFlight(std::make_shared<std::atomic_bool>(false)),
            m_AbrCurrentBitrateKbps(std::make_shared<std::atomic_int>(0)),
      m_Toast(nullptr),
      m_FileMappingState(OverlayMenuPanel::FileMappingState::Unknown),
      m_FileMappingDetail(tr("Checking")),
      m_FileMappingToast(),
      m_FileMappingToastPending(false),
      m_FileMappingProbeState(nullptr),
      m_FileMappingMountState(nullptr),
      m_FileMappingMountPath(),
      m_FileMappingSessionId(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      m_MenuCloseTicks(0),
      m_MicStream(nullptr)
{
    memset(&m_LastAbrVideoStats, 0, sizeof(m_LastAbrVideoStats));
    m_ClipboardHelper = nullptr;
}

Session::~Session()
{
    // NB: This may not get destroyed for a long time! Don't put any non-trivial cleanup here.
    // Use Session::exec() or DeferredSessionCleanupTask instead.

    if (m_ClipboardHelper != nullptr) {
        m_ClipboardHelper->stop();
        delete m_ClipboardHelper;
        m_ClipboardHelper = nullptr;
    }

    SDL_DestroyMutex(m_DecoderLock);
}

bool Session::initialize(QQuickWindow* qtWindow)
{
    m_QtWindow = qtWindow;

    // SDL reads this hint when the video subsystem initializes. Configure it for
    // each session so preference changes take effect on the next stream without
    // restarting Moonlight. This hint makes sens only on macOS currently.
    bool nativeTouchpadEnabled = m_Preferences->enableNativeTouchpad;
    SDL_SetHint(SDL_HINT_TRACKPAD_IS_TOUCH_ONLY, nativeTouchpadEnabled ? "1" : "0");

#ifdef Q_OS_DARWIN
    if (qEnvironmentVariableIntValue("I_WANT_BUGGY_FULLSCREEN") == 0) {
        // If we have a notch and the user specified one of the two native display modes
        // (notched or notchless), override the fullscreen mode to ensure it works as expected.
        // - SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES=0 will place the video underneath the notch
        // - SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES=1 will place the video below the notch
        bool shouldUseFullScreenSpaces = m_Preferences->windowMode != StreamingPreferences::WM_FULLSCREEN;
        SDL_DisplayMode desktopMode;
        SDL_Rect safeArea;
        for (int displayIndex = 0; StreamUtils::getNativeDesktopMode(displayIndex, &desktopMode, &safeArea); displayIndex++) {
            // Check if this display has a notch (safeArea != desktopMode)
            if (desktopMode.h != safeArea.h || desktopMode.w != safeArea.w) {
                // Check if we're trying to stream at the full native resolution (including notch)
                if (m_Preferences->width == desktopMode.w && m_Preferences->height == desktopMode.h) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Overriding default fullscreen mode for native fullscreen resolution");
                    shouldUseFullScreenSpaces = false;
                    break;
                }
                else if (m_Preferences->width == safeArea.w && m_Preferences->height == safeArea.h) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Overriding default fullscreen mode for native safe area resolution");
                    shouldUseFullScreenSpaces = true;
                    break;
                }
            }
        }

        // Using modesetting on modern versions of macOS is extremely unreliable
        // and leads to hangs, deadlocks, and other nasty stuff. The only time
        // people seem to use it is to get the full screen on notched Macs,
        // which setting SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES=1 also accomplishes
        // with much less headache.
        //
        // https://github.com/moonlight-stream/moonlight-qt/issues/973
        // https://github.com/moonlight-stream/moonlight-qt/issues/999
        // https://github.com/moonlight-stream/moonlight-qt/issues/1211
        // https://github.com/moonlight-stream/moonlight-qt/issues/1218
        SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, shouldUseFullScreenSpaces ? "1" : "0");
    }
#endif

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return false;
    }

    // Stop text input. SDL enables it by default
    // when we initialize the video subsystem, but this
    // causes an IME popup when certain keys are held down
    // on macOS.
    SDL_StopTextInput();

    LiInitializeStreamConfiguration(&m_StreamConfig);
    m_StreamConfig.width = m_Preferences->width;
    m_StreamConfig.height = m_Preferences->height;

    // 应用分辨率缩放
    if (m_Preferences->streamResolutionScale && m_Preferences->streamResolutionScaleRatio != 100) {
        int scaledWidth = m_StreamConfig.width * m_Preferences->streamResolutionScaleRatio / 100;
        int scaledHeight = m_StreamConfig.height * m_Preferences->streamResolutionScaleRatio / 100;
        // 确保缩放后的分辨率是8的倍数
        m_StreamConfig.width = (scaledWidth / 8) * 8;
        m_StreamConfig.height = (scaledHeight / 8) * 8;
    }

    int x, y, width, height;
    getWindowDimensions(x, y, width, height);

    // Create a hidden window to use for decoder initialization tests
    SDL_Window* testWindow = StreamUtils::createTestWindow();
    if (!testWindow) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create window for hardware decode test: %s",
                     SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    qInfo() << "Server GPU:" << m_Computer->gpuModel;
    qInfo() << "Server GFE version:" << m_Computer->gfeVersion;

    LiInitializeVideoCallbacks(&m_VideoCallbacks);
    m_VideoCallbacks.setup = drSetup;

    m_StreamConfig.fps = m_Preferences->fps;
    m_StreamConfig.bitrate = m_Preferences->bitrateKbps;

#ifndef STEAM_LINK
    // Opt-in to all encryption features if we detect that the platform
    // has AES cryptography acceleration instructions and more than 2 cores.
    if (StreamUtils::hasFastAes() && SDL_GetCPUCount() > 2) {
        m_StreamConfig.encryptionFlags = ENCFLG_ALL;
    }
    else {
        // Enable audio encryption as long as we're not on Steam Link.
        // That hardware can hardly handle Opus decoding at all.
        m_StreamConfig.encryptionFlags = ENCFLG_AUDIO;
    }
#endif

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Video bitrate: %d kbps",
                m_StreamConfig.bitrate);

    RAND_bytes(reinterpret_cast<unsigned char*>(m_StreamConfig.remoteInputAesKey),
               sizeof(m_StreamConfig.remoteInputAesKey));

    // Only the first 4 bytes are populated in the RI key IV
    RAND_bytes(reinterpret_cast<unsigned char*>(m_StreamConfig.remoteInputAesIv), 4);

    switch (m_Preferences->audioConfig)
    {
    case StreamingPreferences::AC_STEREO:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
        break;
    case StreamingPreferences::AC_51_SURROUND:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_51_SURROUND;
        break;
    case StreamingPreferences::AC_71_SURROUND:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_71_SURROUND;
        break;
    case StreamingPreferences::AC_714_SURROUND:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_714_SURROUND;
        break;
    }

    m_StreamConfig.enableMic = m_Preferences->enableMicrophone;

    LiInitializeAudioCallbacks(&m_AudioCallbacks);
    m_AudioCallbacks.init = arInit;
    m_AudioCallbacks.cleanup = arCleanup;
    m_AudioCallbacks.decodeAndPlaySample = arDecodeAndPlaySample;
    m_AudioCallbacks.capabilities = getAudioRendererCapabilities(m_StreamConfig.audioConfiguration);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio channel count: %d",
                CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration));
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio channel mask: %X",
                CHANNEL_MASK_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration));

    // Start with all codecs and profiles in priority order
    m_SupportedVideoFormats.append(VIDEO_FORMAT_AV1_HIGH10_444);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_AV1_MAIN10);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H265_REXT10_444);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H265_MAIN10);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_AV1_HIGH8_444);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_AV1_MAIN8);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H265_REXT8_444);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H265);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H264_HIGH8_444);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H264);

    switch (m_Preferences->videoCodecConfig)
    {
    case StreamingPreferences::VCC_AUTO:
    {
        // Codecs are checked in order of ascending decode complexity to ensure
        // the the deprioritized list prefers lighter codecs for software decoding

        // H.264 is already the lowest priority codec, so we don't need to do
        // any probing for deprioritization for it here.

        auto hevcDA = getDecoderAvailability(testWindow,
                                             m_Preferences->videoDecoderSelection,
                                             m_Preferences->enableYUV444 ?
                                                 (m_Preferences->enableHdr ? VIDEO_FORMAT_H265_REXT10_444 : VIDEO_FORMAT_H265_REXT8_444) :
                                                 (m_Preferences->enableHdr ? VIDEO_FORMAT_H265_MAIN10 : VIDEO_FORMAT_H265),
                                             m_StreamConfig.width,
                                             m_StreamConfig.height,
                                             m_StreamConfig.fps);
        if (hevcDA == DecoderAvailability::None && m_Preferences->enableHdr) {
            // Remove all 10-bit HEVC profiles
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_H265 & VIDEO_FORMAT_MASK_10BIT);

            // Check if we have 10-bit AV1 support
            auto av1DA = getDecoderAvailability(testWindow,
                                                m_Preferences->videoDecoderSelection,
                                                m_Preferences->enableYUV444 ? VIDEO_FORMAT_AV1_HIGH10_444 : VIDEO_FORMAT_AV1_MAIN10,
                                                m_StreamConfig.width,
                                                m_StreamConfig.height,
                                                m_StreamConfig.fps);
            if (av1DA == DecoderAvailability::None) {
                // Remove all 10-bit AV1 profiles
                m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_AV1 & VIDEO_FORMAT_MASK_10BIT);

                // There are no available 10-bit profiles, so reprobe for 8-bit HEVC
                // and we'll proceed as normal for an SDR streaming scenario.
                SDL_assert(!(m_SupportedVideoFormats & VIDEO_FORMAT_MASK_10BIT));
                hevcDA = getDecoderAvailability(testWindow,
                                                m_Preferences->videoDecoderSelection,
                                                m_Preferences->enableYUV444 ? VIDEO_FORMAT_H265_REXT8_444 : VIDEO_FORMAT_H265,
                                                m_StreamConfig.width,
                                                m_StreamConfig.height,
                                                m_StreamConfig.fps);
            }
        }

        if (hevcDA != DecoderAvailability::Hardware) {
            // Deprioritize HEVC unless the user forced software decoding and enabled HDR.
            // We need HEVC in that case because we cannot support 10-bit content with H.264,
            // which would ordinarily be prioritized for software decoding performance.
            if (m_Preferences->videoDecoderSelection != StreamingPreferences::VDS_FORCE_SOFTWARE || !m_Preferences->enableHdr) {
                m_SupportedVideoFormats.deprioritizeByMask(VIDEO_FORMAT_MASK_H265);
            }
        }

        // Deprioritize AV1 unless we can't hardware decode HEVC, and have HDR enabled
        // or we're on Windows or a non-x86 Linux/BSD.
        //
        // Normally, we'd assume hardware that can't decode HEVC definitely can't decode
        // AV1 either, and we wouldn't even bother probing for AV1 support. However, some
        // Windows business systems have HEVC support disabled in firmware from the factory,
        // yet they can still decode AV1 in hardware. To avoid falling back to H.264 on
        // these systems, we don't deprioritize AV1. This firmware-based HEVC licensing
        // behavior seems to be unique to Windows, and Linux on the same system is able
        // to decode HEVC in hardware normally using VAAPI.
        // https://www.reddit.com/r/GeForceNOW/comments/1omsckt/psa_be_wary_of_purchasing_dell_computers_with/
        //
        // Some embedded Linux platforms have incomplete V4L2 decoding support which can
        // lead to unusual cases where a system might support H.264 and AV1 but not HEVC,
        // even if the underlying hardware supports all three. RK3588 is an example of
        // such a SoC. To handle this situation, we will also probe for AV1 if we're on
        // a non-x86 non-macOS UNIX system.
        //
        // We want to keep AV1 at the top of the list for HDR with software decoding
        // because dav1d is higher performance than FFmpeg's HEVC software decoder.
        if (hevcDA == DecoderAvailability::Hardware
#if !defined(Q_OS_WIN32) && (!(defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)) || defined(Q_PROCESSOR_X86))
            || !m_Preferences->enableHdr
#endif
            ) {
            m_SupportedVideoFormats.deprioritizeByMask(VIDEO_FORMAT_MASK_AV1);
        }
        else if (!m_Preferences->enableHdr &&
                   getDecoderAvailability(testWindow,
                                          m_Preferences->videoDecoderSelection,
                                          m_Preferences->enableYUV444 ? VIDEO_FORMAT_AV1_HIGH8_444 : VIDEO_FORMAT_AV1_MAIN8,
                                          m_StreamConfig.width,
                                          m_StreamConfig.height,
                                          m_StreamConfig.fps) != DecoderAvailability::Hardware) {
            m_SupportedVideoFormats.deprioritizeByMask(VIDEO_FORMAT_MASK_AV1);
        }

#ifdef Q_OS_DARWIN
        {
            // Prior to GFE 3.11, GFE did not allow us to constrain
            // the number of reference frames, so we have to fixup the SPS
            // to allow decoding via VideoToolbox on macOS. Since we don't
            // have fixup code for HEVC, just avoid it if GFE is too old.
            QVector<int> gfeVersion = NvHTTP::parseQuad(m_Computer->gfeVersion);
            if (gfeVersion.isEmpty() || // Very old versions don't have GfeVersion at all
                    gfeVersion[0] < 3 ||
                    (gfeVersion[0] == 3 && gfeVersion[1] < 11)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Disabling HEVC on macOS due to old GFE version");
                m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_H265);
            }
        }
#endif
        break;
    }
    case StreamingPreferences::VCC_FORCE_H264:
        m_SupportedVideoFormats.removeByMask(~VIDEO_FORMAT_MASK_H264);
        break;
    case StreamingPreferences::VCC_FORCE_HEVC:
    case StreamingPreferences::VCC_FORCE_HEVC_HDR_DEPRECATED:
        m_SupportedVideoFormats.removeByMask(~VIDEO_FORMAT_MASK_H265);
        break;
    case StreamingPreferences::VCC_FORCE_AV1:
        // We'll try to fall back to HEVC first if AV1 fails. We'd rather not fall back
        // straight to H.264 if the user asked for AV1 and the host doesn't support it.
        m_SupportedVideoFormats.removeByMask(~(VIDEO_FORMAT_MASK_AV1 | VIDEO_FORMAT_MASK_H265));
        break;
    }

    // NB: Since deprioritization puts codecs in reverse order (at the bottom of the list),
    // we want to deprioritize for the most critical attributes last to ensure they are the
    // lowest priority codecs during server negotiation. Here we do that with YUV 4:4:4 and
    // HDR to ensure we never pick a codec profile that doesn't meet the user's requirement
    // if we can avoid it.

    // Mask off YUV 4:4:4 codecs if the option is not enabled
    if (!m_Preferences->enableYUV444) {
        m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_YUV444);
    }
    else {
        // Deprioritize YUV 4:2:0 codecs if the user wants YUV 4:4:4
        //
        // NB: Since this happens first before deprioritizing HDR, we will
        // pick a YUV 4:4:4 profile instead of a 10-bit profile if they
        // aren't both available together for any codec.
        m_SupportedVideoFormats.deprioritizeByMask(~VIDEO_FORMAT_MASK_YUV444);
    }

    // Mask off 10-bit codecs if HDR is not enabled
    if (!m_Preferences->enableHdr) {
        m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_10BIT);
    }
    else {
        // Deprioritize 8-bit codecs if HDR is enabled
        m_SupportedVideoFormats.deprioritizeByMask(~VIDEO_FORMAT_MASK_10BIT);

        // Set the HDR mode (PQ or HLG) based on user preference
        m_StreamConfig.hdrMode = static_cast<int>(m_Preferences->hdrMode);
    }

    switch (m_Preferences->windowMode)
    {
    default:
        // Normally we'd default to fullscreen desktop when starting in windowed
        // mode, but in the case of a slow GPU, we want to use real fullscreen
        // to allow the display to assist with the video scaling work.
        if (WMUtils::isGpuSlow()) {
            m_FullScreenFlag = SDL_WINDOW_FULLSCREEN;
            break;
        }
        // Fall-through
    case StreamingPreferences::WM_FULLSCREEN_DESKTOP:
        // Only use full-screen desktop mode if we're running a desktop environment
        if (WMUtils::isRunningDesktopEnvironment()) {
            m_FullScreenFlag = SDL_WINDOW_FULLSCREEN_DESKTOP;
            break;
        }
        // Fall-through
    case StreamingPreferences::WM_FULLSCREEN:
#ifdef Q_OS_DARWIN
        if (qEnvironmentVariableIntValue("I_WANT_BUGGY_FULLSCREEN") == 0) {
            // Don't use "real" fullscreen on macOS by default. See comments above.
            m_FullScreenFlag = SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        else {
            m_FullScreenFlag = SDL_WINDOW_FULLSCREEN;
        }
#else
        m_FullScreenFlag = SDL_WINDOW_FULLSCREEN;
#endif
        break;
    }

#if !SDL_VERSION_ATLEAST(2, 0, 11)
    // HACK: Using a full-screen window breaks mouse capture on the Pi's LXDE
    // GUI environment. Force the session to use windowed mode (which won't
    // really matter anyway because the MMAL renderer always draws full-screen).
    if (qgetenv("DESKTOP_SESSION") == "LXDE-pi") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Forcing windowed mode on LXDE-Pi");
        m_FullScreenFlag = 0;
    }
#endif

    // Check for validation errors/warnings and emit
    // signals for them, if appropriate
    bool ret = validateLaunch(testWindow);

    if (ret) {
        // Video format is now locked in
        m_StreamConfig.supportedVideoFormats = m_SupportedVideoFormats.front();

        // Populate decoder-dependent properties.
        // Must be done after validateLaunch() since m_StreamConfig is finalized.
        ret = populateDecoderProperties(testWindow);
    }

    SDL_DestroyWindow(testWindow);

    if (!ret) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    // 启动带宽计算
    BandwidthCalculator::instance()->start();

    return true;
}

void Session::emitLaunchWarning(QString text)
{
    if (m_Preferences->configurationWarnings) {
        // Queue this launch warning to be displayed after validation
        m_LaunchWarnings.append(text);
        emit launchWarningsChanged();
    }
}

bool Session::validateLaunch(SDL_Window* testWindow)
{
    if (!m_Computer->isSupportedServerVersion) {
        emit displayLaunchError(tr("The version of GeForce Experience on %1 is not supported by this build of Moonlight. You must update Moonlight to stream from %1.").arg(m_Computer->name));
        return false;
    }

    if (m_Preferences->absoluteMouseMode && !m_App.isAppCollectorGame) {
        emitLaunchWarning(tr("Your selection to enable remote desktop mouse mode may cause problems in games."));
    }

    if (m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_FORCE_SOFTWARE) {
        emitLaunchWarning(tr("Your settings selection to force software decoding may cause poor streaming performance."));
    }

    if (m_SupportedVideoFormats & VIDEO_FORMAT_MASK_AV1) {
        if (m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport & SCM_MASK_AV1) == 0) {
            if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_AV1) {
                emitLaunchWarning(tr("Your host software or GPU doesn't support encoding AV1."));
            }

            // Moonlight-common-c will handle this case already, but we want
            // to set this explicitly here so we can do our hardware acceleration
            // check below.
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_AV1);
        }
        else {
            if (!m_Preferences->enableHdr && // HDR is checked below
                 m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_AUTO && // Force hardware decoding checked below
                 m_Preferences->videoCodecConfig != StreamingPreferences::VCC_AUTO && // Auto VCC is already checked in initialize()
                 getDecoderAvailability(testWindow,
                                        m_Preferences->videoDecoderSelection,
                                        VIDEO_FORMAT_AV1_MAIN8,
                                        m_StreamConfig.width,
                                        m_StreamConfig.height,
                                        m_StreamConfig.fps) != DecoderAvailability::Hardware) {
                emitLaunchWarning(tr("Using software decoding due to your selection to force AV1 without GPU support. This may cause poor streaming performance."));
            }

            if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_AV1) {
                m_SupportedVideoFormats.removeByMask(~VIDEO_FORMAT_MASK_AV1);
            }
        }
    }

    if (m_SupportedVideoFormats & VIDEO_FORMAT_MASK_H265) {
        if (m_Computer->maxLumaPixelsHEVC == 0) {
            if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC) {
                emitLaunchWarning(tr("Your host PC doesn't support encoding HEVC."));
            }

            // Moonlight-common-c will handle this case already, but we want
            // to set this explicitly here so we can do our hardware acceleration
            // check below.
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_H265);
        }
        else {
            if (!m_Preferences->enableHdr && // HDR is checked below
                 m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_AUTO && // Force hardware decoding checked below
                 m_Preferences->videoCodecConfig != StreamingPreferences::VCC_AUTO && // Auto VCC is already checked in initialize()
                 getDecoderAvailability(testWindow,
                                        m_Preferences->videoDecoderSelection,
                                        VIDEO_FORMAT_H265,
                                        m_StreamConfig.width,
                                        m_StreamConfig.height,
                                        m_StreamConfig.fps) != DecoderAvailability::Hardware) {
                emitLaunchWarning(tr("Using software decoding due to your selection to force HEVC without GPU support. This may cause poor streaming performance."));
            }

            if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC) {
                m_SupportedVideoFormats.removeByMask(~VIDEO_FORMAT_MASK_H265);
            }
        }
    }

    if (!(m_SupportedVideoFormats & ~VIDEO_FORMAT_MASK_H264) &&
            m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_AUTO &&
            getDecoderAvailability(testWindow,
                                   m_Preferences->videoDecoderSelection,
                                   VIDEO_FORMAT_H264,
                                   m_StreamConfig.width,
                                   m_StreamConfig.height,
                                   m_StreamConfig.fps) != DecoderAvailability::Hardware) {

        if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_H264) {
            emitLaunchWarning(tr("Using software decoding due to your selection to force H.264 without GPU support. This may cause poor streaming performance."));
        }
        else {
            if (m_Computer->maxLumaPixelsHEVC == 0 &&
                    getDecoderAvailability(testWindow,
                                           m_Preferences->videoDecoderSelection,
                                           VIDEO_FORMAT_H265,
                                           m_StreamConfig.width,
                                           m_StreamConfig.height,
                                           m_StreamConfig.fps) == DecoderAvailability::Hardware) {
                emitLaunchWarning(tr("Your host PC and client PC don't support the same video codecs. This may cause poor streaming performance."));
            }
            else {
                emitLaunchWarning(tr("Your client GPU doesn't support H.264 decoding. This may cause poor streaming performance."));
            }
        }
    }

    if (m_Preferences->enableHdr) {
        if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_H264) {
            emitLaunchWarning(tr("HDR is not supported using the H.264 codec."));
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_10BIT);
        }
        else if (!(m_SupportedVideoFormats & VIDEO_FORMAT_MASK_10BIT)) {
            emitLaunchWarning(tr("This PC's GPU doesn't support 10-bit HEVC or AV1 decoding for HDR streaming."));
        }
        // Check that the server GPU supports HDR
        else if (m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport & SCM_MASK_10BIT) == 0) {
            emitLaunchWarning(tr("Your host PC doesn't support HDR streaming."));
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_10BIT);
        }
        else if (m_Preferences->videoCodecConfig != StreamingPreferences::VCC_AUTO) { // Auto was already checked during init
            bool displayedHdrSoftwareDecodeWarning = false;

            // Check that the available HDR-capable codecs on the client and server are compatible
            if (m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport & SCM_AV1_MAIN10)) {
                auto da = getDecoderAvailability(testWindow,
                                                 m_Preferences->videoDecoderSelection,
                                                 VIDEO_FORMAT_AV1_MAIN10,
                                                 m_StreamConfig.width,
                                                 m_StreamConfig.height,
                                                 m_StreamConfig.fps);
                if (da == DecoderAvailability::None) {
                    emitLaunchWarning(tr("This PC's GPU doesn't support AV1 Main10 decoding for HDR streaming."));
                    m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_AV1_MAIN10);
                }
                else if (da == DecoderAvailability::Software &&
                           m_Preferences->videoDecoderSelection != StreamingPreferences::VDS_FORCE_SOFTWARE &&
                           !displayedHdrSoftwareDecodeWarning) {
                    emitLaunchWarning(tr("Using software decoding due to your selection to force HDR without GPU support. This may cause poor streaming performance."));
                    displayedHdrSoftwareDecodeWarning = true;
                }
            }
            if (m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport & SCM_HEVC_MAIN10)) {
                auto da = getDecoderAvailability(testWindow,
                                                 m_Preferences->videoDecoderSelection,
                                                 VIDEO_FORMAT_H265_MAIN10,
                                                 m_StreamConfig.width,
                                                 m_StreamConfig.height,
                                                 m_StreamConfig.fps);
                if (da == DecoderAvailability::None) {
                    emitLaunchWarning(tr("This PC's GPU doesn't support HEVC Main10 decoding for HDR streaming."));
                    m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_H265_MAIN10);
                }
                else if (da == DecoderAvailability::Software &&
                         m_Preferences->videoDecoderSelection != StreamingPreferences::VDS_FORCE_SOFTWARE &&
                         !displayedHdrSoftwareDecodeWarning) {
                    emitLaunchWarning(tr("Using software decoding due to your selection to force HDR without GPU support. This may cause poor streaming performance."));
                    displayedHdrSoftwareDecodeWarning = true;
                }
            }
        }

        // Check for compatibility between server and client codecs
        if ((m_SupportedVideoFormats & VIDEO_FORMAT_MASK_10BIT) && // Ignore this check if we already failed one above
            !(m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport) & VIDEO_FORMAT_MASK_10BIT)) {
            emitLaunchWarning(tr("Your host PC and client PC don't support the same HDR video codecs."));
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_10BIT);
        }
    }

    if (m_Preferences->enableYUV444) {
        if (!(m_Computer->serverCodecModeSupport & SCM_MASK_YUV444)) {
            emitLaunchWarning(tr("Your host PC doesn't support YUV 4:4:4 streaming."));
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_YUV444);
        }
        else {
            m_SupportedVideoFormats.removeByMask(~m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport));

            if (!m_SupportedVideoFormats.isEmpty() &&
                !(m_SupportedVideoFormats.front() & VIDEO_FORMAT_MASK_YUV444)) {
                emitLaunchWarning(tr("Your host PC doesn't support YUV 4:4:4 streaming for selected video codec."));
            }
            else if (m_Preferences->videoDecoderSelection != StreamingPreferences::VDS_FORCE_SOFTWARE) {
                while (!m_SupportedVideoFormats.isEmpty() &&
                       (m_SupportedVideoFormats.front() & VIDEO_FORMAT_MASK_YUV444) &&
                       getDecoderAvailability(testWindow,
                                              m_Preferences->videoDecoderSelection,
                                              m_SupportedVideoFormats.front(),
                                              m_StreamConfig.width,
                                              m_StreamConfig.height,
                                              m_StreamConfig.fps) != DecoderAvailability::Hardware) {
                    if (m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_FORCE_HARDWARE) {
                        m_SupportedVideoFormats.removeFirst();
                    }
                    else {
                        emitLaunchWarning(tr("Using software decoding due to your selection to force YUV 4:4:4 without GPU support. This may cause poor streaming performance."));
                        break;
                    }
                }
                if (!m_SupportedVideoFormats.isEmpty() &&
                    !(m_SupportedVideoFormats.front() & VIDEO_FORMAT_MASK_YUV444)) {
                    emitLaunchWarning(tr("This PC's GPU doesn't support YUV 4:4:4 decoding for selected video codec."));
                }
            }
        }
    }

    if (m_StreamConfig.width >= 3840) {
        // Only allow 4K on GFE 3.x+
        if (m_Computer->gfeVersion.isEmpty() || m_Computer->gfeVersion.startsWith("2.")) {
            emitLaunchWarning(tr("GeForce Experience 3.0 or higher is required for 4K streaming."));

            m_StreamConfig.width = 1920;
            m_StreamConfig.height = 1080;
        }
    }

    // Test if audio works at the specified audio configuration
    bool audioTestPassed = testAudio(m_StreamConfig.audioConfiguration);

    // Gracefully degrade to stereo if surround sound doesn't work
    if (!audioTestPassed && CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration) > 2) {
        audioTestPassed = testAudio(AUDIO_CONFIGURATION_STEREO);
        if (audioTestPassed) {
            m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
            emitLaunchWarning(tr("Your selected surround sound setting is not supported by the current audio device."));
        }
    }

    // If nothing worked, warn the user that audio will not work
    if (!audioTestPassed) {
        emitLaunchWarning(tr("Failed to open audio device. Audio will be unavailable during this session."));
    }

    // Check for unmapped gamepads
    if (!SdlInputHandler::getUnmappedGamepads().isEmpty()) {
        emitLaunchWarning(tr("An attached gamepad has no mapping and won't be usable. Visit the Moonlight help to resolve this."));
    }

    // If we removed all codecs with the checks above, use H.264 as the codec of last resort.
    if (m_SupportedVideoFormats.empty()) {
        m_SupportedVideoFormats.append(VIDEO_FORMAT_H264);
    }

    // NVENC will fail to initialize when any dimension exceeds 4096 using:
    // - H.264 on all versions of NVENC
    // - HEVC prior to Pascal
    //
    // However, if we aren't using Nvidia hosting software, don't assume anything about
    // encoding capabilities by using HEVC Main 10 support. It will likely be wrong.
    if ((m_StreamConfig.width > 4096 || m_StreamConfig.height > 4096) && m_Computer->isNvidiaServerSoftware) {
        // Pascal added support for 8K HEVC encoding support. Maxwell 2 could encode HEVC but only up to 4K.
        // We can't directly identify Pascal, but we can look for HEVC Main10 which was added in the same generation.
        if (m_Computer->maxLumaPixelsHEVC == 0 || !(m_Computer->serverCodecModeSupport & SCM_HEVC_MAIN10)) {
            emit displayLaunchError(tr("Your host PC's GPU doesn't support streaming video resolutions over 4K."));
            return false;
        }
        else if ((m_SupportedVideoFormats & ~VIDEO_FORMAT_MASK_H264) == 0) {
            emit displayLaunchError(tr("Video resolutions over 4K are not supported by the H.264 codec."));
            return false;
        }
    }

    if (m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_FORCE_HARDWARE &&
            !(m_SupportedVideoFormats & VIDEO_FORMAT_MASK_10BIT) && // HDR was already checked for hardware decode support above
            getDecoderAvailability(testWindow,
                                   m_Preferences->videoDecoderSelection,
                                   m_SupportedVideoFormats.front(),
                                   m_StreamConfig.width,
                                   m_StreamConfig.height,
                                   m_StreamConfig.fps) != DecoderAvailability::Hardware) {
        if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_AUTO) {
            emit displayLaunchError(tr("Your selection to force hardware decoding cannot be satisfied due to missing hardware decoding support on this PC's GPU."));
        }
        else {
            emit displayLaunchError(tr("Your codec selection and force hardware decoding setting are not compatible. This PC's GPU lacks support for decoding your chosen codec."));
        }

        // Fail the launch, because we won't manage to get a decoder for the actual stream
        return false;
    }

    return true;
}


class DeferredSessionCleanupTask : public QRunnable
{
public:
    DeferredSessionCleanupTask(Session* session) :
        m_Session(session) {}

private:
    virtual ~DeferredSessionCleanupTask() override
    {
        // Allow another session to start now that we're cleaned up
        Session::s_ActiveSession = nullptr;
        Session::s_ActiveSessionSemaphore.release();

        // Notify that the session is ready to be cleaned up
        emit m_Session->readyForDeletion();
    }

    void run() override
    {
        // Only quit the running app if our session terminated gracefully
        bool shouldQuit =
                !m_Session->m_UnexpectedTermination &&
                m_Session->m_Preferences->quitAppAfter;

        // Notify the UI
        if (shouldQuit) {
            emit m_Session->quitStarting();
        }
        else {
            emit m_Session->sessionFinished(m_Session->m_PortTestResults);
        }

        // The video decoder must already be destroyed, since it could
        // try to interact with APIs that can only be called between
        // LiStartConnection() and LiStopConnection().
        SDL_assert(m_Session->m_VideoDecoder == nullptr);

        // Finish cleanup of the connection state
        QMetaObject::invokeMethod(m_Session, &Session::stopMicrophone, Qt::BlockingQueuedConnection);
        LiStopConnection();

        // Perform a best-effort app quit
        if (shouldQuit) {
            NvHTTP http(m_Session->m_Computer);

            // Logging is already done inside NvHTTP
            try {
                http.quitApp();
            } catch (const GfeHttpResponseException&) {
            } catch (const QtNetworkReplyException&) {
            }

            // Session is finished now
            emit m_Session->sessionFinished(m_Session->m_PortTestResults);
        }

        // Exit the entire program if requested
        if (m_Session->m_ShouldExit) {
            QCoreApplication::instance()->quit();
        }
    }

    Session* m_Session;
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
        Q_ASSERT(m_QtWindow != nullptr);
        if (m_QtWindow != nullptr) {
            QScreen* screen = m_QtWindow->screen();
            if (screen != nullptr) {
                QRect displayRect = screen->geometry();

                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Qt UI screen is at (%d,%d)",
                            displayRect.x(), displayRect.y());
                for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
                    SDL_Rect displayBounds;

                    if (SDL_GetDisplayBounds(i, &displayBounds) == 0) {
                        if (displayBounds.x == displayRect.x() &&
                            displayBounds.y == displayRect.y()) {
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
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Qt window is not associated with a QScreen!");
            }
        }
    }

    SDL_Rect usableBounds;
    if (SDL_GetDisplayUsableBounds(displayIndex, &usableBounds) == 0) {
        // If the stream resolution fits within the usable display area, use it directly
        if (m_StreamConfig.width <= usableBounds.w &&
            m_StreamConfig.height <= usableBounds.h) {
            width = m_StreamConfig.width;
            height = m_StreamConfig.height;
        } else {
            // Otherwise, use 80% of usable bounds and preserve aspect ratio
            SDL_Rect src, dst;
            src.x = src.y = dst.x = dst.y = 0;
            src.w = m_StreamConfig.width;
            src.h = m_StreamConfig.height;

            dst.w = ((int)(usableBounds.w * 0.80f)) & ~0x1;  // even width
            dst.h = ((int)(usableBounds.h * 0.80f)) & ~0x1;  // even height

            StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

            width = dst.w;
            height = dst.h;
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
            SDL_Rect safeArea;
            if (!StreamUtils::getNativeDesktopMode(displayIndex, &desktopMode, &safeArea)) {
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

    // On devices with slow GPUs, we will try to match the display mode
    // to the video stream to offload the scaling work to the display.
    //
    // We also try to match the video resolution if we're using KMSDRM,
    // because scaling on the display is generally higher quality than
    // scaling performed by drmModeSetPlane().
    bool matchVideo;
    if (!Utils::getEnvironmentVariableOverride("MATCH_DISPLAY_MODE_TO_VIDEO", &matchVideo)) {
        matchVideo = WMUtils::isGpuSlow() || QString(SDL_GetCurrentVideoDriver()) == "KMSDRM";
    }

    bestMode = desktopMode;
    bestMode.refresh_rate = 0;
    if (!matchVideo) {
        // Start with the native desktop resolution and try to find
        // the highest refresh rate that our stream FPS evenly divides.
        int numDisplayModes = SDL_GetNumDisplayModes(displayIndex);
        for (int i = 0; i < numDisplayModes; i++) {
            if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0) {
                if (mode.w == desktopMode.w && mode.h == desktopMode.h &&
                    mode.refresh_rate % m_StreamConfig.fps == 0) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Found display mode with desktop resolution: %dx%dx%d",
                                mode.w, mode.h, mode.refresh_rate);
                    if (mode.refresh_rate > bestMode.refresh_rate) {
                        bestMode = mode;
                    }
                }
            }
        }
    }

    // If we didn't find a mode that matched the current resolution and
    // had a high enough refresh rate, start looking for lower resolution
    // modes that can meet the required refresh rate and minimum video
    // resolution. We will also try to pick a display mode that matches
    // aspect ratio closest to the video stream.
    if (bestMode.refresh_rate == 0) {
        float bestModeAspectRatio = 0;
        float videoAspectRatio = (float)m_ActiveVideoWidth / (float)m_ActiveVideoHeight;
        int numDisplayModes = SDL_GetNumDisplayModes(displayIndex);
        for (int i = 0; i < numDisplayModes; i++) {
            if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0) {
                float modeAspectRatio = (float)mode.w / (float)mode.h;
                if (mode.w >= m_ActiveVideoWidth && mode.h >= m_ActiveVideoHeight &&
                        mode.refresh_rate % m_StreamConfig.fps == 0) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Found display mode with video resolution: %dx%dx%d",
                                mode.w, mode.h, mode.refresh_rate);
                    if (mode.refresh_rate >= bestMode.refresh_rate &&
                            (bestModeAspectRatio == 0 || fabs(videoAspectRatio - modeAspectRatio) <= fabs(videoAspectRatio - bestModeAspectRatio))) {
                        bestMode = mode;
                        bestModeAspectRatio = modeAspectRatio;
                    }
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
                    "No matching display mode found; using desktop mode");
        bestMode = desktopMode;
    }

    if ((SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
        // Only print when the window is actually in full-screen exclusive mode,
        // otherwise we're not actually using the mode we've set here
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Chosen best display mode: %dx%dx%d",
                    bestMode.w, bestMode.h, bestMode.refresh_rate);
    }

    SDL_SetWindowDisplayMode(m_Window, &bestMode);
}

void Session::toggleFullscreen()
{
    bool fullScreen = !(SDL_GetWindowFlags(m_Window) & m_FullScreenFlag);

#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN)
    // Destroy the video decoder before toggling full-screen because D3D9 can try
    // to put the window back into full-screen before we've managed to destroy
    // the renderer. This leads to excessive flickering and can cause the window
    // decorations to get messed up as SDL and D3D9 fight over the window style.
    //
    // On Apple Silicon Macs, the AVSampleBufferDisplayLayer may cause WindowServer
    // to deadlock when transitioning out of fullscreen. Destroy the decoder before
    // exiting fullscreen as a workaround. See issue #973.
    SDL_LockMutex(m_DecoderLock);
    delete m_VideoDecoder;
    m_VideoDecoder = nullptr;
    SDL_UnlockMutex(m_DecoderLock);
#endif

    // Actually enter/leave fullscreen
    SDL_SetWindowFullscreen(m_Window, fullScreen ? m_FullScreenFlag : 0);

#ifdef Q_OS_DARWIN
    // SDL on macOS has a bug that causes the window size to be reset to crazy
    // large dimensions when exiting out of true fullscreen mode. We can work
    // around the issue by manually resetting the position and size here.
    if (!fullScreen && m_FullScreenFlag == SDL_WINDOW_FULLSCREEN) {
        int x, y, width, height;
        getWindowDimensions(x, y, width, height);
        SDL_SetWindowSize(m_Window, width, height);
        SDL_SetWindowPosition(m_Window, x, y);
    }
#endif

    // Input handler might need to start/stop keyboard grab after changing modes
    m_InputHandler->updateKeyboardGrabState();

    // Input handler might need stop/stop mouse grab after changing modes
    m_InputHandler->updatePointerRegionLock();
}

// ---- Qt-based overlay menu methods ----

void Session::showQtOverlayMenu()
{
    if (!m_MenuPanel || m_MenuPanel->isMenuVisible() || m_MenuPanel->isClosing()) return;
    if (!isStreamingWindowVisible()) return;

    // Check if overlay menu is disabled before releasing mouse capture
    if (m_Preferences->overlayMenuPosition == StreamingPreferences::OMP_DISABLED) {
        return; // Do not show
    }

    // Save capture state and release mouse
    m_WasCapturedBeforeMenu = m_InputHandler->isCaptureActive();
    if (m_WasCapturedBeforeMenu) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_ENABLE);
    }

    // Flush stale mouse motion events from relative mode
    SDL_FlushEvent(SDL_MOUSEMOTION);

    // Get SDL window position and size in screen coordinates
    int wx, wy, ww, wh;
    SDL_GetWindowPosition(m_Window, &wx, &wy);
    SDL_GetWindowSize(m_Window, &ww, &wh);

    // Update dynamic state before showing
    m_MenuPanel->updateMicrophoneState(m_MicStream != nullptr);
    m_MenuPanel->updateBitrateState(m_Preferences->bitrateKbps);
    m_MenuPanel->setHasGamepads(m_InputHandler->getAttachedGamepadMask() != 0);
    m_MenuPanel->updateGamepadMouseState(m_InputHandler->isMouseEmulationActive());
    updateFileMappingMenuState();

    // Show menu based on user preference
    switch (m_Preferences->overlayMenuPosition) {
    case StreamingPreferences::OMP_LEFT_EDGE:
        m_MenuPanel->showAtLeftEdge(wx, wy, ww, wh);
        break;
    case StreamingPreferences::OMP_BUTTON:
        // Show menu at the button's position (top-right corner)
        m_MenuPanel->showAtCursor(wx, wy, ww, wh,
                                  wx + ww - 40, wy + 40);
        // Hide button while menu is visible
        if (m_MenuButton) {
            m_MenuButton->hideButton();
        }
        break;
    case StreamingPreferences::OMP_RIGHT_EDGE:
    default:
        m_MenuPanel->showAtRightEdge(wx, wy, ww, wh);
        break;
    }

    // Pump Qt events immediately to trigger first paint
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Qt overlay menu shown at (%d,%d) %dx%d", wx, wy, ww, wh);
}

void Session::hideQtOverlayMenu()
{
    if (!m_MenuPanel || !m_MenuPanel->isMenuVisible()) return;
    m_MenuPanel->closeMenu();
    // Note: close callback handles mouse capture restore
}

void Session::toggleQtOverlayMenu()
{
    if (!m_MenuPanel) return;

    if (m_MenuPanel->isMenuVisible()) {
        hideQtOverlayMenu();
    } else {
        showQtOverlayMenu();
    }
}

bool Session::isStreamingWindowVisible() const
{
    if (m_Window == nullptr) {
        return false;
    }

    Uint32 windowFlags = SDL_GetWindowFlags(m_Window);
    return (windowFlags & SDL_WINDOW_SHOWN) &&
           !(windowFlags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED));
}

void Session::syncQtOverlayWindowsWithSdlWindowState()
{
    if (!isStreamingWindowVisible()) {
        if (m_MenuPanel && m_MenuPanel->isMenuVisible()) {
            m_MenuPanel->closeMenu();
        }
        if (m_MenuButton) {
            m_MenuButton->hideButton();
        }
        if (m_Toast && m_Toast->isVisible()) {
            m_Toast->hide();
        }
        return;
    }

    if (!m_MenuButton) {
        return;
    }

    if (m_Preferences->overlayMenuPosition != StreamingPreferences::OMP_BUTTON ||
        (m_MenuPanel && (m_MenuPanel->isMenuVisible() || m_MenuPanel->isClosing()))) {
        m_MenuButton->hideButton();
        return;
    }

    int wx, wy, ww, wh;
    SDL_GetWindowPosition(m_Window, &wx, &wy);
    SDL_GetWindowSize(m_Window, &ww, &wh);
    m_MenuButton->showButton(wx, wy, ww, wh);
}

void Session::dispatchQtMenuAction(OverlayMenuPanel::MenuAction action)
{
    // Map OverlayMenuPanel::MenuAction to SdlInputHandler::KeyCombo
    SdlInputHandler::KeyCombo combo;
    switch (action) {
    case OverlayMenuPanel::MenuAction::Quit:
        combo = SdlInputHandler::KeyComboQuit;
        break;
    case OverlayMenuPanel::MenuAction::QuitAndExit:
        combo = SdlInputHandler::KeyComboQuitAndExit;
        break;
    case OverlayMenuPanel::MenuAction::ToggleFullScreen:
        combo = SdlInputHandler::KeyComboToggleFullScreen;
        // Defer capture restore until after fullscreen toggle completes
        m_DeferCaptureRestore = true;
        break;
    case OverlayMenuPanel::MenuAction::ToggleStatsOverlay:
        combo = SdlInputHandler::KeyComboToggleStatsOverlay;
        break;
    case OverlayMenuPanel::MenuAction::ToggleMouseMode:
        combo = SdlInputHandler::KeyComboToggleMouseMode;
        break;
    case OverlayMenuPanel::MenuAction::ToggleCursorHide:
        combo = SdlInputHandler::KeyComboToggleCursorHide;
        break;
    case OverlayMenuPanel::MenuAction::ToggleMinimize:
        combo = SdlInputHandler::KeyComboToggleMinimize;
        break;
    case OverlayMenuPanel::MenuAction::UngrabInput:
        combo = SdlInputHandler::KeyComboUngrabInput;
        break;
    case OverlayMenuPanel::MenuAction::PasteText:
        combo = SdlInputHandler::KeyComboPasteText;
        break;
    case OverlayMenuPanel::MenuAction::TogglePointerRegionLock:
        combo = SdlInputHandler::KeyComboTogglePointerRegionLock;
        break;

    case OverlayMenuPanel::MenuAction::ShowHostFiles:
        appendFileMappingDiagnostic(
                QStringLiteral("overlay.click_host_files"),
                QStringLiteral("state=%1 detail=%2 mount_path=%3")
                        .arg(fileMappingStateName(m_FileMappingState),
                             m_FileMappingDetail,
                             m_FileMappingMountPath),
                m_Computer ? m_Computer->uuid : QString(),
                m_FileMappingSessionId);
        if (m_FileMappingState == OverlayMenuPanel::FileMappingState::Checking ||
            m_FileMappingState == OverlayMenuPanel::FileMappingState::Unknown) {
            showStreamingToast(tr("Checking host file sharing..."), 2000);
        }
        else if (m_FileMappingState == OverlayMenuPanel::FileMappingState::Mounting) {
            showStreamingToast(tr("Preparing host files..."), 2000);
        }
        else if (m_FileMappingState == OverlayMenuPanel::FileMappingState::Open &&
                 !m_FileMappingMountPath.isEmpty()) {
            openFileMappingMountPath();
            showStreamingToast(tr("Opening host files..."), 2000);
        }
        else if (m_FileMappingState == OverlayMenuPanel::FileMappingState::Available) {
            startFileMappingMount();
        }
        else if (m_FileMappingState == OverlayMenuPanel::FileMappingState::Error ||
                 m_FileMappingState == OverlayMenuPanel::FileMappingState::Unavailable) {
            showStreamingToast(m_FileMappingToast.isEmpty()
                               ? tr("Host file sharing is not available. Retrying...")
                               : m_FileMappingToast + tr(" Retrying..."),
                               4500);
            startFileMappingUxProbe();
        }
        else {
            showStreamingToast(tr("Host file sharing is not available."), 3000);
        }
        return;

    // --- Microphone toggle ---
    // Deferred: toggle mic outside processEvents() to avoid QAudioSource heap corruption
    case OverlayMenuPanel::MenuAction::ToggleMicrophone:
        m_PendingMicToggle = true;
        return;

    // --- Gamepad mouse emulation toggle ---
    // Immediately activates/deactivates mouse emulation on the connected gamepad
    case OverlayMenuPanel::MenuAction::ToggleGamepadMouse:
    {
        if (m_InputHandler) {
            bool nowActive = m_InputHandler->toggleGamepadMouseEmulation();
            if (m_MenuPanel) {
                m_MenuPanel->updateGamepadMouseState(nowActive);
            }
        }
        return;
    }

    // --- Bitrate presets ---
    case OverlayMenuPanel::MenuAction::SetBitrate1000:
    case OverlayMenuPanel::MenuAction::SetBitrate2000:
    case OverlayMenuPanel::MenuAction::SetBitrate5000:
    case OverlayMenuPanel::MenuAction::SetBitrate10000:
    case OverlayMenuPanel::MenuAction::SetBitrate20000:
    case OverlayMenuPanel::MenuAction::SetBitrate30000:
    case OverlayMenuPanel::MenuAction::SetBitrate50000:
    case OverlayMenuPanel::MenuAction::SetBitrate100000:
    {
        static const int kBitrateMap[] = {
            1000, 2000, 5000, 10000, 20000, 30000, 50000, 100000
        };
        int idx = (int)action - (int)OverlayMenuPanel::MenuAction::SetBitrate1000;
        if (idx >= 0 && idx < 8) {
            int newBitrate = kBitrateMap[idx];
            // Save preference for future sessions
            m_Preferences->bitrateKbps = newBitrate;
            m_Preferences->save();
            // Try to change bitrate in the current session via Sunshine API
            requestRuntimeBitrateChange(newBitrate);
            // Show toast notification
            if (newBitrate >= 1000) {
                showStreamingToast(QString("Bitrate: %1 Mbps").arg(newBitrate / 1000));
            } else {
                showStreamingToast(QString("Bitrate: %1 Kbps").arg(newBitrate));
            }
        }
        return;
    }

    default:
        return;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Qt overlay menu action: %d", (int)action);
    m_InputHandler->performSpecialKeyCombo(combo);

    // Restore mouse capture after window-state-changing actions complete
    if (m_DeferCaptureRestore) {
        m_DeferCaptureRestore = false;
        if (m_WasCapturedBeforeMenu) {
            // Allow the window to settle after fullscreen/minimize toggle
            SDL_Delay(100);
            SDL_FlushEvent(SDL_MOUSEMOTION);

            // Recapture via the proper input handler path, which handles
            // pointer region lock, keyboard grab, and relative mouse mode
            m_InputHandler->setCaptureActive(true);
            m_WasCapturedBeforeMenu = false;

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Deferred capture restore completed after window state change");
        }
    }
}

void Session::showStreamingToast(const QString& message, int durationMs)
{
    if (!m_Toast) return;

    int wx, wy, ww, wh;
    SDL_GetWindowPosition(m_Window, &wx, &wy);
    SDL_GetWindowSize(m_Window, &ww, &wh);
    m_Toast->showToast(wx, wy, ww, wh, message, durationMs);
    QCoreApplication::processEvents();
}

void Session::updateFileMappingMenuState()
{
    if (m_MenuPanel) {
        m_MenuPanel->updateFileMappingState(m_FileMappingState, m_FileMappingDetail);
    }
}

bool Session::openFileMappingMountPath()
{
    if (m_FileMappingMountPath.isEmpty()) {
        appendFileMappingDiagnostic(
                QStringLiteral("finder_reveal.skip"),
                QStringLiteral("empty_mount_path"),
                m_Computer ? m_Computer->uuid : QString(),
                m_FileMappingSessionId);
        return false;
    }

    const QString markerPath = QDir(m_FileMappingMountPath).filePath(QStringLiteral("README.txt"));
    appendFileMappingDiagnostic(
            QStringLiteral("finder_reveal.start"),
            QStringLiteral("mount_path=%1 marker=%2 marker_exists=%3")
                    .arg(m_FileMappingMountPath,
                         markerPath,
                         QFileInfo::exists(markerPath) ? QStringLiteral("true") : QStringLiteral("false")),
            m_Computer ? m_Computer->uuid : QString(),
            m_FileMappingSessionId);

#if defined(Q_OS_MACOS)
    if (QFileInfo::exists(markerPath) &&
            QProcess::startDetached(QStringLiteral("/usr/bin/open"),
                                    { QStringLiteral("-R"),
                                      markerPath })) {
        appendFileMappingDiagnostic(
                QStringLiteral("finder_reveal.open_r"),
                QStringLiteral("ok=true marker=%1").arg(markerPath),
                m_Computer ? m_Computer->uuid : QString(),
                m_FileMappingSessionId);
        return true;
    }
    if (QProcess::startDetached(QStringLiteral("/usr/bin/open"),
                                { m_FileMappingMountPath })) {
        appendFileMappingDiagnostic(
                QStringLiteral("finder_reveal.open_folder"),
                QStringLiteral("ok=true mount_path=%1").arg(m_FileMappingMountPath),
                m_Computer ? m_Computer->uuid : QString(),
                m_FileMappingSessionId);
        return true;
    }
#elif defined(Q_OS_WIN32) || defined(Q_OS_WIN)
    if (QFileInfo::exists(markerPath) &&
            QProcess::startDetached(QStringLiteral("explorer.exe"),
                                    { QStringLiteral("/select,%1").arg(QDir::toNativeSeparators(markerPath)) })) {
        appendFileMappingDiagnostic(
                QStringLiteral("explorer_reveal.open_select"),
                QStringLiteral("ok=true marker=%1").arg(markerPath),
                m_Computer ? m_Computer->uuid : QString(),
                m_FileMappingSessionId);
        return true;
    }
    if (QProcess::startDetached(QStringLiteral("explorer.exe"),
                                { QDir::toNativeSeparators(m_FileMappingMountPath) })) {
        appendFileMappingDiagnostic(
                QStringLiteral("explorer_reveal.open_folder"),
                QStringLiteral("ok=true mount_path=%1").arg(m_FileMappingMountPath),
                m_Computer ? m_Computer->uuid : QString(),
                m_FileMappingSessionId);
        return true;
    }
#endif

    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(m_FileMappingMountPath));
    appendFileMappingDiagnostic(
            QStringLiteral("finder_reveal.desktop_services"),
            QStringLiteral("ok=%1 mount_path=%2")
                    .arg(opened ? QStringLiteral("true") : QStringLiteral("false"),
                         m_FileMappingMountPath),
            m_Computer ? m_Computer->uuid : QString(),
            m_FileMappingSessionId);
    return opened;
}

void Session::requestRuntimeBitrateChange(int bitrateKbps)
{
    if (!m_Computer) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Cannot change runtime bitrate: no m_Computer");
        return;
    }

    try {
        NvHTTP http(m_Computer);

        // Build clientname the same way as openConnection() does for /launch
        QString clientname = QHostInfo::localHostName();
        if (!m_Computer->uuid.isEmpty()) {
            QString pairname = NvComputer::getPairname(m_Computer->uuid);
            if (!pairname.isEmpty()) {
                clientname = pairname;
            }
        }

        QString args = QString("bitrate=%1&clientname=%2")
                           .arg(bitrateKbps)
                           .arg(clientname);

        QString response = http.openConnectionToString(
            http.m_BaseUrlHttps,
            "bitrate",
            args,
            5000,
            NvHTTP::NVLL_VERBOSE);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Runtime bitrate change to %d kbps: %s",
                    bitrateKbps,
                    response.toUtf8().constData());

        m_AbrCurrentBitrateKbps->store(bitrateKbps);
    }
    catch (const GfeHttpResponseException& e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Runtime bitrate change failed (HTTP): %s",
                     e.toQString().toUtf8().constData());
    }
    catch (const QtNetworkReplyException& e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Runtime bitrate change failed (Network): %s",
                     e.toQString().toUtf8().constData());
    }
    catch (...) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Runtime bitrate change failed: unknown error");
    }
}

void Session::startSunshineAbr()
{
    m_SunshineAbrEnabled = false;

    if (!m_Preferences->enableSunshineAbr || !m_Computer) {
        return;
    }

    try {
        NvHTTP http(m_Computer);

        int hostMaxBitrate = 0;
        if (!http.getAbrCapabilities(&hostMaxBitrate)) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Host does not advertise Sunshine ABR support");
            return;
        }

        QJsonObject configResponse = http.configureAbr(true,
                                                       0,
                                                       m_Preferences->bitrateKbps,
                                                       "balanced",
                                                       2000);
        if (!configResponse.value("success").toBool(false)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Sunshine ABR configure rejected by host");
            return;
        }

        int initialBitrate = configResponse.value("initialBitrate").toInt(m_StreamConfig.bitrate);
        m_AbrCurrentBitrateKbps->store(initialBitrate);
        m_AbrFeedbackInFlight->store(false);

        const RTP_VIDEO_STATS* videoStats = LiGetRTPVideoStats();
        if (videoStats != nullptr) {
            memcpy(&m_LastAbrVideoStats, videoStats, sizeof(m_LastAbrVideoStats));
        }
        else {
            memset(&m_LastAbrVideoStats, 0, sizeof(m_LastAbrVideoStats));
        }

        m_LastAbrFeedbackTicks = SDL_GetTicks();
        m_SunshineAbrEnabled = true;

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Sunshine ABR enabled: initial=%d Kbps, max=%d Kbps, hostMax=%d Kbps",
                    initialBitrate,
                    configResponse.value("maxBitrate").toInt(0),
                    hostMaxBitrate);
    }
    catch (const std::exception& e) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Sunshine ABR unavailable: %s",
                    e.what());
    }
    catch (...) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Sunshine ABR unavailable: unknown error");
    }
}

void Session::stopSunshineAbr()
{
    if (!m_SunshineAbrEnabled || !m_Computer) {
        m_SunshineAbrEnabled = false;
        return;
    }

    m_SunshineAbrEnabled = false;

    try {
        NvHTTP http(m_Computer);
        http.configureAbr(false, 0, 0, "balanced", 1000);
    }
    catch (const std::exception& e) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Sunshine ABR disable failed: %s",
                    e.what());
    }
    catch (...) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Sunshine ABR disable failed: unknown error");
    }
}

void Session::startFileMappingUxProbe()
{
    appendFileMappingDiagnostic(
            QStringLiteral("ux_probe.request"),
            QStringLiteral("current_state=%1").arg(fileMappingStateName(m_FileMappingState)),
            m_Computer ? m_Computer->uuid : QString(),
            m_FileMappingSessionId);

    m_FileMappingState = OverlayMenuPanel::FileMappingState::Checking;
    m_FileMappingDetail = tr("Checking");
    m_FileMappingToast = tr("Checking host file sharing...");
    m_FileMappingToastPending = false;
    updateFileMappingMenuState();

    if (m_Computer == nullptr) {
        m_FileMappingState = OverlayMenuPanel::FileMappingState::Unavailable;
        m_FileMappingDetail = tr("Unavailable");
        m_FileMappingToast = tr("Host file sharing is not available.");
        appendFileMappingDiagnostic(
                QStringLiteral("ux_probe.no_computer"),
                m_FileMappingToast,
                QString(),
                m_FileMappingSessionId);
        updateFileMappingMenuState();
        return;
    }

    NvComputer computerSnapshot;
    {
        QReadLocker locker(&m_Computer->lock);
        computerSnapshot = *m_Computer;
    }

    m_FileMappingProbeState = std::make_shared<FileMappingUx::ProbeState>();
    FileMappingUx::startCapabilityProbe(std::move(computerSnapshot),
                                        m_FileMappingProbeState,
                                        2500);
}

void Session::processFileMappingUxProbeResult()
{
    if (!m_FileMappingProbeState) {
        return;
    }

    bool available = false;
    bool error = false;
    QString detail;
    QString message;
    QString diagnosticsPath;
    {
        QMutexLocker locker(&m_FileMappingProbeState->lock);
        if (!m_FileMappingProbeState->pending) {
            return;
        }
        m_FileMappingProbeState->pending = false;
        available = m_FileMappingProbeState->available;
        error = m_FileMappingProbeState->error;
        detail = m_FileMappingProbeState->detail;
        message = m_FileMappingProbeState->message;
        diagnosticsPath = m_FileMappingProbeState->diagnosticsPath;
    }

    m_FileMappingState = available ? OverlayMenuPanel::FileMappingState::Available :
                         error ? OverlayMenuPanel::FileMappingState::Error :
                         OverlayMenuPanel::FileMappingState::Unavailable;
    m_FileMappingDetail = detail;
    m_FileMappingToast = message;
    m_FileMappingToastPending = available;
    updateFileMappingMenuState();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "File mapping UX probe: state=%d detail=%s message=%s diagnostics=%s",
                static_cast<int>(m_FileMappingState),
                m_FileMappingDetail.toUtf8().constData(),
                m_FileMappingToast.toUtf8().constData(),
                diagnosticsPath.toUtf8().constData());

    if (m_FileMappingToastPending) {
        m_FileMappingToastPending = false;
        showStreamingToast(m_FileMappingToast, 2500);
    }
}

void Session::startFileMappingMount()
{
    appendFileMappingDiagnostic(
            QStringLiteral("mount.request"),
            QStringLiteral("current_state=%1 existing_task=%2 mount_path=%3")
                    .arg(fileMappingStateName(m_FileMappingState),
                         m_FileMappingMountState ? QStringLiteral("true") : QStringLiteral("false"),
                         m_FileMappingMountPath),
            m_Computer ? m_Computer->uuid : QString(),
            m_FileMappingSessionId);

    if (m_FileMappingMountState) {
        showStreamingToast(tr("Preparing host files..."), 2000);
        return;
    }

    if (!m_FileMappingMountPath.isEmpty()) {
        openFileMappingMountPath();
        showStreamingToast(tr("Opening host files..."), 2000);
        return;
    }

    m_FileMappingState = OverlayMenuPanel::FileMappingState::Mounting;
    m_FileMappingDetail = tr("Preparing");
    m_FileMappingToast = tr("Preparing host files...");
    m_FileMappingToastPending = false;
    updateFileMappingMenuState();
    showStreamingToast(m_FileMappingToast, 2000);

    if (m_Computer == nullptr) {
        m_FileMappingState = OverlayMenuPanel::FileMappingState::Unavailable;
        m_FileMappingDetail = tr("Unavailable");
        m_FileMappingToast = tr("Host file sharing is not available.");
        appendFileMappingDiagnostic(
                QStringLiteral("mount.no_computer"),
                m_FileMappingToast,
                QString(),
                m_FileMappingSessionId);
        updateFileMappingMenuState();
        return;
    }

    NvComputer computerSnapshot;
    {
        QReadLocker locker(&m_Computer->lock);
        computerSnapshot = *m_Computer;
    }

    m_FileMappingMountState = std::make_shared<FileMappingUx::MountState>();
    FileMappingUx::startMount(std::move(computerSnapshot),
                              m_FileMappingSessionId,
                              m_FileMappingMountState,
                              8000);
}

void Session::processFileMappingMountResult()
{
    if (!m_FileMappingMountState) {
        return;
    }

    bool ok = false;
    QString detail;
    QString message;
    QString displayPath;
    QString diagnosticsPath;
    {
        QMutexLocker locker(&m_FileMappingMountState->lock);
        if (!m_FileMappingMountState->pending) {
            return;
        }
        m_FileMappingMountState->pending = false;
        ok = m_FileMappingMountState->ok;
        detail = m_FileMappingMountState->detail;
        message = m_FileMappingMountState->message;
        displayPath = m_FileMappingMountState->displayPath;
        diagnosticsPath = m_FileMappingMountState->diagnosticsPath;
    }
    m_FileMappingMountState.reset();

    appendFileMappingDiagnostic(
            QStringLiteral("mount.result"),
            QStringLiteral("ok=%1 detail=%2 message=%3 display_path=%4 diagnostics=%5")
                    .arg(ok ? QStringLiteral("true") : QStringLiteral("false"),
                         detail,
                         message,
                         displayPath,
                         diagnosticsPath),
            m_Computer ? m_Computer->uuid : QString(),
            m_FileMappingSessionId);

    if (ok && !displayPath.isEmpty()) {
        m_FileMappingMountPath = displayPath;
        m_FileMappingState = OverlayMenuPanel::FileMappingState::Open;
        m_FileMappingDetail = detail.isEmpty() ? tr("Open") : detail;
        m_FileMappingToast = message.isEmpty() ? tr("Host files are ready.") : message;
        updateFileMappingMenuState();
        const bool opened = openFileMappingMountPath();
        showStreamingToast(opened
                           ? m_FileMappingToast
                           : tr("Host files are ready, but the folder did not open. Check %1.").arg(m_FileMappingMountPath),
                           3000);
    }
    else {
        m_FileMappingMountPath.clear();
        m_FileMappingState = OverlayMenuPanel::FileMappingState::Error;
        m_FileMappingDetail = detail.isEmpty() ? tr("Error") : detail;
        m_FileMappingToast = message.isEmpty() ? tr("Host files could not be opened.") : message;
        updateFileMappingMenuState();
        showStreamingToast(m_FileMappingToast, 4500);
    }
}

void Session::cleanupFileMappingMount()
{
    appendFileMappingDiagnostic(
            QStringLiteral("mount.cleanup"),
            QStringLiteral("mount_path=%1").arg(m_FileMappingMountPath),
            m_Computer ? m_Computer->uuid : QString(),
            m_FileMappingSessionId);
    m_FileMappingMountState.reset();
    const QString mountPath = m_FileMappingMountPath;
    m_FileMappingMountPath.clear();
    if (!mountPath.isEmpty()) {
        const bool generatedMirrorPath = isMoonlightGeneratedFileMappingMirrorPath(mountPath);
#if defined(Q_OS_MACOS)
        if (generatedMirrorPath) {
            QProcess::execute(QStringLiteral("/sbin/umount"), { mountPath });
        }
#endif
        if (generatedMirrorPath) {
            const bool removed = QDir(mountPath).removeRecursively();
            appendFileMappingDiagnostic(
                    removed ? QStringLiteral("mount.cleanup.delete")
                            : QStringLiteral("mount.cleanup.delete_failed"),
                    QStringLiteral("mount_path=%1").arg(mountPath),
                    m_Computer ? m_Computer->uuid : QString(),
                    m_FileMappingSessionId);
        }
        else {
            appendFileMappingDiagnostic(
                    QStringLiteral("mount.cleanup.skip_delete"),
                    QStringLiteral("mount_path=%1 reason=outside_moonlight_mirror").arg(mountPath),
                    m_Computer ? m_Computer->uuid : QString(),
                    m_FileMappingSessionId);
        }
    }
}

void Session::startFileMappingSmokeProbe()
{
    if (qEnvironmentVariableIntValue("MOONLIGHT_FILE_MAPPING_SMOKE") == 0) {
        return;
    }

    if (m_Computer == nullptr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "File mapping smoke skipped: no active computer");
        return;
    }

    const QString mappingId = QString::fromLocal8Bit(qgetenv("MOONLIGHT_FILE_MAPPING_SMOKE_MAPPING")).trimmed();
    const QString path = QString::fromLocal8Bit(qgetenv("MOONLIGHT_FILE_MAPPING_SMOKE_PATH")).trimmed();
    if (mappingId.isEmpty() || path.isEmpty()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "File mapping smoke skipped: set MOONLIGHT_FILE_MAPPING_SMOKE_MAPPING and MOONLIGHT_FILE_MAPPING_SMOKE_PATH");
        return;
    }

    const int timeoutMs = readBoundedEnvInt("MOONLIGHT_FILE_MAPPING_SMOKE_TIMEOUT_MS", 5000, 1000, 60000);
    const int offset = readBoundedEnvInt("MOONLIGHT_FILE_MAPPING_SMOKE_OFFSET", 0, 0, 1024 * 1024 * 1024);
    const int length = readBoundedEnvInt("MOONLIGHT_FILE_MAPPING_SMOKE_LENGTH", 4096, 1, 1024 * 1024);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Starting file mapping smoke probe: mapping=%s path=%s",
                mappingId.toUtf8().constData(),
                path.toUtf8().constData());

    NvComputer computerSnapshot;
    {
        QReadLocker locker(&m_Computer->lock);
        computerSnapshot = *m_Computer;
    }

    QThreadPool::globalInstance()->start(new FileMappingSmokeTask(std::move(computerSnapshot),
                                                                  mappingId,
                                                                  path,
                                                                  static_cast<quint64>(offset),
                                                                  static_cast<quint32>(length),
                                                                  timeoutMs));
}

void Session::sendSunshineAbrFeedback()
{
    if (!m_SunshineAbrEnabled || !m_Computer) {
        return;
    }

    if (m_AbrFeedbackInFlight->exchange(true)) {
        return;
    }

    const RTP_VIDEO_STATS* videoStats = LiGetRTPVideoStats();
    if (videoStats == nullptr) {
        m_AbrFeedbackInFlight->store(false);
        return;
    }

    const uint32_t deltaVideo = videoStats->packetCountVideo - m_LastAbrVideoStats.packetCountVideo;
    const uint32_t deltaFec = videoStats->packetCountFec - m_LastAbrVideoStats.packetCountFec;
    const uint32_t deltaFecFailed = videoStats->packetCountFecFailed - m_LastAbrVideoStats.packetCountFecFailed;
    const uint32_t deltaOos = videoStats->packetCountOOS - m_LastAbrVideoStats.packetCountOOS;
    const uint32_t deltaInvalid = videoStats->packetCountInvalid - m_LastAbrVideoStats.packetCountInvalid;
    const uint32_t deltaFecInvalid = videoStats->packetCountFecInvalid - m_LastAbrVideoStats.packetCountFecInvalid;
    memcpy(&m_LastAbrVideoStats, videoStats, sizeof(m_LastAbrVideoStats));

    const uint64_t packetCount = static_cast<uint64_t>(deltaVideo) + deltaFec;
    const uint64_t lossIndicators = static_cast<uint64_t>(deltaFecFailed) + deltaOos + deltaInvalid + deltaFecInvalid;
    const double packetLoss = packetCount > 0 ? qMin(100.0, (static_cast<double>(lossIndicators) * 100.0) / packetCount) : 0.0;
    const int droppedFrames = static_cast<int>(deltaFecFailed + deltaOos + deltaInvalid);

    uint32_t rtt = 0;
    uint32_t rttVariance = 0;
    LiGetEstimatedRttInfo(&rtt, &rttVariance);

    QThreadPool::globalInstance()->start(new AbrFeedbackTask(m_Computer->activeAddress,
                                                             m_Computer->activeHttpsPort,
                                                             m_Computer->serverCert,
                                                             m_Computer->uuid,
                                                             packetLoss,
                                                             rtt,
                                                             m_ActiveVideoFrameRate,
                                                             droppedFrames,
                                                             m_AbrFeedbackInFlight,
                                                             m_AbrCurrentBitrateKbps));
}

void Session::notifyMouseEmulationMode(bool enabled)
{
    m_MouseEmulationRefCount += enabled ? 1 : -1;
    SDL_assert(m_MouseEmulationRefCount >= 0);

    // We re-use the status update overlay for mouse mode notification
    if (m_MouseEmulationRefCount > 0) {
        m_OverlayManager.updateOverlayText(Overlay::OverlayStatusUpdate, "Gamepad mouse mode active\nLong press Start to deactivate");
        m_OverlayManager.setOverlayState(Overlay::OverlayStatusUpdate, true);
    }
    else {
        m_OverlayManager.setOverlayState(Overlay::OverlayStatusUpdate, false);
    }
}

class AsyncConnectionStartThread : public QThread
{
public:
    AsyncConnectionStartThread(Session* session) :
        QThread(nullptr),
        m_Session(session)
    {
        setObjectName("Async Conn Start");
    }

    void run() override
    {
        m_Session->m_AsyncConnectionSuccess = m_Session->startConnectionAsync();
    }

    Session* m_Session;
};

bool Session::tryReconnect()
{
    // Release any locally tracked pressed keys before tearing down the dead connection.
    // If the old control stream is still partly alive, this gives the host one last
    // chance to clear stuck key state before reconnecting.
    if (m_InputHandler != nullptr) {
        m_InputHandler->raiseAllKeys(false);
    }

    // The decoder can only be used between LiStartConnection() and
    // LiStopConnection(), so tear it down before stopping the dead connection.
    SDL_LockMutex(m_DecoderLock);
    delete m_VideoDecoder;
    m_VideoDecoder = nullptr;
    SDL_UnlockMutex(m_DecoderLock);

    // Stop ABR feedback (startConnectionAsync() restarts it) and the dead connection
    stopSunshineAbr();
    LiStopConnection();

    // Total time budget for reconnect attempts before giving up
    const Uint32 graceMs = 60 * 1000;
    const Uint32 startTicks = SDL_GetTicks();
    int backoffMs = 1000;

    // Suppress connection error dialogs during silent retries
    m_SuppressConnectionErrorDialog = true;

    bool reconnected = false;
    bool cancelled = false;

    // Returns true if the user wants to cancel the reconnect (close/quit/Esc)
    auto isCancelEvent = [](const SDL_Event& ev) -> bool {
        if (ev.type == SDL_QUIT) {
            return true;
        }
        if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE) {
            return true;
        }
        if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
            return true;
        }
        return false;
    };

    while (!cancelled && SDL_GetTicks() - startTicks < graceMs) {
        // Update the on-screen indicator (re-shown each attempt so it stays up)
        Uint32 remainingMs = graceMs - (SDL_GetTicks() - startTicks);
        showStreamingToast(tr("Connection interrupted. Reconnecting... (%1s)")
                               .arg((remainingMs + 999) / 1000),
                           2500);

        // Run the connection start on a worker thread and pump events while we wait
        m_AsyncConnectionSuccess = false;
        m_HasReceivedVideo = false;
        AsyncConnectionStartThread thread(this);
        thread.start();

        while (thread.isRunning()) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (isCancelEvent(ev)) {
                    cancelled = true;
                }
            }
            if (cancelled) {
                // Abort the in-progress connection attempt
                LiInterruptConnection();
            }
            QCoreApplication::processEvents();
            SDL_Delay(10);
        }
        thread.wait();

        if (cancelled) {
            break;
        }

        if (m_AsyncConnectionSuccess) {
            reconnected = true;
            if (m_ClipboardHelper != nullptr) {
                m_ClipboardHelper->updateHostContext();
            }
            if (m_InputHandler != nullptr) {
                m_InputHandler->raiseAllKeys();
            }
            break;
        }

        // Failed: clean up the partial attempt and back off before retrying
        LiStopConnection();

        Uint32 backoffUntil = SDL_GetTicks() + backoffMs;
        while (SDL_GetTicks() < backoffUntil &&
               SDL_GetTicks() - startTicks < graceMs) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (isCancelEvent(ev)) {
                    cancelled = true;
                    break;
                }
            }
            if (cancelled) {
                break;
            }
            QCoreApplication::processEvents();
            SDL_Delay(10);
        }

        // Exponential backoff, capped at 8 seconds
        backoffMs = (backoffMs * 2 > 8000) ? 8000 : backoffMs * 2;
    }

    m_SuppressConnectionErrorDialog = false;

    if (reconnected) {
        // moonlight-common-c has already called drSetup() with the new video
        // format. Recreate the decoder through the normal reset path.
        SDL_Event resetEvent = {};
        resetEvent.type = SDL_RENDER_DEVICE_RESET;
        SDL_PushEvent(&resetEvent);

        // Streaming is healthy again, so a subsequent SDL_QUIT is expected
        // to mean a real termination rather than another reconnect.
        m_UnexpectedTermination = false;

        // startConnectionAsync() may have requested a mic toggle for the
        // initial-launch path. The mic capture is client-side and was never
        // torn down, so don't toggle it off on reconnect.
        m_PendingMicToggle = false;

        showStreamingToast(tr("Reconnected"), 1500);
        return true;
    }

    if (cancelled) {
        // User asked to quit during reconnect; honor it without an error dialog
        m_ShouldExit = true;
        m_UnexpectedTermination = false;
    }

    return false;
}

namespace {

struct HostConnectionInfoSnapshot
{
    QString address;
    QString appVersion;
    QString gfeVersion;
    int currentGameId = 0;
    int serverCodecModeSupport = 0;
};

struct PreparedServerInformation
{
    QByteArray address;
    QByteArray appVersion;
    QByteArray gfeVersion;
    QByteArray rtspSessionUrl;
    SERVER_INFORMATION info;
};

HostConnectionInfoSnapshot captureHostConnectionInfoSnapshot(NvComputer* computer)
{
    HostConnectionInfoSnapshot snapshot;

    QReadLocker lock(&computer->lock);
    snapshot.address = computer->activeAddress.address();
    snapshot.appVersion = computer->appVersion;
    snapshot.gfeVersion = computer->gfeVersion;
    snapshot.currentGameId = computer->currentGameId;
    snapshot.serverCodecModeSupport = computer->serverCodecModeSupport;

    return snapshot;
}

void updateHostConnectionInfoFromServerInfo(const QString& serverInfo,
                                            NvComputer* computer,
                                            HostConnectionInfoSnapshot* snapshot,
                                            bool resumingSession)
{
    QString refreshedCodecSupport = NvHTTP::getXmlString(serverInfo, "ServerCodecModeSupport");
    QString refreshedAppVersion = NvHTTP::getXmlString(serverInfo, "appversion");
    QString refreshedGfeVersion = NvHTTP::getXmlString(serverInfo, "GfeVersion");

    if (!refreshedCodecSupport.isEmpty()) {
        int refreshedServerCodecModeSupport = refreshedCodecSupport.toInt();

        if (refreshedServerCodecModeSupport != snapshot->serverCodecModeSupport) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Host codec mode support changed after %s: %d -> %d",
                        resumingSession ? "resume" : "launch",
                        snapshot->serverCodecModeSupport,
                        refreshedServerCodecModeSupport);
        }

        snapshot->serverCodecModeSupport = refreshedServerCodecModeSupport;
    }

    if (!refreshedAppVersion.isEmpty()) {
        snapshot->appVersion = refreshedAppVersion;
    }

    if (!refreshedGfeVersion.isEmpty()) {
        snapshot->gfeVersion = refreshedGfeVersion;
    }

    snapshot->currentGameId = NvHTTP::getCurrentGame(serverInfo);

    QWriteLocker lock(&computer->lock);
    computer->serverCodecModeSupport = snapshot->serverCodecModeSupport;
    computer->currentGameId = snapshot->currentGameId;

    if (!snapshot->appVersion.isEmpty()) {
        computer->appVersion = snapshot->appVersion;
    }

    if (!snapshot->gfeVersion.isEmpty()) {
        computer->gfeVersion = snapshot->gfeVersion;
    }
}

void prepareServerInformation(const HostConnectionInfoSnapshot& snapshot,
                              const QString& rtspSessionUrl,
                              PreparedServerInformation* prepared)
{
    LiInitializeServerInformation(&prepared->info);

    prepared->address = snapshot.address.toUtf8();
    prepared->appVersion = snapshot.appVersion.toUtf8();
    prepared->info.address = prepared->address.constData();
    prepared->info.serverInfoAppVersion = prepared->appVersion.constData();
    prepared->info.serverCodecModeSupport = snapshot.serverCodecModeSupport;

    if (!snapshot.gfeVersion.isEmpty()) {
        prepared->gfeVersion = snapshot.gfeVersion.toUtf8();
        prepared->info.serverInfoGfeVersion = prepared->gfeVersion.constData();
    }

    if (!rtspSessionUrl.isEmpty()) {
        prepared->rtspSessionUrl = rtspSessionUrl.toUtf8();
        prepared->info.rtspSessionUrl = prepared->rtspSessionUrl.constData();
    }
}

} // namespace

// Called in a non-main thread
bool Session::startConnectionAsync()
{
    // The UI should have ensured the old game was already quit
    // if we decide to stream a different game.
    Q_ASSERT(m_Computer->currentGameId == 0 ||
             m_Computer->currentGameId == m_App.id);

    bool enableGameOptimizations;
    if (m_Computer->isNvidiaServerSoftware) {
        // GFE will set all settings to 720p60 if it doesn't recognize
        // the chosen resolution. Avoid that by disabling SOPS when it
        // is not streaming a supported resolution.
        enableGameOptimizations = false;
        for (const NvDisplayMode &mode : std::as_const(m_Computer->displayModes)) {
            if (mode.width == m_StreamConfig.width &&
                    mode.height == m_StreamConfig.height) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Found host supported resolution: %dx%d",
                            mode.width, mode.height);
                enableGameOptimizations = m_Preferences->gameOptimizations;
                break;
            }
        }
    }
    else {
        // Always send SOPS to Sunshine because we may repurpose the
        // option to control whether the display mode is adjusted
        enableGameOptimizations = m_Preferences->gameOptimizations;
    }

    HostConnectionInfoSnapshot hostConnectionInfo = captureHostConnectionInfoSnapshot(m_Computer);
    const bool resumingSession = hostConnectionInfo.currentGameId != 0;

    QString rtspSessionUrl;

    // Resolve the HDR brightness profile for Foundation Sunshine.
    float maxBrightness = 0, minBrightness = 0, maxAverageBrightness = 0;
    switch (m_Preferences->hdrBrightnessMode) {
    case StreamingPreferences::HBM_MANUAL:
        maxBrightness = static_cast<float>(m_Preferences->hdrMaxBrightness);
        minBrightness = static_cast<float>(m_Preferences->hdrMinBrightness);
        maxAverageBrightness = static_cast<float>(m_Preferences->hdrMaxAverageBrightness);

        if (maxBrightness <= 0 || minBrightness < 0 ||
                maxAverageBrightness <= 0 || minBrightness > maxAverageBrightness ||
                maxAverageBrightness > maxBrightness) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Ignoring invalid manual HDR brightness profile: max=%.3f, min=%.6f, maxAverage=%.3f",
                        maxBrightness, minBrightness, maxAverageBrightness);
            maxBrightness = minBrightness = maxAverageBrightness = 0;
        }
        break;
    case StreamingPreferences::HBM_AUTO:
#ifdef Q_OS_WIN32
        queryDisplayHdrBrightness(maxBrightness, minBrightness, maxAverageBrightness);
#endif
        break;
    case StreamingPreferences::HBM_HOST_DEFAULT:
    default:
        break;
    }

    try {
        NvHTTP http(m_Computer);
        RemoteStreamConfig remoteStreamConfig(
            m_Preferences->remoteResolution,
            m_Preferences->remoteResolutionWidth,
            m_Preferences->remoteResolutionHeight,
            m_Preferences->remoteFps,
            m_Preferences->remoteFpsRate,
            m_Preferences->width,
            m_Preferences->height,
            maxBrightness,
            minBrightness,
            maxAverageBrightness
        );
        http.startApp(resumingSession ? "resume" : "launch",
                      m_Computer->isNvidiaServerSoftware,
                      m_App.id, &m_StreamConfig,
                      enableGameOptimizations,
                      m_Preferences->playAudioOnHost,
                      m_InputHandler->getAttachedGamepadMask(),
                      !m_Preferences->multiController,
                      rtspSessionUrl,
                      m_Preferences->customScreenMode,
                      m_Preferences->customVddScreenMode,
                      remoteStreamConfig);

        try {
            updateHostConnectionInfoFromServerInfo(http.getServerInfo(NvHTTP::NVLL_NONE, true),
                                                   m_Computer,
                                                   &hostConnectionInfo,
                                                   resumingSession);
        } catch (const GfeHttpResponseException& e) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to refresh server info after %s: %s",
                        resumingSession ? "resume" : "launch",
                        qPrintable(e.toQString()));
        } catch (const QtNetworkReplyException& e) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to refresh server info after %s: %s",
                        resumingSession ? "resume" : "launch",
                        qPrintable(e.toQString()));
        }
    } catch (const GfeHttpResponseException& e) {
        if (!m_SuppressConnectionErrorDialog) {
            emit displayLaunchError(tr("Host returned error: %1").arg(e.toQString()));
        }
        return false;
    } catch (const QtNetworkReplyException& e) {
        if (!m_SuppressConnectionErrorDialog) {
            emit displayLaunchError(e.toQString());
        }
        return false;
    }

    PreparedServerInformation preparedHostInfo;
    prepareServerInformation(hostConnectionInfo, rtspSessionUrl, &preparedHostInfo);

    if (m_Preferences->packetSize != 0) {
        // Override default packet size and remote streaming detection
        // NB: Using STREAM_CFG_AUTO will cap our packet size at 1024 for remote hosts.
        m_StreamConfig.streamingRemotely = STREAM_CFG_LOCAL;
        m_StreamConfig.packetSize = m_Preferences->packetSize;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using custom packet size: %d bytes",
                    m_Preferences->packetSize);
    }
    else {
        // Use 1392 byte video packets by default
        m_StreamConfig.packetSize = 1392;

        // getActiveAddressReachability() does network I/O, so we only attempt to check
        // reachability if we've already contacted the PC successfully.
        switch (m_Computer->getActiveAddressReachability()) {
        case NvComputer::RI_LAN:
            // This address is on-link, so treat it as a local address
            // even if it's not in RFC 1918 space or it's an IPv6 address.
            m_StreamConfig.streamingRemotely = STREAM_CFG_LOCAL;
            break;
        case NvComputer::RI_VPN:
            // It looks like our route to this PC is over a VPN, so cap at 1024 bytes.
            // Treat it as remote even if the target address is in RFC 1918 address space.
            m_StreamConfig.streamingRemotely = STREAM_CFG_REMOTE;
            m_StreamConfig.packetSize = 1024;
            break;
        default:
            // If we don't have reachability info, let moonlight-common-c decide.
            m_StreamConfig.streamingRemotely = STREAM_CFG_AUTO;
            break;
        }
    }

    // If the user has chosen YUV444 without adjusting the bitrate but the host doesn't
    // support YUV444 streaming, use the default non-444 bitrate for the stream instead.
    // This should provide equivalent image quality for YUV420 as the stream would have
    // had if the host supported YUV444 (though obviously with 4:2:0 subsampling).
    // If the user has adjusted the bitrate from default, we'll assume they really wanted
    // that value and not second guess them.
    if (m_Preferences->enableYUV444 &&
        !(m_StreamConfig.supportedVideoFormats & VIDEO_FORMAT_MASK_YUV444) &&
        m_StreamConfig.bitrate == StreamingPreferences::getDefaultBitrate(m_StreamConfig.width,
                                                                          m_StreamConfig.height,
                                                                          m_StreamConfig.fps,
                                                                          true)) {
        m_StreamConfig.bitrate = StreamingPreferences::getDefaultBitrate(m_StreamConfig.width,
                                                                         m_StreamConfig.height,
                                                                         m_StreamConfig.fps,
                                                                         false);
    }

    int err = LiStartConnection(&preparedHostInfo.info, &m_StreamConfig, &k_ConnCallbacks,
                                &m_VideoCallbacks, &m_AudioCallbacks,
                                NULL, 0, NULL, 0);
    if (err != 0) {
        // We already displayed an error dialog in the stage failure
        // listener.
        return false;
    }

    if (m_InputHandler != nullptr) {
        const int cursorResult =
            LiSetCursorMode(m_InputHandler->getLocalCursorMode());
        if (cursorResult != LI_CURSOR_MODE_OK &&
            cursorResult != LI_CURSOR_MODE_ERR_UNSUPPORTED) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to set initial local cursor mode: %d",
                        cursorResult);
        }
    }

    emit connectionStarted();
    startSunshineAbr();
    startFileMappingUxProbe();
    startFileMappingSmokeProbe();
    if (m_Preferences->enableMicrophone) {
        // Use the deferred mic toggle mechanism instead of QueuedConnection to avoid
        // heap corruption when creating QAudioSource within nested event loops (processEvents).
        m_PendingMicToggle = true;
    }
    return true;
}

void Session::flushWindowEvents()
{
    // Pump events to ensure all pending OS events are posted
    SDL_PumpEvents();

    // Insert a barrier to discard any additional window events.
    // We don't use SDL_FlushEvent() here because it could cause
    // important events to be lost.
    m_FlushingWindowEventsRef++;

    // This event will cause us to set m_FlushingWindowEvents back to false.
    SDL_Event flushEvent = {};
    flushEvent.type = SDL_USEREVENT;
    flushEvent.user.code = SDL_CODE_FLUSH_WINDOW_EVENT_BARRIER;
    SDL_PushEvent(&flushEvent);
}

void Session::setShouldExit(bool quitHostApp)
{
    // If the caller has explicitly asked us to quit the host app,
    // override whatever the preferences say and do it. If the
    // caller doesn't override to force quit, let the preferences
    // dictate what we do.
    if (quitHostApp) {
        m_Preferences->quitAppAfter = true;
    }

    m_ShouldExit = true;
}

void Session::start()
{
    // Wait for any old session to finish cleanup
    s_ActiveSessionSemaphore.acquire();

    // We're now active
    s_ActiveSession = this;

#ifndef STEAM_LINK
    // Construct the clipboard sync helper process before the control receive
    // thread can possibly invoke clClipboardData(). Host frames are queued by
    // ClipboardHelperClient and flushed from the SDL loop.
    if (m_ClipboardHelper == nullptr) {
        m_ClipboardHelper = new ClipboardHelperClient(m_Computer, this);
        m_ClipboardHelper->start();
    }
#endif

    // Initialize the gamepad code with our preferences
    // NB: m_InputHandler must be initialize before starting the connection.
    m_InputHandler = new SdlInputHandler(*m_Preferences, m_StreamConfig.width, m_StreamConfig.height);

    // Kick off the async connection thread then return to the caller to pump the event loop
    auto thread = new AsyncConnectionStartThread(this);
    QObject::connect(thread, &QThread::finished, this, &Session::exec);
    QObject::connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void Session::interrupt()
{
    // Stop any connection in progress
    LiInterruptConnection();

    // Inject a quit event to our SDL event loop
    SDL_Event event;
    event.type = SDL_QUIT;
    event.quit.timestamp = SDL_GetTicks();
    SDL_PushEvent(&event);
}

#ifdef Q_OS_WIN32
void Session::queryDisplayHdrBrightness(float& maxNits, float& minNits, float& maxFullNits)
{
    using Microsoft::WRL::ComPtr;

    maxNits = 0;
    minNits = 0;
    maxFullNits = 0;

    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory))) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to create DXGI factory for brightness query");
        return;
    }

    // Enumerate adapters and outputs to find HDR-capable display
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIdx = 0; SUCCEEDED(factory->EnumAdapters1(adapterIdx, &adapter)); adapterIdx++) {
        ComPtr<IDXGIOutput> output;
        for (UINT outputIdx = 0; SUCCEEDED(adapter->EnumOutputs(outputIdx, &output)); outputIdx++) {
            ComPtr<IDXGIOutput6> output6;
            if (SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput6), (void**)&output6))) {
                DXGI_OUTPUT_DESC1 desc1;
                if (SUCCEEDED(output6->GetDesc1(&desc1))) {
                    // Use the first display with HDR support, or the first display if none support HDR
                    if (maxNits == 0 || desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
                        maxNits = desc1.MaxLuminance;
                        minNits = desc1.MinLuminance;
                        maxFullNits = desc1.MaxFullFrameLuminance;

                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                    "Display HDR brightness: max=%.1f nits, min=%.4f nits, maxFull=%.1f nits (HDR: %s)",
                                    maxNits, minNits, maxFullNits,
                                    desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 ? "yes" : "no");

                        if (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
                            return; // Found an HDR display, use it
                        }
                    }
                }
            }
        }
    }

    if (maxNits > 0) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Using SDR display brightness: max=%.1f, min=%.4f, maxFull=%.1f",
                    maxNits, minNits, maxFullNits);
    } else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "No display brightness information available");
    }
}
#endif

// 加载页退场淡幕的时长上限，比 StreamSegue.qml 里那条 340ms 动画留一点余量。
// 两边要一起改。
static const int k_StreamEnterVeilMs = 380;

// 等全屏切换完成的兜底超时。macOS 的全屏动画约 0.5~0.7s，取个宽松上限；
// 万一平台不发 SIZE_CHANGED，也不能让界面窗口一直留着。
static const Uint32 k_FullScreenEntryTimeoutMs = 1200;

void Session::exec()
{
    // If the connection failed, clean up and abort the connection.
    if (!m_AsyncConnectionSuccess) {
        if (m_ClipboardHelper != nullptr) {
            m_ClipboardHelper->stop();
            delete m_ClipboardHelper;
            m_ClipboardHelper = nullptr;
        }
        delete m_InputHandler;
        m_InputHandler = nullptr;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        QThreadPool::globalInstance()->start(new DeferredSessionCleanupTask(this));
        return;
    }

    if (m_ClipboardHelper != nullptr) {
        m_ClipboardHelper->updateHostContext();
    }

    // Pump the Qt event loop one last time before we create our SDL window
    // This is sometimes necessary for the QML code to process any signals
    // we've emitted from the async connection thread.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QCoreApplication::sendPostedEvents();

    int x, y, width, height;
    getWindowDimensions(x, y, width, height);

    // 全屏会话：让串流窗口从 Qt 界面窗口所在的位置和尺寸起步。
    //
    // getWindowDimensions() 给的是「屏幕居中 + 串流分辨率」，那是退出全屏之后
    // 该有的窗口大小，但拿它当全屏动画的起点就不对了 —— 系统会从一个和刚才那个
    // 界面窗口毫无关系的矩形开始放大，看起来像凭空弹出一个空窗口再被撑大。
    // 用界面窗口自己的 frame 起步，动画读起来就是「刚才那个窗口变成了全屏」。
    //
    // 这里只改初始创建；运行中 Ctrl+Alt+Shift+F 退出全屏时走的是 toggleFullscreen()
    // 里的那次 getWindowDimensions()，恢复的仍然是串流分辨率大小。
    if (m_IsFullScreen && m_QtWindow != nullptr) {
        QRect guiFrame = m_QtWindow->geometry();
        if (guiFrame.width() > 0 && guiFrame.height() > 0) {
            qreal scale = 1.0;
#ifndef Q_OS_DARWIN
            // macOS 上 SDL 和 Qt 都用点（逻辑坐标）；其他平台 SDL 用物理像素，要折算
            scale = m_QtWindow->devicePixelRatio();
#endif
            x = qRound(guiFrame.x() * scale);
            y = qRound(guiFrame.y() * scale);
            width = qRound(guiFrame.width() * scale);
            height = qRound(guiFrame.height() * scale);
        }
    }

    // 让加载页的退场动画真正跑完，再去创建串流窗口。
    //
    // connectionStarted 和 exec() 是同一批投递到主线程的信号，所以那个 340ms 的
    // 淡幕动画实际上拿不到任何主循环时间 —— 幕从来没黑下来过，SDL 窗口就直接盖在
    // 加载页上，观感是硬切。这里主动把主循环喂满一段时间让它跑完。
    //
    // 必须在这儿做完：exec() 往下就进 SDL 事件循环了，那之后 Qt 的定时器和队列信号
    // 只在串流覆盖层可见时才会被 pump，QML 侧再也等不到机会。
    if (m_QtWindow != nullptr) {
        QElapsedTimer veilTimer;
        veilTimer.start();
        while (veilTimer.elapsed() < k_StreamEnterVeilMs) {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            SDL_Delay(4);
        }
    }

#ifdef STEAM_LINK
    // We need a little delay before creating the window or we will trigger some kind
    // of graphics driver bug on Steam Link that causes a jagged overlay to appear in
    // the top right corner randomly.
    SDL_Delay(500);
#endif

    // Request at least 8 bits per color for GL
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

    // Disable depth and stencil buffers
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    // We always want a resizable window with High DPI enabled
    Uint32 defaultWindowFlags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;

    // If we're starting in windowed mode and the Moonlight GUI is maximized or
    // minimized, match that with the streaming window.
    if (!m_IsFullScreen && m_QtWindow != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        // Qt 5.10+ can propagate multiple states together
        if (m_QtWindow->windowStates() & Qt::WindowMaximized) {
            defaultWindowFlags |= SDL_WINDOW_MAXIMIZED;
        }
        if (m_QtWindow->windowStates() & Qt::WindowMinimized) {
            defaultWindowFlags |= SDL_WINDOW_MINIMIZED;
        }
#else
        // Qt 5.9 only supports a single state at a time
        if (m_QtWindow->windowState() == Qt::WindowMaximized) {
            defaultWindowFlags |= SDL_WINDOW_MAXIMIZED;
        }
        else if (m_QtWindow->windowState() == Qt::WindowMinimized) {
            defaultWindowFlags |= SDL_WINDOW_MINIMIZED;
        }
#endif
    }

    // We use only the computer name on macOS to match Apple conventions where the
    // app name is featured in the menu bar and the document name is in the title bar.
#ifdef Q_OS_DARWIN
    std::string windowName = QString(m_Computer->name).toStdString();
#else
    std::string windowName = QString(m_Computer->name + " - Moonlight").toStdString();
#endif

    m_Window = SDL_CreateWindow(windowName.c_str(),
                                x,
                                y,
                                width,
                                height,
                                defaultWindowFlags | StreamUtils::getPlatformWindowFlags());
    if (!m_Window) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL_CreateWindow() failed with platform flags: %s",
                    SDL_GetError());

        m_Window = SDL_CreateWindow(windowName.c_str(),
                                    x,
                                    y,
                                    width,
                                    height,
                                    defaultWindowFlags);
        if (!m_Window) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_CreateWindow() failed: %s",
                         SDL_GetError());

            if (m_ClipboardHelper != nullptr) {
                m_ClipboardHelper->stop();
                delete m_ClipboardHelper;
                m_ClipboardHelper = nullptr;
            }
            delete m_InputHandler;
            m_InputHandler = nullptr;
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            QThreadPool::globalInstance()->start(new DeferredSessionCleanupTask(this));
            return;
        }
    }

    m_InputHandler->setWindow(m_Window);

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
    // Other platforms seem to preserve our Qt icon when creating a new window.
    if (iconSurface != nullptr) {
        // This must be called before entering full-screen mode on Windows
        // or our icon will not persist when toggling to windowed mode
        SDL_SetWindowIcon(m_Window, iconSurface);
    }
#endif

    // Update the window display mode based on our current monitor
    // for if/when we enter full-screen mode.
    updateOptimalWindowDisplayMode();

    // Enter full screen if requested
    bool awaitingFullScreenEntry = false;
    if (m_IsFullScreen) {
        if (SDL_SetWindowFullscreen(m_Window, m_FullScreenFlag) == 0) {
            awaitingFullScreenEntry = true;
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "SDL_SetWindowFullscreen() failed: %s",
                        SDL_GetError());
        }
    }

    // 串流窗口就位之后才隐藏界面窗口。
    //
    // 以前这件事在 QML 的退场动画末尾做（hideForStreaming），时机太早：macOS 上
    // 全屏窗口会切进一个新的 Space，界面窗口一旦提前藏掉，旧 Space 在整个切换动画
    // 期间露出来的就是桌面。留着它（上面盖着那层已经全黑的幕）观感是连续的。
    //
    // 但也不能在这儿就藏：SDL_SetWindowFullscreen() 在 macOS 上是异步的，它在 Cocoa
    // 的 toggleFullScreen: 动画一开始就返回，那会儿旧 Space 还在往外滑。所以这里只
    // 登记一个截止时间，真正的隐藏交给事件循环 —— 收到 SIZE_CHANGED（Cocoa 在全屏
    // 切换结束后才发）就藏，收不到就等超时兜底。
    //
    // 由 C++ 来做是因为 QML 等不到通知：往下就进 SDL 事件循环了，那之后 Qt 的队列
    // 信号和定时器只在串流覆盖层可见时才会被 pump。
    Uint32 qtWindowHideDeadline = 0;
    if (m_QtWindow != nullptr) {
        if (awaitingFullScreenEntry) {
            qtWindowHideDeadline = SDL_GetTicks() + k_FullScreenEntryTimeoutMs;
        }
        else {
            // 没有全屏切换要等：窗口化会话，或者上面那次请求失败了。
            // 失败也照样藏 —— 界面窗口在串流期间没有任何作用，留着它只会把一张
            // 停在加载页的窗口压在串流窗口后面。
            m_QtWindow->setVisible(false);
        }
    }

    bool needsFirstEnterCapture = false;
    bool needsPostDecoderCreationCapture = false;

    // Avoid capturing the mouse initially for windowed relative mode.
    // We still capture in windowed absolute mode because it doesn't
    // constrain the motion of the cursor. This allows the user to
    // easily reposition or resize the window.
    if (m_IsFullScreen || m_Preferences->absoluteMouseMode) {
        // HACK: For Wayland, we wait until we get the first SDL_WINDOWEVENT_ENTER
        // event where it seems to work consistently on GNOME. For other platforms,
        // especially where SDL may call SDL_RecreateWindow(), we must only capture
        // after the decoder is created.
        if (strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
            // Native Wayland: Capture on SDL_WINDOWEVENT_ENTER
            needsFirstEnterCapture = true;
        }
        else {
            // X11/XWayland: Capture after decoder creation
            needsPostDecoderCreationCapture = true;
        }
    }

    // Disable the screen saver if requested
    if (m_Preferences->keepAwake) {
        SDL_DisableScreenSaver();
    }

    // Hide Qt's fake mouse cursor on EGLFS systems
    if (QGuiApplication::platformName() == "eglfs") {
        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
    }

    // Set timer resolution to 1 ms on Windows for greater
    // sleep precision and more accurate callback timing.
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    int currentDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);

    // Now that we're about to stream, any SDL_QUIT event is expected
    // unless it comes from the connection termination callback where
    // (m_UnexpectedTermination is set back to true).
    m_UnexpectedTermination = false;

    // Start rich presence to indicate we're in game
    RichPresenceManager presence(*m_Preferences, m_App.name);

    // Toggle the stats overlay if requested by the user
    m_OverlayManager.setOverlayState(Overlay::OverlayDebug, m_Preferences->showPerformanceOverlay);

    // Initialize mouse state for menu
    m_WasCapturedBeforeMenu = false;

    // Create Qt-based overlay menu panel (rendered by OS compositor, not D3D11)
    m_MenuPanel = new OverlayMenuPanel();
    m_MenuButton = nullptr;
    m_Toast = new OverlayToast();
    m_MenuPanel->setActionCallback([this](OverlayMenuPanel::MenuAction action) {
        dispatchQtMenuAction(action);
    });
    m_MenuPanel->setCloseCallback([this]() {
        // Record close timestamp for edge-trigger debounce
        m_MenuCloseTicks = SDL_GetTicks();

        // Restore mouse capture after menu closes
        // Note: for actions that change window state (fullscreen, minimize),
        // we defer capture restoration to after the action completes.
        // See dispatchQtMenuAction() for those cases.
        if (m_WasCapturedBeforeMenu && !m_DeferCaptureRestore) {
            if (isStreamingWindowVisible()) {
                m_InputHandler->setCaptureActive(true);
            }
            m_WasCapturedBeforeMenu = false;
        }

        syncQtOverlayWindowsWithSdlWindowState();

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Qt overlay menu closed, capture %s",
                    m_DeferCaptureRestore ? "deferred" : "restored");
    });

    // Create floating menu button if configured
    if (m_Preferences->overlayMenuPosition == StreamingPreferences::OMP_BUTTON) {
        m_MenuButton = new OverlayMenuButton();
        m_MenuButton->setClickCallback([this]() {
            showQtOverlayMenu();
        });
        // Show button at initial position
        int wx, wy, ww, wh;
        SDL_GetWindowPosition(m_Window, &wx, &wy);
        SDL_GetWindowSize(m_Window, &ww, &wh);
        m_MenuButton->showButton(wx, wy, ww, wh);
    }

    // Switch to async logging mode when we enter the SDL loop
    StreamUtils::enterAsyncLoggingMode();

    // Hijack this thread to be the SDL main thread. Pump Qt only for visible
    // streaming UI; clipboard sync runs in a helper process with its own Qt
    // event loop and is serviced via pipe polling below.
    constexpr Uint32 QT_UI_EVENT_PUMP_INTERVAL_MS = 10;
    Uint32 lastQtEventPumpTicks = 0;
    auto processQtEventsDuringStream = [this, &lastQtEventPumpTicks](bool force = false) {
        const bool qtUiVisible = (m_MenuPanel && m_MenuPanel->needsEventProcessing()) ||
                                 (m_Toast && m_Toast->isVisible());
        if (!qtUiVisible) {
            return;
        }

        const Uint32 now = SDL_GetTicks();
        if (!force && now - lastQtEventPumpTicks < QT_UI_EVENT_PUMP_INTERVAL_MS) {
            return;
        }
        lastQtEventPumpTicks = now;
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    };

    auto processClipboardHelperMessages = [this]() {
        if (m_ClipboardHelper != nullptr) {
            m_ClipboardHelper->processPendingMessages();
        }
    };

    constexpr Uint32 ABR_FEEDBACK_INTERVAL_MS = 3000;
    auto processSunshineAbrFeedback = [this]() {
        if (!m_SunshineAbrEnabled) {
            return;
        }

        const Uint32 now = SDL_GetTicks();
        if (now - m_LastAbrFeedbackTicks >= ABR_FEEDBACK_INTERVAL_MS) {
            m_LastAbrFeedbackTicks = now;
            sendSunshineAbrFeedback();
        }
    };

    // 见上面 qtWindowHideDeadline 处的注释：串流窗口真的落定了（或者等超时了）
    // 再把界面窗口藏掉。
    auto hideGuiWindowWhenSettled = [&](bool settled) {
        if (qtWindowHideDeadline == 0 || m_QtWindow == nullptr) {
            return;
        }
        if (settled || SDL_TICKS_PASSED(SDL_GetTicks(), qtWindowHideDeadline)) {
            m_QtWindow->setVisible(false);
            qtWindowHideDeadline = 0;
        }
    };

    SDL_Event event;
    for (;;) {
        hideGuiWindowWhenSettled(false);
        processSunshineAbrFeedback();
        processFileMappingUxProbeResult();
        processFileMappingMountResult();
        processClipboardHelperMessages();

#if SDL_VERSION_ATLEAST(2, 0, 18) && !defined(STEAM_LINK)
        // SDL 2.0.18 has a proper wait event implementation that uses platform
        // support to block on events rather than polling on Windows, macOS, X11,
        // and Wayland. It will fall back to 1 ms polling if a joystick is
        // connected, so we don't use it for STEAM_LINK to ensure we only poll
        // every 10 ms.
        //
        // NB: This behavior was introduced in SDL 2.0.16, but had a few critical
        // issues that could cause indefinite timeouts, delayed joystick detection,
        // and other problems.
        int waitTimeoutMs = (m_ClipboardHelper != nullptr && m_ClipboardHelper->isRunning()) ? 100 : 1000;
        if (!SDL_WaitEventTimeout(&event, waitTimeoutMs)) {
            presence.runCallbacks();
            processClipboardHelperMessages();
            processFileMappingUxProbeResult();
            processFileMappingMountResult();
            processQtEventsDuringStream(true);
            processSunshineAbrFeedback();
            continue;
        }
#else
        // We explicitly use SDL_PollEvent() and SDL_Delay() because
        // SDL_WaitEvent() has an internal SDL_Delay(10) inside which
        // blocks this thread too long for high polling rate mice and high
        // refresh rate displays.
        if (!SDL_PollEvent(&event)) {
#ifndef STEAM_LINK
            SDL_Delay(1);
#else
            // Waking every 1 ms to process input is too much for the low performance
            // ARM core in the Steam Link, so we will wait 10 ms instead.
            SDL_Delay(10);
#endif
            presence.runCallbacks();
            processClipboardHelperMessages();
            processFileMappingUxProbeResult();
            processFileMappingMountResult();
            processQtEventsDuringStream();
            processSunshineAbrFeedback();
            continue;
        }
#endif
        switch (event.type) {
        case SDL_QUIT:
            // If the connection was interrupted by a transient network problem
            // (rather than a user-initiated quit), try to silently reconnect
            // before tearing the session down.
            if (m_ConnectionInterrupted && !m_ShouldExit) {
                m_ConnectionInterrupted = false;
                if (tryReconnect()) {
                    // Streaming resumed; keep running the event loop
                    continue;
                }

                // Reconnect gave up or was cancelled. Show the original
                // termination error (unless the user asked to quit) and
                // fall through to cleanup.
                if (!m_ShouldExit) {
                    displayTerminationError(m_LastTerminationErrorCode);
                }
            }
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Quit event received");
            goto DispatchDeferredCleanup;

        case SDL_USEREVENT:
            switch (event.user.code) {
            case SDL_CODE_FRAME_READY:
                if (m_VideoDecoder != nullptr) {
                    m_VideoDecoder->renderFrameOnMainThread();
                }
                break;
            case SDL_CODE_FLUSH_WINDOW_EVENT_BARRIER:
                m_FlushingWindowEventsRef--;
                break;
            case SDL_CODE_GAMECONTROLLER_RUMBLE:
                m_InputHandler->rumble((uint16_t)(uintptr_t)event.user.data1,
                                       (uint16_t)((uintptr_t)event.user.data2 >> 16),
                                       (uint16_t)((uintptr_t)event.user.data2 & 0xFFFF));
                break;
            case SDL_CODE_GAMECONTROLLER_RUMBLE_TRIGGERS:
                m_InputHandler->rumbleTriggers((uint16_t)(uintptr_t)event.user.data1,
                                               (uint16_t)((uintptr_t)event.user.data2 >> 16),
                                               (uint16_t)((uintptr_t)event.user.data2 & 0xFFFF));
                break;
            case SDL_CODE_GAMECONTROLLER_SET_MOTION_EVENT_STATE:
                m_InputHandler->setMotionEventState((uint16_t)(uintptr_t)event.user.data1,
                                                    (uint8_t)((uintptr_t)event.user.data2 >> 16),
                                                    (uint16_t)((uintptr_t)event.user.data2 & 0xFFFF));
                break;
            case SDL_CODE_GAMECONTROLLER_SET_CONTROLLER_LED:
                m_InputHandler->setControllerLED((uint16_t)(uintptr_t)event.user.data1,
                                                 (uint8_t)((uintptr_t)event.user.data2 >> 16),
                                                 (uint8_t)((uintptr_t)event.user.data2 >> 8),
                                                 (uint8_t)((uintptr_t)event.user.data2));
                break;
            case SDL_CODE_GAMECONTROLLER_SET_ADAPTIVE_TRIGGERS:
                m_InputHandler->setAdaptiveTriggers((uint16_t)(uintptr_t)event.user.data1,
                                                    (DualSenseOutputReport *)event.user.data2);
                break;
            case SDL_CODE_FLUSH_TOUCHPAD_FRAME:
                m_InputHandler->flushPendingTouchpadFrameEvent();
                break;
            case SDL_CODE_CURSOR_UPDATE:
            {
                std::shared_ptr<RemoteCursorUpdate> cursorUpdate;
                {
                    std::lock_guard<std::mutex> lock(m_CursorUpdateMutex);
                    cursorUpdate.swap(m_PendingCursorUpdate);
                    m_CursorUpdateEventQueued = false;
                }
                if (cursorUpdate != nullptr && m_InputHandler != nullptr) {
                    m_InputHandler->updateRemoteCursor(*cursorUpdate);
                }
                break;
            }
            default:
                SDL_assert(false);
            }
            break;

        case SDL_WINDOWEVENT:
            // Early handling of some events
            switch (event.window.event) {
            case SDL_WINDOWEVENT_FOCUS_LOST:
                if (m_Preferences->muteOnFocusLoss) {
                    m_AudioMuted = true;
                }
                m_InputHandler->notifyFocusLost();
                // Close overlay menu when main window loses focus
                if (m_MenuPanel && m_MenuPanel->isMenuVisible()) {
                    m_MenuPanel->closeMenu();
                }
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                if (m_Preferences->muteOnFocusLoss) {
                    m_AudioMuted = false;
                }
                m_InputHandler->notifyFocusGained();
                break;
            case SDL_WINDOWEVENT_LEAVE:
                m_InputHandler->notifyMouseLeave();
                break;
            case SDL_WINDOWEVENT_HIDDEN:
            case SDL_WINDOWEVENT_MINIMIZED:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_SHOWN:
            case SDL_WINDOWEVENT_MOVED:
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                // Cocoa 在全屏切换结束之后才发这个事件，拿它当「窗口已落定」的信号
                hideGuiWindowWhenSettled(true);
                syncQtOverlayWindowsWithSdlWindowState();
                // 远端光标的尺寸是按窗口的 backing 比例算的（见 getRemoteCursorScale()），
                // 换屏、改分辨率、改缩放都会让它失效。挂在这一组事件上而不是只挂
                // DISPLAY_CHANGED：同一块屏上改系统缩放只会发 SIZE_CHANGED。
                // 比例没变时这个调用会直接返回，挂宽一点不亏。
                m_InputHandler->refreshRemoteCursorScale();
                break;
            }

            presence.runCallbacks();

            // Capture the mouse on SDL_WINDOWEVENT_ENTER if needed
            if (needsFirstEnterCapture && event.window.event == SDL_WINDOWEVENT_ENTER) {
                m_InputHandler->setCaptureActive(true);
                needsFirstEnterCapture = false;
            }

            // We want to recreate the decoder for resizes (full-screen toggles) and the initial shown event.
            // We use SDL_WINDOWEVENT_SIZE_CHANGED rather than SDL_WINDOWEVENT_RESIZED because the latter doesn't
            // seem to fire when switching from windowed to full-screen on X11.
            if (event.window.event != SDL_WINDOWEVENT_SIZE_CHANGED &&
                (event.window.event != SDL_WINDOWEVENT_SHOWN || m_VideoDecoder != nullptr)) {
                // Check that the window display hasn't changed. If it has, we want
                // to recreate the decoder to allow it to adapt to the new display.
                // This will allow Pacer to pull the new display refresh rate.
#if SDL_VERSION_ATLEAST(2, 0, 18)
                // On SDL 2.0.18+, there's an event for this specific situation
                if (event.window.event != SDL_WINDOWEVENT_DISPLAY_CHANGED) {
                    break;
                }
#else
                // Prior to SDL 2.0.18, we must check the display index for each window event
                if (SDL_GetWindowDisplayIndex(m_Window) == currentDisplayIndex) {
                    break;
                }
#endif
            }
#ifdef Q_OS_WIN32
            // We can get a resize event after being minimized. Recreating the renderer at that time can cause
            // us to start drawing on the screen even while our window is minimized. Minimizing on Windows also
            // moves the window to -32000, -32000 which can cause a false window display index change. Avoid
            // that whole mess by never recreating the decoder if we're minimized.
            else if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED) {
                break;
            }
#endif

            if (m_FlushingWindowEventsRef > 0) {
                // Ignore window events for renderer reset if flushing
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Dropping window event during flush: %d (%d %d)",
                            event.window.event,
                            event.window.data1,
                            event.window.data2);
                break;
            }

            // Allow the renderer to handle the state change without being recreated
            if (m_VideoDecoder) {
                bool forceRecreation = false;

                WINDOW_STATE_CHANGE_INFO windowChangeInfo = {};
                windowChangeInfo.window = m_Window;

                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    windowChangeInfo.stateChangeFlags |= WINDOW_STATE_CHANGE_SIZE;

                    windowChangeInfo.width = event.window.data1;
                    windowChangeInfo.height = event.window.data2;
                }

                int newDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);
                if (newDisplayIndex != currentDisplayIndex) {
                    windowChangeInfo.stateChangeFlags |= WINDOW_STATE_CHANGE_DISPLAY;

                    windowChangeInfo.displayIndex = newDisplayIndex;

                    // If the refresh rates have changed, we will need to go through the full
                    // decoder recreation path to ensure Pacer is switched to the new display
                    // and that we apply any V-Sync disablement rules that may be needed for
                    // this display.
                    SDL_DisplayMode oldMode, newMode;
                    if (SDL_GetCurrentDisplayMode(currentDisplayIndex, &oldMode) < 0 ||
                            SDL_GetCurrentDisplayMode(newDisplayIndex, &newMode) < 0 ||
                            oldMode.refresh_rate != newMode.refresh_rate) {
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                    "Forcing renderer recreation due to refresh rate change between displays");
                        forceRecreation = true;
                    }
                }

                if (!forceRecreation && m_VideoDecoder->notifyWindowChanged(&windowChangeInfo)) {
                    // Update the window display mode based on our current monitor
                    // NB: Avoid a useless modeset by only doing this if it changed.
                    if (newDisplayIndex != currentDisplayIndex) {
                        currentDisplayIndex = newDisplayIndex;
                        updateOptimalWindowDisplayMode();
                    }

                    break;
                }
            }

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Recreating renderer for window event: %d (%d %d)",
                        event.window.event,
                        event.window.data1,
                        event.window.data2);

            // Fall through
        case SDL_RENDER_DEVICE_RESET:

            if (event.type != SDL_WINDOWEVENT) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Recreating renderer by internal request: %d",
                            event.type);
            }

            SDL_LockMutex(m_DecoderLock);

            // Destroy the old decoder
            delete m_VideoDecoder;

            // Insert a barrier to discard any additional window events
            // that could cause the renderer to be and recreated again.
            // We don't use SDL_FlushEvent() here because it could cause
            // important events to be lost.
            flushWindowEvents();

            // Update the window display mode based on our current monitor
            // NB: Avoid a useless modeset by only doing this if it changed.
            if (currentDisplayIndex != SDL_GetWindowDisplayIndex(m_Window)) {
                currentDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);
                updateOptimalWindowDisplayMode();
            }

            // Now that the old decoder is dead, flush any events it may
            // have queued to reset itself (if this reset was the result
            // of device loss or an internal error).
            SDL_PumpEvents();
            SDL_FlushEvent(SDL_RENDER_DEVICE_RESET);

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
                                   m_Preferences->rendererSelection,
                                   m_Window, m_ActiveVideoFormat, m_ActiveVideoWidth,
                                   m_ActiveVideoHeight, m_ActiveVideoFrameRate,
                                   enableVsync,
                                   enableVsync && m_Preferences->framePacing,
                                   m_Preferences->videoEnhancement,
                                   m_Preferences->ignoreAspectRatio,
                                   false,
                                   s_ActiveSession->m_VideoDecoder)) {
                    SDL_UnlockMutex(m_DecoderLock);
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "Failed to recreate decoder after reset");
                    emit displayLaunchError(tr("Unable to initialize video decoder. Please check your streaming settings and try again."));
                    goto DispatchDeferredCleanup;
                }

                // As of SDL 2.0.12, SDL_RecreateWindow() doesn't carry over mouse capture
                // or mouse hiding state to the new window. By capturing after the decoder
                // is set up, this ensures the window re-creation is already done.
                if (needsPostDecoderCreationCapture) {
                    m_InputHandler->setCaptureActive(true);
                    needsPostDecoderCreationCapture = false;
                }
            }

            // Request an IDR frame to complete the reset
            LiRequestIdrFrame();

            // Set HDR mode. We may miss the callback if we're in the middle
            // of recreating our decoder at the time the HDR transition happens.
            m_VideoDecoder->setHdrMode(LiGetCurrentHostDisplayHdrMode());

            // After a window resize, we need to reset the pointer lock region
            m_InputHandler->updatePointerRegionLock();

            SDL_UnlockMutex(m_DecoderLock);
            break;

        case SDL_KEYUP:
        case SDL_KEYDOWN:
            presence.runCallbacks();
            // Ctrl+Alt+Shift+O toggles the Qt overlay menu
            if (event.key.state == SDL_PRESSED &&
                (event.key.keysym.mod & KMOD_CTRL) &&
                (event.key.keysym.mod & KMOD_ALT) &&
                (event.key.keysym.mod & KMOD_SHIFT) &&
                (event.key.keysym.sym == SDLK_o || event.key.keysym.scancode == SDL_SCANCODE_O)) {
                toggleQtOverlayMenu();
                break;
            }
            m_InputHandler->handleKeyEvent(&event.key);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            presence.runCallbacks();

            // When Qt overlay menu is visible, consume all button events
            // to prevent SDL from re-capturing the mouse
            if (m_MenuPanel && m_MenuPanel->isMenuVisible()) {
                break;
            }

            m_InputHandler->handleMouseButtonEvent(&event.button);
            break;
        }
        case SDL_MOUSEMOTION:
        {
            // Qt overlay menu: edge detection with debounce (500ms cooldown after close)
            // Only trigger for edge-based positions (not disabled, at-cursor, or button)
            if (m_MenuPanel && !m_MenuPanel->isMenuVisible() &&
                m_Preferences->overlayMenuPosition != StreamingPreferences::OMP_DISABLED &&
                m_Preferences->overlayMenuPosition != StreamingPreferences::OMP_BUTTON) {
                Uint32 elapsed = SDL_GetTicks() - m_MenuCloseTicks;
                if (elapsed > 500) {
                    int ww, wh;
                    SDL_GetWindowSize(m_Window, &ww, &wh);
                    bool atEdge = false;
                    if (m_Preferences->overlayMenuPosition == StreamingPreferences::OMP_LEFT_EDGE) {
                        atEdge = (event.motion.x <= 5);
                    } else {
                        // OMP_RIGHT_EDGE (default)
                        atEdge = (event.motion.x >= ww - 5);
                    }
                    if (atEdge) {
                        showQtOverlayMenu();
                        break;
                    }
                }
            }

            // When Qt menu is visible, don't forward motion to input handler
            if (m_MenuPanel && m_MenuPanel->isMenuVisible()) {
                break;
            }

            m_InputHandler->handleMouseMotionEvent(&event.motion);
            break;
        }
        case SDL_MOUSEWHEEL:
            m_InputHandler->handleMouseWheelEvent(&event.wheel);
            break;
        case SDL_CONTROLLERAXISMOTION:
            // When menu is visible, consume axis events to prevent game input
            if (m_MenuPanel && m_MenuPanel->isMenuVisible()) {
                break;
            }
            m_InputHandler->handleControllerAxisEvent(&event.caxis);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            presence.runCallbacks();
            // When Qt overlay menu is visible, handle gamepad navigation
            if (m_MenuPanel && m_MenuPanel->isMenuVisible()) {
                if (event.cbutton.state == SDL_PRESSED) {
                    switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        m_MenuPanel->gamepadMoveUp();
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        m_MenuPanel->gamepadMoveDown();
                        break;
                    case SDL_CONTROLLER_BUTTON_A:
                        m_MenuPanel->gamepadSelect();
                        break;
                    case SDL_CONTROLLER_BUTTON_B:
                    case SDL_CONTROLLER_BUTTON_START:
                        m_MenuPanel->gamepadBack();
                        break;
                    default:
                        break;
                    }
                }
                break;
            }
            m_InputHandler->handleControllerButtonEvent(&event.cbutton);
            break;
#if SDL_VERSION_ATLEAST(2, 0, 14)
        case SDL_CONTROLLERSENSORUPDATE:
            m_InputHandler->handleControllerSensorEvent(&event.csensor);
            break;
        case SDL_CONTROLLERTOUCHPADDOWN:
        case SDL_CONTROLLERTOUCHPADUP:
        case SDL_CONTROLLERTOUCHPADMOTION:
            m_InputHandler->handleControllerTouchpadEvent(&event.ctouchpad);
            break;
#endif
#if SDL_VERSION_ATLEAST(2, 24, 0)
        case SDL_JOYBATTERYUPDATED:
            m_InputHandler->handleJoystickBatteryEvent(&event.jbattery);
            break;
#endif
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
            m_InputHandler->handleControllerDeviceEvent(&event.cdevice);
            break;
        case SDL_JOYDEVICEADDED:
            m_InputHandler->handleJoystickArrivalEvent(&event.jdevice);
            break;
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        case SDL_FINGERUP:
            m_InputHandler->handleTouchFingerEvent(&event.tfinger);
            break;
        case SDL_DISPLAYEVENT:
            switch (event.display.event) {
            case SDL_DISPLAYEVENT_CONNECTED:
            case SDL_DISPLAYEVENT_DISCONNECTED:
                m_InputHandler->updatePointerRegionLock();
                break;
            }
            break;
        }

        processClipboardHelperMessages();
        processFileMappingUxProbeResult();
        processFileMappingMountResult();
        processQtEventsDuringStream();

        // Deferred microphone toggle — runs outside processEvents() to avoid
        // heap corruption when creating QAudioSource within nested event loops
        if (m_PendingMicToggle) {
            m_PendingMicToggle = false;
            if (m_MicStream) {
                stopMicrophone();
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Microphone stopped via overlay menu");
            } else {
                startMicrophone();
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Microphone %s via overlay menu",
                            m_MicStream ? "started" : "failed to start");
            }
            // Update the toggle state in menu
            if (m_MenuPanel) {
                m_MenuPanel->updateMicrophoneState(m_MicStream != nullptr);
            }
        }
    }

DispatchDeferredCleanup:
    // Switch back to synchronous logging mode
    StreamUtils::exitAsyncLoggingMode();

    if (m_ClipboardHelper != nullptr) {
        m_ClipboardHelper->processPendingMessages();
        m_ClipboardHelper->stop();
        delete m_ClipboardHelper;
        m_ClipboardHelper = nullptr;
    }

    cleanupFileMappingMount();

    // Destroy the Qt overlay menu panel
    if (m_MenuPanel) {
        m_MenuPanel->closeMenu();
        delete m_MenuPanel;
        m_MenuPanel = nullptr;
    }

    // Destroy the Qt overlay menu button
    if (m_MenuButton) {
        m_MenuButton->hideButton();
        delete m_MenuButton;
        m_MenuButton = nullptr;
    }

    // Destroy the Qt overlay toast
    if (m_Toast) {
        m_Toast->close();
        delete m_Toast;
        m_Toast = nullptr;
    }

    // Uncapture the mouse and hide the window immediately,
    // so we can return to the Qt GUI ASAP.
    m_InputHandler->setCaptureActive(false);
    SDL_EnableScreenSaver();
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "0");
    if (QGuiApplication::platformName() == "eglfs") {
        QGuiApplication::restoreOverrideCursor();
    }

    // Raise any keys that are still down
    m_InputHandler->raiseAllKeys();

    // Destroy the input handler now. This must be destroyed
    // before allowwing the UI to continue execution or it could
    // interfere with SDLGamepadKeyNavigation.
    delete m_InputHandler;
    m_InputHandler = nullptr;

    // Destroy the decoder, since this must be done on the main thread
    // NB: This must happen before LiStopConnection() for pull-based
    // decoders.
    stopSunshineAbr();
    SDL_LockMutex(m_DecoderLock);
    delete m_VideoDecoder;
    m_VideoDecoder = nullptr;
    SDL_UnlockMutex(m_DecoderLock);

    // Propagate state changes from the SDL window back to the Qt window
    //
    // NB: We're making a conscious decision not to propagate the maximized
    // or normal state of the window here. The thinking is that users may
    // routinely maximize the streaming window simply to view the stream
    // in a larger window, but they don't necessarily want the UI in such
    // a large window.
    if (!m_IsFullScreen && m_QtWindow != nullptr && m_Window != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED) {
            m_QtWindow->setWindowStates(m_QtWindow->windowStates() | Qt::WindowMinimized);
        }
        else if (m_QtWindow->windowStates() & Qt::WindowMinimized) {
            m_QtWindow->setWindowStates(m_QtWindow->windowStates() & ~Qt::WindowMinimized);
        }
#else
        if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED) {
            m_QtWindow->setWindowState(Qt::WindowMinimized);
        }
        else if (m_QtWindow->windowState() & Qt::WindowMinimized) {
            m_QtWindow->setWindowState(Qt::WindowNoState);
        }
#endif
    }

    // This must be called after the decoder is deleted, because
    // the renderer may want to interact with the window
    SDL_DestroyWindow(m_Window);

    if (iconSurface != nullptr) {
        SDL_FreeSurface(iconSurface);
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    // Cleanup can take a while, so dispatch it to a worker thread.
    // When it is complete, it will release our s_ActiveSessionSemaphore
    // reference.
    QThreadPool::globalInstance()->start(new DeferredSessionCleanupTask(this));

    // 停止带宽计算
    BandwidthCalculator::instance()->stop();
}

#ifndef STEAM_LINK
void Session::startMicrophone()
{
    if (!m_MicStream) {
        m_MicStream = new MicStream(this);
        if (!m_MicStream->start()) {
            delete m_MicStream;
            m_MicStream = nullptr;
        }
    }
}

void Session::stopMicrophone()
{
    if (m_MicStream) {
        m_MicStream->stop();
        delete m_MicStream;
        m_MicStream = nullptr;
    }
}
#else
void Session::startMicrophone()
{
    // Microphone not supported on Steam Link
}

void Session::stopMicrophone()
{
    // Microphone not supported on Steam Link
}
#endif
