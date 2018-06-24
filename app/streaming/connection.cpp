#include "streaming.h"
#include <Limelight.h>
#include <SDL.h>

#include <QMessageBox>

bool g_StreamActive;
QMessageBox* g_ProgressBox;

static
void
ClStageStarting(int stage)
{
    char buffer[512];
    sprintf(buffer, "Starting %s...", LiGetStageName(stage));
    g_ProgressBox->setText(buffer);
}

static
void
ClStageFailed(int stage, long errorCode)
{
    char buffer[512];
    sprintf(buffer, "Failed %s with error: %ld",
            LiGetStageName(stage), errorCode);
    g_ProgressBox->setText(buffer);
}

static
void
ClConnectionTerminated(long errorCode)
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

static
void
ClLogMessage(const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION,
                    SDL_LOG_PRIORITY_INFO,
                    format,
                    ap);
    va_end(ap);
}

int
StartConnection(PSERVER_INFORMATION serverInfo, PSTREAM_CONFIGURATION streamConfig, QMessageBox* progressBox)
{
    CONNECTION_LISTENER_CALLBACKS listener;

    // Hold onto this for our callbacks
    g_ProgressBox = progressBox;

    if (streamConfig->audioConfiguration < 0) {
        // This signals to us that we should auto-detect
        streamConfig->audioConfiguration = SdlDetermineAudioConfiguration();
    }

    LiInitializeConnectionCallbacks(&listener);
    listener.stageStarting = ClStageStarting;
    listener.stageFailed = ClStageFailed;
    listener.connectionTerminated = ClConnectionTerminated;
    listener.logMessage = ClLogMessage;

    int err = LiStartConnection(serverInfo, streamConfig, &listener,
                                &k_VideoCallbacks, &k_AudioCallbacks,
                                NULL, 0, NULL, 0);
    if (err != 0) {
        SDL_Quit();
        return err;
    }

    // Before we get into our loop, close the message box used to
    // display progress
    Q_ASSERT(g_ProgressBox == progressBox);
    progressBox->close();
    delete progressBox;
    g_ProgressBox = nullptr;

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
            SdlHandleKeyEvent(&event.key);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            SdlHandleMouseButtonEvent(&event.button);
            break;
        case SDL_MOUSEMOTION:
            SdlHandleMouseMotionEvent(&event.motion);
            break;
        case SDL_MOUSEWHEEL:
            SdlHandleMouseWheelEvent(&event.wheel);
            break;
        case SDL_CONTROLLERAXISMOTION:
            SdlHandleControllerAxisEvent(&event.caxis);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            SdlHandleControllerButtonEvent(&event.cbutton);
            break;
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
            SdlHandleControllerDeviceEvent(&event.cdevice);
            break;
        }
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "SDL_WaitEvent() failed: %s",
                 SDL_GetError());

Exit:
    g_StreamActive = false;
    LiStopConnection();

    return 0;
}
