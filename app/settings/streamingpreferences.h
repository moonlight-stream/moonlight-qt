#pragma once

#include <QObject>
#include <QRect>

class StreamingPreferences : public QObject
{
    Q_OBJECT

public:
    StreamingPreferences(QObject *parent = nullptr);

    Q_INVOKABLE static int
    getDefaultBitrate(int width, int height, int fps);

    Q_INVOKABLE void save();

    Q_INVOKABLE static bool hasAnyHardwareAcceleration();

    Q_INVOKABLE static bool isRunningWayland();

    Q_INVOKABLE static bool isWow64();

    Q_INVOKABLE static int getMaximumStreamingFrameRate();

    Q_INVOKABLE QRect getDesktopResolution(int displayIndex);

    Q_INVOKABLE QRect getNativeResolution(int displayIndex);

    Q_INVOKABLE QString getUnmappedGamepads();

    void reload();

    enum AudioConfig
    {
        AC_STEREO,
        AC_51_SURROUND
    };
    Q_ENUM(AudioConfig)

    enum VideoCodecConfig
    {
        VCC_AUTO,
        VCC_FORCE_H264,
        VCC_FORCE_HEVC,
        VCC_FORCE_HEVC_HDR
    };
    Q_ENUM(VideoCodecConfig)

    enum VideoDecoderSelection
    {
        VDS_AUTO,
        VDS_FORCE_HARDWARE,
        VDS_FORCE_SOFTWARE
    };
    Q_ENUM(VideoDecoderSelection)

    enum WindowMode
    {
        WM_FULLSCREEN,
        WM_FULLSCREEN_DESKTOP,
        WM_WINDOWED
    };
    Q_ENUM(WindowMode)

    Q_PROPERTY(int width MEMBER width NOTIFY displayModeChanged)
    Q_PROPERTY(int height MEMBER height NOTIFY displayModeChanged)
    Q_PROPERTY(int fps MEMBER fps NOTIFY displayModeChanged)
    Q_PROPERTY(int bitrateKbps MEMBER bitrateKbps NOTIFY bitrateChanged)
    Q_PROPERTY(bool enableVsync MEMBER enableVsync NOTIFY enableVsyncChanged)
    Q_PROPERTY(bool gameOptimizations MEMBER gameOptimizations NOTIFY gameOptimizationsChanged)
    Q_PROPERTY(bool playAudioOnHost MEMBER playAudioOnHost NOTIFY playAudioOnHostChanged)
    Q_PROPERTY(bool multiController MEMBER multiController NOTIFY multiControllerChanged)
    Q_PROPERTY(bool unsupportedFps MEMBER unsupportedFps NOTIFY unsupportedFpsChanged)
    Q_PROPERTY(bool enableMdns MEMBER enableMdns NOTIFY enableMdnsChanged)
    Q_PROPERTY(bool quitAppAfter MEMBER quitAppAfter NOTIFY quitAppAfterChanged)
    Q_PROPERTY(bool mouseAcceleration MEMBER mouseAcceleration NOTIFY mouseAccelerationChanged)
    Q_PROPERTY(bool startWindowed MEMBER startWindowed NOTIFY startWindowedChanged)
    Q_PROPERTY(AudioConfig audioConfig MEMBER audioConfig NOTIFY audioConfigChanged)
    Q_PROPERTY(VideoCodecConfig videoCodecConfig MEMBER videoCodecConfig NOTIFY videoCodecConfigChanged)
    Q_PROPERTY(VideoDecoderSelection videoDecoderSelection MEMBER videoDecoderSelection NOTIFY videoDecoderSelectionChanged)
    Q_PROPERTY(WindowMode windowMode MEMBER windowMode NOTIFY windowModeChanged)

    // Directly accessible members for preferences
    int width;
    int height;
    int fps;
    int bitrateKbps;
    bool enableVsync;
    bool gameOptimizations;
    bool playAudioOnHost;
    bool multiController;
    bool unsupportedFps;
    bool enableMdns;
    bool quitAppAfter;
    bool mouseAcceleration;
    bool startWindowed;
    AudioConfig audioConfig;
    VideoCodecConfig videoCodecConfig;
    VideoDecoderSelection videoDecoderSelection;
    WindowMode windowMode;

signals:
    void displayModeChanged();
    void bitrateChanged();
    void enableVsyncChanged();
    void gameOptimizationsChanged();
    void playAudioOnHostChanged();
    void multiControllerChanged();
    void unsupportedFpsChanged();
    void enableMdnsChanged();
    void quitAppAfterChanged();
    void mouseAccelerationChanged();
    void audioConfigChanged();
    void videoCodecConfigChanged();
    void videoDecoderSelectionChanged();
    void windowModeChanged();
    void startWindowedChanged();
};

