#pragma once

#include <QObject>
#include <QRect>
#include <QQmlEngine>

class StreamingPreferences : public QObject
{
    Q_OBJECT

public:
    static StreamingPreferences* get(QQmlEngine *qmlEngine = nullptr);

    Q_INVOKABLE static int
    getDefaultBitrate(int width, int height, int fps, bool yuv444);

    Q_INVOKABLE void save();

    void reload();

    enum AudioConfig
    {
        AC_STEREO,
        AC_51_SURROUND,
        AC_71_SURROUND,
        AC_714_SURROUND
    };
    Q_ENUM(AudioConfig)

    enum VideoCodecConfig
    {
        VCC_AUTO,
        VCC_FORCE_H264,
        VCC_FORCE_HEVC,
        VCC_FORCE_HEVC_HDR_DEPRECATED, // Kept for backwards compatibility
        VCC_FORCE_AV1
    };
    Q_ENUM(VideoCodecConfig)

    enum VideoDecoderSelection
    {
        VDS_AUTO,
        VDS_FORCE_HARDWARE,
        VDS_FORCE_SOFTWARE
    };
    Q_ENUM(VideoDecoderSelection)

    // Mac only (for now)
    enum RendererSelection
    {
        RS_PROBE_ONLY = -1, // Only valid for probing decoder properties
        RS_AUTO,
        RS_VULKAN,
        RS_METAL,
        RS_AVSBDL
    };
    Q_ENUM(RendererSelection)

    enum WindowMode
    {
        WM_FULLSCREEN,
        WM_FULLSCREEN_DESKTOP,
        WM_WINDOWED
    };
    Q_ENUM(WindowMode)

    enum UIDisplayMode
    {
        UI_WINDOWED,
        UI_MAXIMIZED,
        UI_FULLSCREEN
    };
    Q_ENUM(UIDisplayMode)

    // New entries must go at the end of the enum
    // to avoid renumbering existing entries (which
    // would affect existing user preferences).
    enum Language
    {
        LANG_AUTO,
        LANG_EN,
        LANG_FR,
        LANG_ZH_CN,
        LANG_DE,
        LANG_NB_NO,
        LANG_RU,
        LANG_ES,
        LANG_JA,
        LANG_VI,
        LANG_TH,
        LANG_KO,
        LANG_HU,
        LANG_NL,
        LANG_SV,
        LANG_TR,
        LANG_UK,
        LANG_ZH_TW,
        LANG_PT,
        LANG_PT_BR,
        LANG_EL,
        LANG_IT,
        LANG_HI,
        LANG_PL,
        LANG_CS,
        LANG_HE,
        LANG_CKB,
        LANG_LT,
        LANG_ET,
        LANG_BG,
        LANG_EO,
        LANG_TA,
    };
    Q_ENUM(Language);

    enum CaptureSysKeysMode
    {
        CSK_OFF,
        CSK_FULLSCREEN,
        CSK_ALWAYS,
    };
    Q_ENUM(CaptureSysKeysMode);

    enum OverlayMenuPosition
    {
        OMP_DISABLED   = 0,  // Default: do not show the overlay menu
        OMP_BUTTON     = 1,  // Show a floating button on the streaming window
        OMP_TOP_EDGE   = 2,  // Show from the top edge of the streaming window
        OMP_RIGHT_EDGE = 3,  // Show on right edge
        OMP_LEFT_EDGE  = 4,  // Show on left edge
    };
    Q_ENUM(OverlayMenuPosition);

    void setOverlayMenuPosition(OverlayMenuPosition position);

    enum HdrMode
    {
        HDR_PQ  = 1,   // HDR10/PQ (SMPTE ST 2084) - default
        HDR_HLG = 2,   // HLG (Hybrid Log-Gamma, ARIB STD-B67)
    };
    Q_ENUM(HdrMode);

    enum HdrBrightnessMode
    {
        HBM_HOST_DEFAULT = 0,
        HBM_AUTO = 1,
        HBM_MANUAL = 2,
    };
    Q_ENUM(HdrBrightnessMode);

    enum DualSenseHapticsMode
    {
        DSHM_PHYSICAL = 0,
        DSHM_EMULATED = 1,
    };
    Q_ENUM(DualSenseHapticsMode);

