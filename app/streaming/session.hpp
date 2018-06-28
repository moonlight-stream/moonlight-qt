#pragma once

#include <Limelight.h>
#include <opus_multistream.h>
#include "backend/computermanager.h"
#include "input.hpp"

#include <QMessageBox>


class Session
{
public:
    explicit Session(NvComputer* computer, NvApp& app);

    QString checkForFatalValidationError();

    QStringList checkForAdvisoryValidationError();

    void exec();

private:
    static
    void clStageStarting(int stage);

    static
    void clStageFailed(int stage, long errorCode);

    static
    void clConnectionTerminated(long errorCode);

    static
    void clLogMessage(const char* format, ...);

    static
    int sdlDetermineAudioConfiguration();

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

    STREAM_CONFIGURATION m_StreamConfig;
    QMessageBox m_ProgressBox;
    NvComputer* m_Computer;
    NvApp m_App;

    static SDL_AudioDeviceID s_AudioDevice;
    static OpusMSDecoder* s_OpusDecoder;
    static short s_OpusDecodeBuffer[];
    static int s_ChannelCount;

    static AUDIO_RENDERER_CALLBACKS k_AudioCallbacks;
    static DECODER_RENDERER_CALLBACKS k_VideoCallbacks;
    static CONNECTION_LISTENER_CALLBACKS k_ConnCallbacks;
    static Session* s_ActiveSession;
};
