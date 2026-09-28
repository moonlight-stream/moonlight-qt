//
// Small Moonlight-specific helpers around native SDL 3 APIs.
// Include this instead of SDL headers directly.
//

#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void* SDLC_Win32_GetHwnd(SDL_Window* window);
void* SDLC_MacOS_GetWindow(SDL_Window* window);
void* SDLC_X11_GetDisplay(SDL_Window* window);
unsigned long SDLC_X11_GetWindow(SDL_Window* window);
void* SDLC_Wayland_GetDisplay(SDL_Window* window);
void* SDLC_Wayland_GetSurface(SDL_Window* window);
int SDLC_KMSDRM_GetFd(SDL_Window* window);
int SDLC_KMSDRM_GetDevIndex(SDL_Window* window);

typedef enum {
    SDLC_VIDEO_UNKNOWN,
    SDLC_VIDEO_WIN32,
    SDLC_VIDEO_MACOS,
    SDLC_VIDEO_X11,
    SDLC_VIDEO_WAYLAND,
    SDLC_VIDEO_KMSDRM,
} SDLC_VideoDriver;

SDLC_VideoDriver SDLC_GetVideoDriver(void);

bool SDLC_IsFullscreen(SDL_Window* window);
bool SDLC_IsFullscreenExclusive(SDL_Window* window);
bool SDLC_IsFullscreenDesktop(SDL_Window* window);
void SDLC_EnterFullscreen(SDL_Window* window, bool exclusive);
void SDLC_LeaveFullscreen(SDL_Window* window);

SDL_Window* SDLC_CreateWindowWithFallback(const char* title,
                                          int x, int y, int w, int h,
                                          SDL_WindowFlags requiredFlags,
                                          SDL_WindowFlags optionalFlags);

void SDLC_FlushWindowEvents(void);
void SDLC_SetCursorVisible(bool visible);
int SDLC_GetDisplayCount(void);

#define SDLC_SUCCESS(x) (x)
#define SDLC_FAILURE(x) (!(x))

#define KEY_DOWN(x) ((x)->down)
#define KEY_KEY(x) ((x)->key)
#define KEY_MOD(x) ((x)->mod)
#define KEY_SCANCODE(x) ((x)->scancode)

#define SDLC_DEFAULT_RENDER_DRIVER NULL

#ifdef __cplusplus
}
#endif
