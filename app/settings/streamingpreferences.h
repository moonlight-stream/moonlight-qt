#pragma once

#include <QObject>
#include <QRect>

class StreamingPreferences : public QObject
{
    Q_OBJECT

public:
    StreamingPreferences();

    Q_INVOKABLE static int
    getDefaultBitrate(int width, int height, int fps);

    Q_INVOKABLE void save();

    Q_INVOKABLE static bool hasAnyHardwareAcceleration();

    Q_INVOKABLE static int getMaximumStreamingFrameRate();

    Q_INVOKABLE QRect getDisplayResolution(int displayIndex);

    void reload();

    enum AudioConfig
    {
        AC_AUTO,
        AC_FORCE_STEREO,
        AC_FORCE_SURROUND
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

    Q_PROPERTY(int width MEMBER width NOTIFY displayModeChanged)
    Q_PROPERTY(int height MEMBER height NOTIFY displayModeChanged)
    Q_PROPERTY(int fps MEMBER fps NOTIFY displayModeChanged)
    Q_PROPERTY(int bitrateKbps MEMBER bitrateKbps NOTIFY bitrateChanged)
    Q_PROPERTY(bool fullScreen MEMBER fullScreen NOTIFY fullScreenChanged)
    Q_PROPERTY(bool gameOptimizations MEMBER gameOptimizations NOTIFY gameOptimizationsChanged)
    Q_PROPERTY(bool playAudioOnHost MEMBER playAudioOnHost NOTIFY playAudioOnHostChanged)
    Q_PROPERTY(bool multiController MEMBER multiController NOTIFY multiControllerChanged)
    Q_PROPERTY(AudioConfig audioConfig MEMBER audioConfig NOTIFY audioConfigChanged)
    Q_PROPERTY(VideoCodecConfig videoCodecConfig MEMBER videoCodecConfig NOTIFY videoCodecConfigChanged)
    Q_PROPERTY(VideoDecoderSelection videoDecoderSelection MEMBER videoDecoderSelection NOTIFY videoDecoderSelectionChanged)

    // Directly accessible members for preferences
    int width;
    int height;
    int fps;
    int bitrateKbps;
    bool fullScreen;
    bool gameOptimizations;
    bool playAudioOnHost;
    bool multiController;
    AudioConfig audioConfig;
    VideoCodecConfig videoCodecConfig;
    VideoDecoderSelection videoDecoderSelection;

signals:
    void displayModeChanged();
    void bitrateChanged();
    void fullScreenChanged();
    void gameOptimizationsChanged();
    void playAudioOnHostChanged();
    void multiControllerChanged();
    void audioConfigChanged();
    void videoCodecConfigChanged();
    void videoDecoderSelectionChanged();
};

