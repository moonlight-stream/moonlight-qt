#pragma once

#include <QSemaphore>
#include <QPoint>
#include <QQuickWindow>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>

#include <Limelight.h>
#include <opus_multistream.h>
#include "settings/streamingpreferences.h"
#include "input/input.h"
#include "video/decoder.h"
#include "audio/renderers/renderer.h"
#include "video/overlaymanager.h"
#include "video/overlaymenupanel.h"
#include "video/overlaymenubutton.h"
#include "video/overlaytoast.h"
#ifndef STEAM_LINK
#include "micstream.h"
#endif

namespace FileMappingUx {
struct ProbeState;
struct MountState;
}

class DualSenseHapticsRenderer;
#ifdef MOONLIGHT_ENABLE_FUNCTION_TESTS
class StylusReplayTest;
#endif

class SupportedVideoFormatList : public QList<int>
{
public:
    operator int() const
    {
        int value = 0;

        for (const int v : *this) {
            value |= v;
        }

        return value;
    }

    void
    removeByMask(int mask)
    {
        int i = 0;
        while (i < this->length()) {
            if (this->value(i) & mask) {
                this->removeAt(i);
            }
            else {
                i++;
            }
        }
    }

    void
    deprioritizeByMask(int mask)
    {
        QList<int> deprioritizedList;

        int i = 0;
        while (i < this->length()) {
            if (this->value(i) & mask) {
                deprioritizedList.append(this->takeAt(i));
            }
            else {
                i++;
            }
        }

        this->append(std::move(deprioritizedList));
    }

    int maskByServerCodecModes(int serverCodecModes)
    {
        int mask = 0;

        const QMap<int, int> mapping = {
            {SCM_H264, VIDEO_FORMAT_H264},
            {SCM_H264_HIGH8_444, VIDEO_FORMAT_H264_HIGH8_444},
            {SCM_HEVC, VIDEO_FORMAT_H265},
            {SCM_HEVC_MAIN10, VIDEO_FORMAT_H265_MAIN10},
            {SCM_HEVC_REXT8_444, VIDEO_FORMAT_H265_REXT8_444},
            {SCM_HEVC_REXT10_444, VIDEO_FORMAT_H265_REXT10_444},
            {SCM_AV1_MAIN8, VIDEO_FORMAT_AV1_MAIN8},
            {SCM_AV1_MAIN10, VIDEO_FORMAT_AV1_MAIN10},
            {SCM_AV1_HIGH8_444, VIDEO_FORMAT_AV1_HIGH8_444},
            {SCM_AV1_HIGH10_444, VIDEO_FORMAT_AV1_HIGH10_444},
        };

        for (QMap<int, int>::const_iterator it = mapping.cbegin(); it != mapping.cend(); ++it) {
            if (serverCodecModes & it.key()) {
                mask |= it.value();
                serverCodecModes &= ~it.key();
            }
        }

        // Make sure nobody forgets to update this for new SCM values
        SDL_assert(serverCodecModes == 0);

        int val = *this;
        return val & mask;
    }
};

class Session : public QObject
{
    Q_OBJECT

    friend class SdlInputHandler;
    friend class DeferredSessionCleanupTask;
    friend class AsyncConnectionStartThread;

public:
    explicit Session(NvComputer* computer,
                     NvApp& app,
                     StreamingPreferences *preferences = nullptr,
                     QString launchDisplayName = QString(),
                     std::optional<bool> launchUseVdd = std::nullopt);
    virtual ~Session();

    Q_INVOKABLE bool initialize(QQuickWindow* qtWindow);
    Q_INVOKABLE void start();
    Q_INVOKABLE void interrupt();
    Q_PROPERTY(QStringList launchWarnings MEMBER m_LaunchWarnings NOTIFY launchWarningsChanged);

    static
    void getDecoderInfo(SDL_Window* window,
                        bool& isHardwareAccelerated, bool& isFullScreenOnly,
                        bool& isHdrSupported, QSize& maxResolution);

    static Session* get()
    {
        return s_ActiveSession;
    }

    Overlay::OverlayManager& getOverlayManager()
    {
        return m_OverlayManager;
    }

    void flushWindowEvents();

    void setShouldExit(bool quitHostApp = false);

signals:
    void stageStarting(QString stage);

    void stageFailed(QString stage, int errorCode, QString failingPorts);

    void connectionStarted();

    void displayLaunchError(QString text);

    void quitStarting();

    void sessionFinished(int portTestResult);

    // Emitted after sessionFinished() when the session is ready to be destroyed
    void readyForDeletion();

    void launchWarningsChanged();

private:
    void exec();

    bool startConnectionAsync();

    // Attempt to silently re-establish a streaming session after an
    // unexpected network interruption. Returns true if streaming resumed.
    bool tryReconnect();

    void handleSdlUserEvent(const SDL_UserEvent& event);