    enum GamepadQuitCombo
    {
        GQC_DEFAULT         = 0,  // Start + Select + L1 + R1 (original)
        GQC_SELECT_L1_R1_X  = 1,  // Select + L1 + R1 + X (avoids Start+Select conflict)
        GQC_SELECT_L1_R1_Y  = 2,  // Select + L1 + R1 + Y
        GQC_START_L1_R1_A   = 3,  // Start + L1 + R1 + A (avoids Select conflict)
        GQC_START_L1_R1_B   = 4,  // Start + L1 + R1 + B (avoids Select conflict)
        GQC_L1_R1_X_Y       = 5,  // L1 + R1 + X + Y (no Select/Start at all)
        GQC_L1_R1_A_B       = 6,  // L1 + R1 + A + B (no Select/Start at all)
    };
    Q_ENUM(GamepadQuitCombo);

    enum ScreenCombinationMode
    {
        SCM_FOLLOW_HOST = -1,
        SCM_NO_OPERATION = 0,
        SCM_ENSURE_ACTIVE = 1,
        SCM_ENSURE_PRIMARY = 2,
        SCM_ENSURE_ONLY_DISPLAY = 3,
        SCM_ENSURE_SECONDARY = 4,
    };
    Q_ENUM(ScreenCombinationMode);

