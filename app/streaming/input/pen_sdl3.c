//
// SDL3 pen event capture for Moonlight running under sdl2-compat.
//

#include "pen_sdl3.h"

#include <Limelight.h>

#include <SDL3/SDL.h>

#include <dlfcn.h>
#include <math.h>
#include <string.h>

typedef void (SDLCALL *SDL3_SetEventFilter_t)(SDL_EventFilter filter, void *userdata);
typedef bool (SDLCALL *SDL3_GetEventFilter_t)(SDL_EventFilter *filter, void **userdata);
typedef bool (SDLCALL *SDL3_SetHint_t)(const char *name, const char *value);
typedef bool (SDLCALL *SDL3_SetHintWithPriority_t)(const char *name, const char *value, SDL_HintPriority priority);

typedef struct PenState {
    float x;
    float y;
    float pressure;
    float distance;
    float xtilt;
    float ytilt;
    float rotation;
    bool tipDown;
    bool eraser;
    bool inProximity;
    uint8_t buttons;
} PenState;

static void *g_sdl3;
static SDL3_SetEventFilter_t g_setEventFilter;
static SDL3_GetEventFilter_t g_getEventFilter;
static SDL3_SetHint_t g_setHint;
static SDL3_SetHintWithPriority_t g_setHintWithPriority;

static SDL_EventFilter g_chainedFilter;
static void *g_chainedUserdata;
static bool g_filterInstalled;

static MoonlightPenHandler g_handler;
static void *g_handlerUserdata;

// Track a handful of concurrent pens (built-in display pens are usually one).
#define MAX_PEN_STATE 8
static SDL_PenID g_penIds[MAX_PEN_STATE];
static PenState g_penState[MAX_PEN_STATE];

static bool loadSdl3(void)
{
    if (g_sdl3) {
        return g_setEventFilter && g_getEventFilter;
    }

    g_sdl3 = dlopen("libSDL3.so.0", RTLD_NOLOAD | RTLD_LAZY);
    if (!g_sdl3) {
        g_sdl3 = dlopen("libSDL3.so.0", RTLD_LAZY | RTLD_LOCAL);
    }
    if (!g_sdl3) {
        return false;
    }

    g_setEventFilter = (SDL3_SetEventFilter_t)dlsym(g_sdl3, "SDL_SetEventFilter");
    g_getEventFilter = (SDL3_GetEventFilter_t)dlsym(g_sdl3, "SDL_GetEventFilter");
    g_setHint = (SDL3_SetHint_t)dlsym(g_sdl3, "SDL_SetHint");
    g_setHintWithPriority = (SDL3_SetHintWithPriority_t)dlsym(g_sdl3, "SDL_SetHintWithPriority");

    return g_setEventFilter && g_getEventFilter;
}

static PenState *getPenState(SDL_PenID which, bool create)
{
    for (int i = 0; i < MAX_PEN_STATE; i++) {
        if (g_penIds[i] == which) {
            return &g_penState[i];
        }
    }

    if (!create) {
        return NULL;
    }

    for (int i = 0; i < MAX_PEN_STATE; i++) {
        if (g_penIds[i] == 0) {
            g_penIds[i] = which;
            memset(&g_penState[i], 0, sizeof(g_penState[i]));
            g_penState[i].pressure = 0.0f;
            g_penState[i].distance = 1.0f;
            return &g_penState[i];
        }
    }

    return NULL;
}

static void clearPenState(SDL_PenID which)
{
    for (int i = 0; i < MAX_PEN_STATE; i++) {
        if (g_penIds[i] == which) {
            g_penIds[i] = 0;
            memset(&g_penState[i], 0, sizeof(g_penState[i]));
            return;
        }
    }
}

static uint8_t mapPenButtons(SDL_PenInputFlags flags)
{
    uint8_t buttons = 0;
    if (flags & SDL_PEN_INPUT_BUTTON_1) {
        buttons |= LI_PEN_BUTTON_PRIMARY;
    }
    if (flags & SDL_PEN_INPUT_BUTTON_2) {
        buttons |= LI_PEN_BUTTON_SECONDARY;
    }
    if (flags & SDL_PEN_INPUT_BUTTON_3) {
        buttons |= LI_PEN_BUTTON_TERTIARY;
    }
    return buttons;
}