    void updateDualSenseHapticsControllerTarget();

    // Emit the appropriate error dialog for a connection termination code.
    void displayTerminationError(int errorCode);

    bool validateLaunch(SDL_Window* testWindow);

    void emitLaunchWarning(QString text);

    bool populateDecoderProperties(SDL_Window* window);

    IAudioRenderer* createAudioRenderer(const POPUS_MULTISTREAM_CONFIGURATION opusConfig);

    bool initializeAudioRenderer();

    bool testAudio(int audioConfiguration);

    int getAudioRendererCapabilities(int audioConfiguration);

    void getWindowDimensions(int& x, int& y,
                             int& width, int& height);

    void toggleFullscreen();

    // Qt-based overlay menu
    void showQtOverlayMenu(std::optional<QPoint> pointerGlobalPosition = std::nullopt,
                           bool closeWhenPointerOutside = true);
    void hideQtOverlayMenu();
    void toggleQtOverlayMenu();
    bool isStreamingWindowVisible() const;
    void syncQtOverlayWindowsWithSdlWindowState();
    void dispatchQtMenuAction(OverlayMenuPanel::MenuAction action);
    void requestRuntimeBitrateChange(int bitrateKbps);
    void showStreamingToast(const QString& message, int durationMs = 2000);
    void updateFileMappingMenuState();
    bool openFileMappingMountPath();
#ifdef MOONLIGHT_ENABLE_FUNCTION_TESTS
    void restoreCaptureAfterStylusReplayPanel();
#endif
#ifdef Q_OS_WIN32
    void queryDisplayHdrBrightness(const QString& preferredDisplayName,
                                   float& maxNits, float& minNits,
                                   float& maxFullNits, float& sdrWhiteNits);
    float queryDisplaySdrWhiteNits(const QString& displayName, bool logFailures = true);
#endif

    void notifyMouseEmulationMode(bool enabled);

    static
    bool queueTouchpadFrameFlush();

    static
    bool queueCursorVisibilityFlush();

    void updateOptimalWindowDisplayMode();

    enum class DecoderAvailability {
        None,
        Software,
        Hardware
    };

    static
    DecoderAvailability getDecoderAvailability(SDL_Window* window,
                                               StreamingPreferences::VideoDecoderSelection vds,
                                               int videoFormat, int width, int height, int frameRate);

    static
    bool chooseDecoder(StreamingPreferences::VideoDecoderSelection vds,
                       StreamingPreferences::RendererSelection renderer,
                       SDL_Window* window, int videoFormat, int width, int height,
                       int frameRate, bool enableVsync, bool enableFramePacing,
                       bool enableVideoEnhancement, bool ignoreAspectRatio, bool testOnly,
                       IVideoDecoder*& chosenDecoder);

    static
    void clStageStarting(int stage);

    static
    void clStageFailed(int stage, int errorCode);

    static
    void clConnectionTerminated(int errorCode);

    static
    void clLogMessage(const char* format, ...);