    Q_PROPERTY(int width MEMBER width NOTIFY displayModeChanged)
    Q_PROPERTY(int height MEMBER height NOTIFY displayModeChanged)
    Q_PROPERTY(int fps MEMBER fps NOTIFY displayModeChanged)
    Q_PROPERTY(int bitrateKbps MEMBER bitrateKbps NOTIFY bitrateChanged)
    Q_PROPERTY(bool autoAdjustBitrate MEMBER autoAdjustBitrate NOTIFY autoAdjustBitrateChanged)
    Q_PROPERTY(bool enableSunshineAbr MEMBER enableSunshineAbr NOTIFY enableSunshineAbrChanged)
    Q_PROPERTY(bool ignoreAspectRatio MEMBER ignoreAspectRatio NOTIFY ignoreAspectRatioChanged)
    Q_PROPERTY(bool enableVsync MEMBER enableVsync NOTIFY enableVsyncChanged)
    Q_PROPERTY(bool gameOptimizations MEMBER gameOptimizations NOTIFY gameOptimizationsChanged)
    Q_PROPERTY(bool playAudioOnHost MEMBER playAudioOnHost NOTIFY playAudioOnHostChanged)
    Q_PROPERTY(bool multiController MEMBER multiController NOTIFY multiControllerChanged)
    Q_PROPERTY(bool enableMdns MEMBER enableMdns NOTIFY enableMdnsChanged)
    Q_PROPERTY(bool quitAppAfter MEMBER quitAppAfter NOTIFY quitAppAfterChanged)
    Q_PROPERTY(bool absoluteMouseMode MEMBER absoluteMouseMode NOTIFY absoluteMouseModeChanged)
    Q_PROPERTY(bool showLocalCursor MEMBER showLocalCursor NOTIFY showLocalCursorChanged)
    Q_PROPERTY(bool absoluteTouchMode MEMBER absoluteTouchMode NOTIFY absoluteTouchModeChanged)
    Q_PROPERTY(bool enableNativeTouchpad MEMBER enableNativeTouchpad NOTIFY enableNativeTouchpadChanged)
    Q_PROPERTY(DualSenseHapticsMode dualSenseHapticsMode MEMBER dualSenseHapticsMode NOTIFY dualSenseHapticsModeChanged)
    Q_PROPERTY(bool framePacing MEMBER framePacing NOTIFY framePacingChanged)
    Q_PROPERTY(bool videoEnhancement MEMBER videoEnhancement NOTIFY videoEnhancementChanged)
    Q_PROPERTY(bool streamResolutionScale MEMBER streamResolutionScale NOTIFY streamResolutionScaleChanged)
    Q_PROPERTY(int streamResolutionScaleRatio MEMBER streamResolutionScaleRatio NOTIFY streamResolutionScaleRatioChanged)
    Q_PROPERTY(bool remoteResolution MEMBER remoteResolution NOTIFY remoteResolutionChanged)
    Q_PROPERTY(int remoteResolutionWidth MEMBER remoteResolutionWidth NOTIFY remoteResolutionWidthChanged)
    Q_PROPERTY(int remoteResolutionHeight MEMBER remoteResolutionHeight NOTIFY remoteResolutionHeightChanged)
    Q_PROPERTY(bool remoteFps MEMBER remoteFps NOTIFY remoteFpsChanged)
    Q_PROPERTY(int remoteFpsRate MEMBER remoteFpsRate NOTIFY remoteFpsRateChanged)
    Q_PROPERTY(bool connectionWarnings MEMBER connectionWarnings NOTIFY connectionWarningsChanged)
    Q_PROPERTY(bool configurationWarnings MEMBER configurationWarnings NOTIFY configurationWarningsChanged)
    Q_PROPERTY(bool richPresence MEMBER richPresence NOTIFY richPresenceChanged)
    Q_PROPERTY(bool gamepadMouse MEMBER gamepadMouse NOTIFY gamepadMouseChanged)
    Q_PROPERTY(bool detectNetworkBlocking MEMBER detectNetworkBlocking NOTIFY detectNetworkBlockingChanged)
    Q_PROPERTY(bool showPerformanceOverlay MEMBER showPerformanceOverlay NOTIFY showPerformanceOverlayChanged)
    Q_PROPERTY(AudioConfig audioConfig MEMBER audioConfig NOTIFY audioConfigChanged)
    Q_PROPERTY(VideoCodecConfig videoCodecConfig MEMBER videoCodecConfig NOTIFY videoCodecConfigChanged)
    Q_PROPERTY(bool enableHdr MEMBER enableHdr NOTIFY enableHdrChanged)
    Q_PROPERTY(HdrMode hdrMode MEMBER hdrMode NOTIFY hdrModeChanged)
    Q_PROPERTY(HdrBrightnessMode hdrBrightnessMode MEMBER hdrBrightnessMode NOTIFY hdrBrightnessModeChanged)
    Q_PROPERTY(double hdrMaxBrightness MEMBER hdrMaxBrightness NOTIFY hdrBrightnessValuesChanged)
    Q_PROPERTY(double hdrMinBrightness MEMBER hdrMinBrightness NOTIFY hdrBrightnessValuesChanged)
    Q_PROPERTY(double hdrMaxAverageBrightness MEMBER hdrMaxAverageBrightness NOTIFY hdrBrightnessValuesChanged)
    Q_PROPERTY(bool enableYUV444 MEMBER enableYUV444 NOTIFY enableYUV444Changed)
    Q_PROPERTY(VideoDecoderSelection videoDecoderSelection MEMBER videoDecoderSelection NOTIFY videoDecoderSelectionChanged)
    Q_PROPERTY(RendererSelection rendererSelection MEMBER rendererSelection NOTIFY rendererSelectionChanged)
    Q_PROPERTY(WindowMode windowMode MEMBER windowMode NOTIFY windowModeChanged)
    Q_PROPERTY(WindowMode recommendedFullScreenMode MEMBER recommendedFullScreenMode CONSTANT)
    Q_PROPERTY(UIDisplayMode uiDisplayMode MEMBER uiDisplayMode NOTIFY uiDisplayModeChanged)
    Q_PROPERTY(bool rememberWindowPosition MEMBER rememberWindowPosition NOTIFY rememberWindowPositionChanged)
    Q_PROPERTY(bool swapMouseButtons MEMBER swapMouseButtons NOTIFY mouseButtonsChanged)
    Q_PROPERTY(bool swapWinAltKeys MEMBER swapWinAltKeys NOTIFY swapWinAltKeysChanged)
    Q_PROPERTY(bool muteOnFocusLoss MEMBER muteOnFocusLoss NOTIFY muteOnFocusLossChanged)
    Q_PROPERTY(bool backgroundGamepad MEMBER backgroundGamepad NOTIFY backgroundGamepadChanged)
    Q_PROPERTY(GamepadQuitCombo gamepadQuitCombo MEMBER gamepadQuitCombo NOTIFY gamepadQuitComboChanged)
    Q_PROPERTY(bool reverseScrollDirection MEMBER reverseScrollDirection NOTIFY reverseScrollDirectionChanged)
    Q_PROPERTY(bool swapFaceButtons MEMBER swapFaceButtons NOTIFY swapFaceButtonsChanged)
    Q_PROPERTY(bool keepAwake MEMBER keepAwake NOTIFY keepAwakeChanged)
    Q_PROPERTY(CaptureSysKeysMode captureSysKeysMode MEMBER captureSysKeysMode NOTIFY captureSysKeysModeChanged)
    Q_PROPERTY(Language language MEMBER language NOTIFY languageChanged)
    Q_PROPERTY(ScreenCombinationMode screenCombinationMode MEMBER screenCombinationMode NOTIFY screenCombinationModeChanged)
    Q_PROPERTY(bool enableMicrophone MEMBER enableMicrophone NOTIFY enableMicrophoneChanged)
    Q_PROPERTY(OverlayMenuPosition overlayMenuPosition MEMBER overlayMenuPosition NOTIFY overlayMenuPositionChanged)
    Q_PROPERTY(bool autoUpdateCheck MEMBER autoUpdateCheck NOTIFY autoUpdateCheckChanged)