static unsigned char computeTilt(const PenState *state)
{
    if (state->xtilt == 0.0f && state->ytilt == 0.0f) {
        // Unknown vs truly vertical is indistinguishable; report vertical.
        return 0;
    }

    // Convert X/Y tilt (degrees from vertical per axis) into a single polar tilt.
    float xr = state->xtilt * (float)M_PI / 180.0f;
    float yr = state->ytilt * (float)M_PI / 180.0f;
    float tilt = atan2f(sqrtf(tanf(xr) * tanf(xr) + tanf(yr) * tanf(yr)), 1.0f) * 180.0f / (float)M_PI;
    if (tilt < 0.0f) {
        tilt = 0.0f;
    }
    if (tilt > 90.0f) {
        tilt = 90.0f;
    }
    return (unsigned char)(tilt + 0.5f);
}

static void emitPen(const PenState *state, unsigned char eventType)
{
    if (!g_handler || !state) {
        return;
    }

    MoonlightPenRawEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.eventType = eventType;
    ev.toolType = state->eraser ? LI_TOOL_TYPE_ERASER : LI_TOOL_TYPE_PEN;
    ev.penButtons = state->buttons;
    ev.x = state->x;
    ev.y = state->y;
    if (state->tipDown) {
        ev.pressure = state->pressure;
    } else {
        // Sunshine uses pressureOrDistance; while hovering, send distance.
        ev.pressure = state->distance;
    }
    ev.distance = state->distance;
    ev.rotationDegrees = state->rotation;
    ev.tilt = computeTilt(state);

    g_handler(&ev, g_handlerUserdata);
}

static void applyAxis(PenState *state, SDL_PenAxis axis, float value)
{
    switch (axis) {
    case SDL_PEN_AXIS_PRESSURE:
        state->pressure = value;
        break;
    case SDL_PEN_AXIS_DISTANCE:
        state->distance = value;
        break;
    case SDL_PEN_AXIS_XTILT:
        state->xtilt = value;
        break;
    case SDL_PEN_AXIS_YTILT:
        state->ytilt = value;
        break;
    case SDL_PEN_AXIS_ROTATION:
        state->rotation = value;
        break;
    default:
        break;
    }
}

static bool handlePenEvent(const SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_PEN_PROXIMITY_IN: {
        PenState *state = getPenState(event->pproximity.which, true);
        if (!state) {
            return true;
        }
        // Wait for motion/axis for coordinates before sending HOVER.
        state->inProximity = true;
        state->eraser = (event->pproximity.pen_state & SDL_PEN_INPUT_ERASER_TIP) != 0;
        state->buttons = mapPenButtons(event->pproximity.pen_state);
        return true;
    }
    case SDL_EVENT_PEN_PROXIMITY_OUT: {
        PenState *state = getPenState(event->pproximity.which, false);
        if (state) {
            emitPen(state, LI_TOUCH_EVENT_HOVER_LEAVE);
            clearPenState(event->pproximity.which);
        }
        return true;
    }
    case SDL_EVENT_PEN_DOWN: {
        PenState *state = getPenState(event->ptouch.which, true);
        if (!state) {
            return true;
        }
        state->inProximity = true;
        state->tipDown = true;
        state->eraser = event->ptouch.eraser || ((event->ptouch.pen_state & SDL_PEN_INPUT_ERASER_TIP) != 0);
        state->buttons = mapPenButtons(event->ptouch.pen_state);
        state->x = event->ptouch.x;
        state->y = event->ptouch.y;
        emitPen(state, LI_TOUCH_EVENT_DOWN);
        return true;
    }
    case SDL_EVENT_PEN_UP: {
        PenState *state = getPenState(event->ptouch.which, true);
        if (!state) {
            return true;
        }
        state->tipDown = false;
        state->eraser = event->ptouch.eraser || ((event->ptouch.pen_state & SDL_PEN_INPUT_ERASER_TIP) != 0);
        state->buttons = mapPenButtons(event->ptouch.pen_state);
        state->x = event->ptouch.x;
        state->y = event->ptouch.y;
        emitPen(state, LI_TOUCH_EVENT_UP);
        return true;
    }
    case SDL_EVENT_PEN_MOTION: {
        PenState *state = getPenState(event->pmotion.which, true);
        if (!state) {
            return true;
        }
        state->inProximity = true;
        state->tipDown = (event->pmotion.pen_state & SDL_PEN_INPUT_DOWN) != 0;
        state->eraser = (event->pmotion.pen_state & SDL_PEN_INPUT_ERASER_TIP) != 0;
        state->buttons = mapPenButtons(event->pmotion.pen_state);
        state->x = event->pmotion.x;
        state->y = event->pmotion.y;
        emitPen(state, state->tipDown ? LI_TOUCH_EVENT_MOVE : LI_TOUCH_EVENT_HOVER);
        return true;
    }
    case SDL_EVENT_PEN_AXIS: {
        PenState *state = getPenState(event->paxis.which, true);
        if (!state) {
            return true;
        }
        state->inProximity = true;
        state->tipDown = (event->paxis.pen_state & SDL_PEN_INPUT_DOWN) != 0;
        state->eraser = (event->paxis.pen_state & SDL_PEN_INPUT_ERASER_TIP) != 0;
        state->buttons = mapPenButtons(event->paxis.pen_state);
        state->x = event->paxis.x;
        state->y = event->paxis.y;
        applyAxis(state, event->paxis.axis, event->paxis.value);
        emitPen(state, state->tipDown ? LI_TOUCH_EVENT_MOVE : LI_TOUCH_EVENT_HOVER);
        return true;
    }
    case SDL_EVENT_PEN_BUTTON_DOWN:
    case SDL_EVENT_PEN_BUTTON_UP: {
        PenState *state = getPenState(event->pbutton.which, true);
        if (!state) {
            return true;
        }
        state->inProximity = true;
        state->tipDown = (event->pbutton.pen_state & SDL_PEN_INPUT_DOWN) != 0;
        state->eraser = (event->pbutton.pen_state & SDL_PEN_INPUT_ERASER_TIP) != 0;
        state->buttons = mapPenButtons(event->pbutton.pen_state);
        state->x = event->pbutton.x;
        state->y = event->pbutton.y;
        emitPen(state, LI_TOUCH_EVENT_BUTTON_ONLY);
        return true;
    }
    default:
        return false;
    }
}

