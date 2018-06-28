#pragma once

class StreamingPreferences
{
public:
    StreamingPreferences();

    static int
    getDefaultBitrate(int width, int height, int fps);

    void save();

    void reload();

    enum AudioConfig
    {
        AC_AUTO,
        AC_FORCE_STEREO,
        AC_FORCE_SURROUND
    };

    enum VideoCodecConfig
    {
        VCC_AUTO,
        VCC_FORCE_H264,
        VCC_FORCE_HEVC,
        VCC_FORCE_HEVC_HDR
    };

    // Directly accessible members for preferences
    int width;
    int height;
    int fps;
    int bitrateKbps;
    bool fullScreen;
    bool enableGameOptimizations;
    bool playAudioOnHost;
    bool multiController;
    AudioConfig audioConfig;
    VideoCodecConfig videoCodecConfig;
};