    Q_INVOKABLE bool retranslate();

    // Directly accessible members for preferences
    int width;
    int height;
    int fps;
    int bitrateKbps;
    bool autoAdjustBitrate;
    bool enableSunshineAbr;
    bool ignoreAspectRatio;
    bool enableVsync;
    bool gameOptimizations;
    bool playAudioOnHost;
    bool multiController;
    bool enableMdns;
    bool quitAppAfter;
    bool absoluteMouseMode;
    bool showLocalCursor;
    bool absoluteTouchMode;
    bool enableNativeTouchpad;
    DualSenseHapticsMode dualSenseHapticsMode;
    bool framePacing;
    bool videoEnhancement;
    bool streamResolutionScale;
    int streamResolutionScaleRatio;
    bool remoteResolution;
    int remoteResolutionWidth;
    int remoteResolutionHeight;
    bool remoteFps;
    int remoteFpsRate;
    bool connectionWarnings;
    bool configurationWarnings;
    bool richPresence;
    bool gamepadMouse;
    bool detectNetworkBlocking;
    bool showPerformanceOverlay;
    bool swapMouseButtons;
    bool swapWinAltKeys;
    bool muteOnFocusLoss;
    bool backgroundGamepad;
    GamepadQuitCombo gamepadQuitCombo;
    bool reverseScrollDirection;
    bool swapFaceButtons;
    bool keepAwake;
    int packetSize;
    AudioConfig audioConfig;
    VideoCodecConfig videoCodecConfig;
    bool enableHdr;
    HdrMode hdrMode;
    HdrBrightnessMode hdrBrightnessMode;
    double hdrMaxBrightness;
    double hdrMinBrightness;
    double hdrMaxAverageBrightness;
    bool enableYUV444;
    VideoDecoderSelection videoDecoderSelection;
    WindowMode windowMode;
    WindowMode recommendedFullScreenMode;
    UIDisplayMode uiDisplayMode;
    bool rememberWindowPosition;
    Language language;
    CaptureSysKeysMode captureSysKeysMode;
    ScreenCombinationMode screenCombinationMode;
    bool enableMicrophone;
    OverlayMenuPosition overlayMenuPosition;
    bool autoUpdateCheck;
    RendererSelection rendererSelection;

signals:
    void displayModeChanged();
    void bitrateChanged();
    void autoAdjustBitrateChanged();
    void enableSunshineAbrChanged();
    void ignoreAspectRatioChanged();
    void enableVsyncChanged();
    void gameOptimizationsChanged();
    void playAudioOnHostChanged();
    void multiControllerChanged();
    void unsupportedFpsChanged();
    void enableMdnsChanged();
    void quitAppAfterChanged();
    void absoluteMouseModeChanged();
    void showLocalCursorChanged();
    void absoluteTouchModeChanged();
    void enableNativeTouchpadChanged();
    void dualSenseHapticsModeChanged();
    void audioConfigChanged();
    void videoCodecConfigChanged();
    void enableHdrChanged();
    void hdrModeChanged();
    void hdrBrightnessModeChanged();
    void hdrBrightnessValuesChanged();
    void enableYUV444Changed();
    void videoDecoderSelectionChanged();
    void uiDisplayModeChanged();
    void rememberWindowPositionChanged();
    void windowModeChanged();
    void framePacingChanged();
    void videoEnhancementChanged();
    void streamResolutionScaleChanged();
    void streamResolutionScaleRatioChanged();
    void remoteResolutionChanged();
    void remoteResolutionWidthChanged();
    void remoteResolutionHeightChanged();
    void remoteFpsChanged();
    void remoteFpsRateChanged();
    void connectionWarningsChanged();
    void configurationWarningsChanged();
    void richPresenceChanged();
    void gamepadMouseChanged();
    void detectNetworkBlockingChanged();
    void showPerformanceOverlayChanged();
    void mouseButtonsChanged();
    void swapWinAltKeysChanged();
    void muteOnFocusLossChanged();
    void backgroundGamepadChanged();
    void gamepadQuitComboChanged();
    void reverseScrollDirectionChanged();
    void swapFaceButtonsChanged();
    void captureSysKeysModeChanged();
    void keepAwakeChanged();
    void languageChanged();
    void screenCombinationModeChanged();
    void enableMicrophoneChanged();
    void overlayMenuPositionChanged();
    void autoUpdateCheckChanged();
    void rendererSelectionChanged();

private:
    explicit StreamingPreferences(QQmlEngine *qmlEngine);

    QString getSuffixFromLanguage(Language lang);

    QQmlEngine* m_QmlEngine;
};