static bool SDLCALL penAwareFilter(void *userdata, SDL_Event *event)
{
    (void)userdata;

    if (handlePenEvent(event)) {
        // Consume pen events so sdl2-compat does not need to (and cannot) map them.
        return false;
    }

    if (g_chainedFilter) {
        return g_chainedFilter(g_chainedUserdata, event);
    }

    return true;
}

static void installHints(void)
{
    // Prefer native pen packets over synthetic mouse/touch.
    if (g_setHintWithPriority) {
        g_setHintWithPriority(SDL_HINT_PEN_MOUSE_EVENTS, "0", SDL_HINT_OVERRIDE);
        g_setHintWithPriority(SDL_HINT_PEN_TOUCH_EVENTS, "0", SDL_HINT_OVERRIDE);
    } else if (g_setHint) {
        g_setHint(SDL_HINT_PEN_MOUSE_EVENTS, "0");
        g_setHint(SDL_HINT_PEN_TOUCH_EVENTS, "0");
    }
}

bool Moonlight_Pen_Start(MoonlightPenHandler handler, void *userdata)
{
    if (!loadSdl3()) {
        return false;
    }

    if (handler) {
        g_handler = handler;
        g_handlerUserdata = userdata;
    }

    installHints();
    Moonlight_Pen_EnsureFilter();
    return g_filterInstalled;
}

void Moonlight_Pen_EnsureFilter(void)
{
    if (!loadSdl3() || !g_handler) {
        return;
    }

    SDL_EventFilter current = NULL;
    void *userdata = NULL;
    if (!g_getEventFilter(&current, &userdata)) {
        current = NULL;
        userdata = NULL;
    }

    if (current == penAwareFilter) {
        g_filterInstalled = true;
        return;
    }

    // sdl2-compat periodically reinstalls its own filter; wrap whatever is active.
    g_chainedFilter = current;
    g_chainedUserdata = userdata;
    g_setEventFilter(penAwareFilter, NULL);
    g_filterInstalled = true;
    installHints();
}

void Moonlight_Pen_Stop(void)
{
    if (g_filterInstalled && g_setEventFilter) {
        g_setEventFilter(g_chainedFilter, g_chainedUserdata);
    }

    g_filterInstalled = false;
    g_chainedFilter = NULL;
    g_chainedUserdata = NULL;
    g_handler = NULL;
    g_handlerUserdata = NULL;
    memset(g_penIds, 0, sizeof(g_penIds));
    memset(g_penState, 0, sizeof(g_penState));
}