    static
    void clRumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor);

    static
    void clConnectionStatusUpdate(int connectionStatus);

    static
    void clSetHdrMode(bool enabled, void* hdrMetadata);

    static
    void clRumbleTriggers(uint16_t controllerNumber, uint16_t leftTrigger, uint16_t rightTrigger);

    static
    void clSetMotionEventState(uint16_t controllerNumber, uint8_t motionType, uint16_t reportRateHz);

    static
    void clSetControllerLED(uint16_t controllerNumber, uint8_t r, uint8_t g, uint8_t b);

    static
    void clSetAdaptiveTriggers(uint16_t controllerNumber, uint8_t eventFlags, uint8_t typeLeft, uint8_t typeRight, uint8_t *left, uint8_t *right);

    static
    void clClipboardData(const char* data, int length);

    static
    void clCursorUpdate(const LI_CURSOR_UPDATE* update);

    static
    void clDs5HapticsPcm(const LI_DS5_HAPTICS_PCM_FRAME* frame);

    static
    void clDs5HapticsIrV2(const LI_DS5_HAPTICS_IR_FRAME_V2* frame);

    static
    int arInit(int audioConfiguration,
               const POPUS_MULTISTREAM_CONFIGURATION opusConfig,
               void* arContext, int arFlags);

    static
    void arCleanup();

    static
    void arDecodeAndPlaySample(char* sampleData, int sampleLength);

    static
    int drSetup(int videoFormat, int width, int height, int frameRate, void*, int);

    static
    void drCleanup();

    void startMicrophone();
    void stopMicrophone();

    void startSunshineAbr();
    void stopSunshineAbr();
    void sendSunshineAbrFeedback();
    void startFileMappingUxProbe();
    void processFileMappingUxProbeResult();
    void startFileMappingMount();
    void processFileMappingMountResult();
    void cleanupFileMappingMount();
    void startFileMappingSmokeProbe();

    static
    int drSubmitDecodeUnit(PDECODE_UNIT du);

    StreamingPreferences* m_Preferences;
    bool m_IsFullScreen;
    SupportedVideoFormatList m_SupportedVideoFormats; // Sorted in order of descending priority
    STREAM_CONFIGURATION m_StreamConfig;
    DECODER_RENDERER_CALLBACKS m_VideoCallbacks;
    AUDIO_RENDERER_CALLBACKS m_AudioCallbacks;
    NvComputer* m_Computer;
    NvApp m_App;
    QString m_LaunchDisplayName;
    QString m_ClientDisplayName;
    std::optional<bool> m_LaunchUseVdd;
    SDL_Window* m_Window;
    IVideoDecoder* m_VideoDecoder;
    SDL_mutex* m_DecoderLock;
    bool m_AudioDisabled;
    bool m_AudioMuted;
    Uint32 m_FullScreenFlag;
    QQuickWindow* m_QtWindow;
    bool m_UnexpectedTermination;
    SdlInputHandler* m_InputHandler;
    int m_MouseEmulationRefCount;
    int m_FlushingWindowEventsRef;
    QStringList m_LaunchWarnings;
    bool m_ShouldExit;

    // Graceful reconnect state
    bool m_ConnectionInterrupted;        // set by clConnectionTerminated for recoverable errors
    bool m_SuppressConnectionErrorDialog; // suppress error dialogs during reconnect attempts
    bool m_HasReceivedVideo;             // true after the first decode unit of the current connection
    int m_LastTerminationErrorCode;      // stored to show final error if reconnect gives up

    bool m_AsyncConnectionSuccess;
    float m_LastClientSdrWhiteNits;
    int m_PortTestResults;

    int m_ActiveVideoFormat;
    int m_ActiveVideoWidth;
    int m_ActiveVideoHeight;
    int m_ActiveVideoFrameRate;

    OpusMSDecoder* m_OpusDecoder;
    IAudioRenderer* m_AudioRenderer;
    DualSenseHapticsRenderer* m_DualSenseHapticsRenderer;
    OPUS_MULTISTREAM_CONFIGURATION m_ActiveAudioConfig;
    OPUS_MULTISTREAM_CONFIGURATION m_OriginalAudioConfig;
    int m_AudioSampleCount;
    Uint32 m_DropAudioEndTime;

    Overlay::OverlayManager m_OverlayManager;
    bool m_WasCapturedBeforeMenu;  // 菜单打开前鼠标是否处于捕获状态
    bool m_DeferCaptureRestore;    // 延迟恢复鼠标捕获（全屏切换等）
    bool m_PendingMicToggle;       // 延迟麦克风切换（避免堆损坏）
#ifdef MOONLIGHT_ENABLE_FUNCTION_TESTS
    // Developer-only test harness. All replay/UI behavior lives behind this
    // boundary so production Session code keeps only integration hooks.
    std::unique_ptr<StylusReplayTest> m_StylusReplayTest;
    bool m_WasCapturedBeforeStylusReplayPanel;
#endif
    bool m_SunshineAbrEnabled;
    Uint32 m_LastAbrFeedbackTicks;
    RTP_VIDEO_STATS m_LastAbrVideoStats;
    std::shared_ptr<std::atomic_bool> m_AbrFeedbackInFlight;
    std::shared_ptr<std::atomic_int> m_AbrCurrentBitrateKbps;
    OverlayMenuPanel* m_MenuPanel; // Qt-based overlay menu window
    OverlayMenuButton* m_MenuButton; // Qt-based floating menu button
    OverlayToast* m_Toast;           // Qt-based toast notification
    OverlayMenuPanel::FileMappingState m_FileMappingState;
    QString m_FileMappingDetail;
    QString m_FileMappingToast;
    bool m_FileMappingToastPending;
    std::shared_ptr<FileMappingUx::ProbeState> m_FileMappingProbeState;
    std::shared_ptr<FileMappingUx::MountState> m_FileMappingMountState;
    QString m_FileMappingMountPath;
    QString m_FileMappingSessionId;
    Uint32 m_MenuCloseTicks;       // 菜单关闭时间戳（防抖）
    class ClipboardHelperClient* m_ClipboardHelper; // Bidirectional clipboard sync helper process; nullptr when stream not active
    std::mutex m_CursorUpdateMutex;
    std::shared_ptr<RemoteCursorUpdate> m_PendingCursorUpdate;
    bool m_CursorUpdateEventQueued = false;

    static CONNECTION_LISTENER_CALLBACKS k_ConnCallbacks;
    static Session* s_ActiveSession;
    static QSemaphore s_ActiveSessionSemaphore;
#ifndef STEAM_LINK
    MicStream* m_MicStream;
#else
    void* m_MicStream;
#endif
};
