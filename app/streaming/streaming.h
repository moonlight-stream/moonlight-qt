#pragma once

#include <Limelight.h>
#include <SDL.h>
#include <QMessageBox>

#ifdef __cplusplus
extern "C" {
#endif

extern AUDIO_RENDERER_CALLBACKS k_AudioCallbacks;
extern DECODER_RENDERER_CALLBACKS k_VideoCallbacks;

int SdlDetermineAudioConfiguration(void);

void SdlInitializeGamepad(bool multiController);
int SdlGetAttachedGamepadMask(void);
void SdlHandleControllerDeviceEvent(SDL_ControllerDeviceEvent* event);
void SdlHandleControllerButtonEvent(SDL_ControllerButtonEvent* event);
void SdlHandleControllerAxisEvent(SDL_ControllerAxisEvent* event);
void SdlHandleMouseWheelEvent(SDL_MouseWheelEvent* event);
void SdlHandleMouseMotionEvent(SDL_MouseMotionEvent* event);
void SdlHandleMouseButtonEvent(SDL_MouseButtonEvent* event);
void SdlHandleKeyEvent(SDL_KeyboardEvent* event);

#ifdef __cplusplus
}
#endif

// This function uses C++ linkage
int StartConnection(PSERVER_INFORMATION serverInfo, PSTREAM_CONFIGURATION streamConfig, QMessageBox* progressBox);
