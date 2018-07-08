#pragma once

#include <Limelight.h>
#include <opus_multistream.h>
#include "backend/computermanager.h"
#include "settings/streamingpreferences.h"
#include "input.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

#define SDL_CODE_FRAME_READY 0

class Session : public QObject
{
    Q_OBJECT

    friend class SdlInputHandler;

public:
    explicit Session(NvComputer* computer, NvApp& app);

    Q_INVOKABLE void exec();

signals:
    void stageStarting(QString stage);

    void stageFailed(QString stage, long errorCode);

    void connectionStarted();

    void displayLaunchError(QString text);

    void displayLaunchWarning(QString text);

private:
    bool validateLaunch();

    int getDecoderCapabilities();

    int sdlDetermineAudioConfiguration();

    void renderFrame(SDL_UserEvent* event);

    void dropFrame(SDL_UserEvent* event);

    static
    void clStageStarting(int stage);

    static
    void clStageFailed(int stage, long errorCode);

    static
    void clConnectionTerminated(long errorCode);

    static
    void clLogMessage(const char* format, ...);

    static
    int drSetup(int videoFormat, int width, int height, int frameRate, void*, int);

    static
    void drCleanup();

    static
    int drSubmitDecodeUnit(PDECODE_UNIT du);

    static
    int sdlAudioInit(int audioConfiguration,
                     POPUS_MULTISTREAM_CONFIGURATION opusConfig,
                     void* arContext, int arFlags);

    static
    void sdlAudioStart();

    static
    void sdlAudioStop();

    static
    void sdlAudioCleanup();

    static
    void sdlAudioDecodeAndPlaySample(char* sampleData, int sampleLength);

    StreamingPreferences m_Preferences;
    STREAM_CONFIGURATION m_StreamConfig;
    DECODER_RENDERER_CALLBACKS m_VideoCallbacks;
    NvComputer* m_Computer;
    NvApp m_App;
    SDL_Window* m_Window;
    SDL_Renderer* m_Renderer;
    SDL_Texture* m_Texture;

    static AVPacket s_Pkt;
    static AVCodecContext* s_VideoDecoderCtx;
    static QByteArray s_DecodeBuffer;

    static SDL_AudioDeviceID s_AudioDevice;
    static OpusMSDecoder* s_OpusDecoder;
    static short s_OpusDecodeBuffer[];
    static int s_ChannelCount;

    static AUDIO_RENDERER_CALLBACKS k_AudioCallbacks;
    static CONNECTION_LISTENER_CALLBACKS k_ConnCallbacks;
    static Session* s_ActiveSession;
};
