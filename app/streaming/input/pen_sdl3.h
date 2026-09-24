//
// SDL3 pen event capture for Moonlight running under sdl2-compat.
//
// sdl2-compat drops SDL_EVENT_PEN_* before they reach the SDL2 event queue.
// This module wraps the native SDL3 event filter so those events can be
// forwarded to Sunshine via LiSendPenEvent().
//

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MoonlightPenRawEvent {
    // LI_TOUCH_EVENT_* values from Limelight.h
    unsigned char eventType;
    // LI_TOOL_TYPE_* values
    unsigned char toolType;
    // LI_PEN_BUTTON_* bitmask
    unsigned char penButtons;
    // Window-relative pixel coordinates from SDL3
    float x;
    float y;
    float pressure;
    float distance;
    float rotationDegrees;
    // Combined tilt from vertical in degrees (0..90), or 255 = unknown
    unsigned char tilt;
} MoonlightPenRawEvent;

typedef void (*MoonlightPenHandler)(const MoonlightPenRawEvent* event, void* userdata);

// Install the SDL3 filter hook and disable synthetic pen→mouse/touch.
// Safe to call multiple times; pass NULL handler to keep prior handler.
bool Moonlight_Pen_Start(MoonlightPenHandler handler, void* userdata);

// Re-wrap the SDL3 filter if sdl2-compat reset it. Call once per event-loop wake.
void Moonlight_Pen_EnsureFilter(void);

// Remove our filter wrap and clear handler state.
void Moonlight_Pen_Stop(void);

#ifdef __cplusplus
}
#endif
