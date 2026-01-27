#include "x11tablet.h"

#include "streaming/streamutils.h"

#include <Limelight.h>
#include <SDL.h>
#include <SDL_syswm.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

#if defined(HAS_X11) && defined(HAVE_XI)
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#endif

struct X11TabletManager::DeviceState {
    int deviceId = -1;
    bool isEraser = false;
    bool inContact = false;
    uint8_t penButtons = 0;

    int xIndex = -1;
    int yIndex = -1;
    int pressureIndex = -1;
    int tiltXIndex = -1;
    int tiltYIndex = -1;
    int rotationIndex = -1;

    double xMin = 0.0;
    double xMax = 0.0;
    double yMin = 0.0;
    double yMax = 0.0;
    double pressureMin = 0.0;
    double pressureMax = 0.0;
    double tiltXMin = 0.0;
    double tiltXMax = 0.0;
    double tiltYMin = 0.0;
    double tiltYMax = 0.0;
    double rotationMin = 0.0;
    double rotationMax = 0.0;

    float lastPressure = 0.0f;
    float lastTilt = 0.0f;
    uint16_t lastRotation = 0;
    bool tiltSeen = false;
    bool rotationSeen = false;
};

namespace {
bool penDebugEnabled() {
    static int cached = -1;
    if (cached == -1) {
        const char* env = SDL_getenv("MOONLIGHT_PEN_DEBUG");
        cached = (env && *env) ? 1 : 0;
    }
    return cached == 1;
}
#if defined(HAS_X11) && defined(HAVE_XI)
struct X11TabletState {
    Display* display = nullptr;
    Window window = 0;
    int xiOpcode = -1;
    bool xiReady = false;
    Atom absX = None;
    Atom absY = None;
    Atom absPressure = None;
    Atom absTiltX = None;
    Atom absTiltY = None;
    Atom absRotation = None;
    std::unordered_map<int, X11TabletManager::DeviceState> devices;
};

bool nameContains(const char* name, const char* needle) {
    if (!name || !needle) {
        return false;
    }
    size_t needleLen = SDL_strlen(needle);
    if (needleLen == 0) {
        return false;
    }

    for (const char* hay = name; *hay; ++hay) {
        size_t i = 0;
        while (i < needleLen) {
            char hc = hay[i];
            if (!hc) {
                break;
            }
            if (SDL_tolower(static_cast<unsigned char>(hc)) !=
                SDL_tolower(static_cast<unsigned char>(needle[i]))) {
                break;
            }
            ++i;
        }
        if (i == needleLen) {
            return true;
        }
    }
    return false;
}

bool nameSuggestsStylus(const char* name) {
    return nameContains(name, "pen") ||
           nameContains(name, "stylus") ||
           nameContains(name, "tablet") ||
           nameContains(name, "eraser");
}

bool nameSuggestsPointer(const char* name) {
    return nameContains(name, "mouse") ||
           nameContains(name, "touchpad") ||
           nameContains(name, "trackpad");
}

uint8_t mapButtonToPen(uint32_t detail) {
    switch (detail) {
    case 2:
        return LI_PEN_BUTTON_SECONDARY;
    case 3:
        return LI_PEN_BUTTON_TERTIARY;
    case 4:
        return LI_PEN_BUTTON_PRIMARY;
    default:
        return 0;
    }
}

uint8_t mapPenButtons(uint8_t current, uint32_t detail, bool pressed) {
    uint8_t mask = mapButtonToPen(detail);
    if (mask == 0) {
        return current;
    }
    if (pressed) {
        return current | mask;
    }
    return current & ~mask;
}
#endif
} // namespace

X11TabletManager::X11TabletManager()
    : m_Initialized(false),
      m_HasTabletDevices(false),
      m_Window(nullptr),
      m_StreamWidth(0),
      m_StreamHeight(0)
{
}

X11TabletManager::~X11TabletManager()
{
    shutdown();
}

bool X11TabletManager::initialize(SDL_Window* window, int streamWidth, int streamHeight)
{
    m_Window = window;
    m_StreamWidth = streamWidth;
    m_StreamHeight = streamHeight;

#if defined(HAS_X11) && defined(HAVE_XI)
    if (!window) {
        return false;
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        return false;
    }

    if (info.subsystem != SDL_SYSWM_X11) {
        return false;
    }

    X11TabletState* state = new X11TabletState();
    state->display = info.info.x11.display;
    state->window = info.info.x11.window;

    m_Initialized = false;
    m_HasTabletDevices = false;

    if (!state->display || !state->window) {
        delete state;
        return false;
    }

    m_Initialized = true;

    // Store state pointer in the window's data via SDL
    SDL_SetWindowData(window, "moonlight.x11tablet", state);

    if (!setupXInput2()) {
        shutdown();
        return false;
    }

    if (penDebugEnabled()) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "X11 stylus passthrough initialized");
    }
    return true;
#else
    (void)window;
    (void)streamWidth;
    (void)streamHeight;
    return false;
#endif
}

void X11TabletManager::shutdown()
{
#if defined(HAS_X11) && defined(HAVE_XI)
    if (m_Window) {
        auto* state = reinterpret_cast<X11TabletState*>(SDL_GetWindowData(m_Window, "moonlight.x11tablet"));
        if (state) {
            delete state;
        }
        SDL_SetWindowData(m_Window, "moonlight.x11tablet", nullptr);
    }
#endif
    m_Initialized = false;
    m_HasTabletDevices = false;
    m_Window = nullptr;
}

void X11TabletManager::setStreamDimensions(int streamWidth, int streamHeight)
{
    m_StreamWidth = streamWidth;
    m_StreamHeight = streamHeight;
}

bool X11TabletManager::isActive() const
{
    return m_Initialized;
}

bool X11TabletManager::wantsPolling() const
{
    return m_Initialized;
}

bool X11TabletManager::setupXInput2()
{
#if defined(HAS_X11) && defined(HAVE_XI)
    if (!m_Window) {
        return false;
    }

    auto* state = reinterpret_cast<X11TabletState*>(SDL_GetWindowData(m_Window, "moonlight.x11tablet"));
    if (!state || !state->display) {
        return false;
    }

    int event = 0;
    int error = 0;
    if (!XQueryExtension(state->display, "XInputExtension", &state->xiOpcode, &event, &error)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "XInput2 not available");
        return false;
    }

    int major = 2;
    int minor = 0;
    if (XIQueryVersion(state->display, &major, &minor) != Success) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "XInput2 query failed");
        return false;
    }
    if (major < 2) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "XInput2 version %d.%d is too old",
                    major, minor);
        return false;
    }

    state->xiReady = true;

    if (penDebugEnabled()) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "XInput2 version negotiated: %d.%d",
                    major, minor);
    }

    state->absX = XInternAtom(state->display, "Abs X", False);
    state->absY = XInternAtom(state->display, "Abs Y", False);
    state->absPressure = XInternAtom(state->display, "Abs Pressure", False);
    state->absTiltX = XInternAtom(state->display, "Abs Tilt X", False);
    state->absTiltY = XInternAtom(state->display, "Abs Tilt Y", False);
    state->absRotation = XInternAtom(state->display, "Abs Rotation", False);

    unsigned char maskData[XIMaskLen(XI_LASTEVENT)] = {};
    XIEventMask mask;
    mask.deviceid = XIAllDevices;
    mask.mask_len = sizeof(maskData);
    mask.mask = maskData;
    XISetMask(mask.mask, XI_Motion);
    XISetMask(mask.mask, XI_ButtonPress);
    XISetMask(mask.mask, XI_ButtonRelease);
    XISetMask(mask.mask, XI_DeviceChanged);
    XISetMask(mask.mask, XI_HierarchyChanged);

    XISelectEvents(state->display, state->window, &mask, 1);
    XFlush(state->display);

    refreshDevices();
    return true;
#else
    return false;
#endif
}

void X11TabletManager::refreshDevices()
{
#if defined(HAS_X11) && defined(HAVE_XI)
    auto* state = reinterpret_cast<X11TabletState*>(SDL_GetWindowData(m_Window, "moonlight.x11tablet"));
    if (!state || !state->display || !state->xiReady) {
        return;
    }

    state->devices.clear();

    int ndevices = 0;
    XIDeviceInfo* devices = XIQueryDevice(state->display, XIAllDevices, &ndevices);
    if (!devices) {
        return;
    }

    for (int i = 0; i < ndevices; ++i) {
        XIDeviceInfo* dev = &devices[i];
        if (dev->use != XISlavePointer && dev->use != XIFloatingSlave) {
            continue;
        }

        DeviceState device;
        device.deviceId = dev->deviceid;
        device.isEraser = nameContains(dev->name, "eraser");
        bool nameStylus = nameSuggestsStylus(dev->name);
        bool namePointer = nameSuggestsPointer(dev->name);
        bool hasTouchClass = false;

        for (int c = 0; c < dev->num_classes; ++c) {
            XIAnyClassInfo* classInfo = dev->classes[c];
            if (classInfo->type == XITouchClass) {
                hasTouchClass = true;
                continue;
            }

            if (classInfo->type != XIValuatorClass) {
                continue;
            }

            auto* v = reinterpret_cast<XIValuatorClassInfo*>(classInfo);
            if (v->label == state->absX) {
                device.xIndex = v->number;
                device.xMin = v->min;
                device.xMax = v->max;
            } else if (v->label == state->absY) {
                device.yIndex = v->number;
                device.yMin = v->min;
                device.yMax = v->max;
            } else if (v->label == state->absPressure) {
                device.pressureIndex = v->number;
                device.pressureMin = v->min;
                device.pressureMax = v->max;
            } else if (v->label == state->absTiltX) {
                device.tiltXIndex = v->number;
                device.tiltXMin = v->min;
                device.tiltXMax = v->max;
            } else if (v->label == state->absTiltY) {
                device.tiltYIndex = v->number;
                device.tiltYMin = v->min;
                device.tiltYMax = v->max;
            } else if (v->label == state->absRotation) {
                device.rotationIndex = v->number;
                device.rotationMin = v->min;
                device.rotationMax = v->max;
            }
        }

        bool hasXY = device.xIndex >= 0 && device.yIndex >= 0;
        bool hasPressure = device.pressureIndex >= 0;
        bool hasTilt = device.tiltXIndex >= 0 || device.tiltYIndex >= 0;
        bool hasRotation = device.rotationIndex >= 0;
        bool hasStylusValuators = hasPressure || hasTilt || hasRotation;

        if (!hasXY) {
            continue;
        }

        if (!hasStylusValuators && !nameStylus) {
            continue;
        }

        if (hasTouchClass && !hasStylusValuators && !nameStylus) {
            continue;
        }

        if (namePointer && !nameStylus && !hasStylusValuators) {
            continue;
        }

        if (hasXY && (hasStylusValuators || nameStylus)) {
            state->devices.emplace(device.deviceId, device);
            if (penDebugEnabled()) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "X11 tablet device detected: id=%d name=\"%s\" x=%d y=%d pressure=%d tiltX=%d tiltY=%d rotation=%d",
                            device.deviceId,
                            dev->name ? dev->name : "(null)",
                            device.xIndex,
                            device.yIndex,
                            device.pressureIndex,
                            device.tiltXIndex,
                            device.tiltYIndex,
                            device.rotationIndex);
            }
        }
    }

    XIFreeDeviceInfo(devices);
    m_HasTabletDevices = !state->devices.empty();
#endif
}

bool X11TabletManager::normalizeWindowCoords(double x, double y, float* outX, float* outY)
{
    if (!m_Window || m_StreamWidth <= 0 || m_StreamHeight <= 0) {
        return false;
    }

    int windowWidth = 0;
    int windowHeight = 0;
    const char* sizeBasis = nullptr;
#if SDL_VERSION_ATLEAST(2, 26, 0)
    SDL_GetWindowSizeInPixels(m_Window, &windowWidth, &windowHeight);
    if (windowWidth > 0 && windowHeight > 0) {
        sizeBasis = "SDL_GetWindowSizeInPixels";
    }
#endif
    if (windowWidth <= 0 || windowHeight <= 0) {
        SDL_GL_GetDrawableSize(m_Window, &windowWidth, &windowHeight);
        if (windowWidth > 0 && windowHeight > 0) {
            sizeBasis = "SDL_GL_GetDrawableSize";
        }
    }
    if (windowWidth <= 0 || windowHeight <= 0) {
        SDL_GetWindowSize(m_Window, &windowWidth, &windowHeight);
        if (windowWidth > 0 && windowHeight > 0) {
            sizeBasis = "SDL_GetWindowSize";
        }
    }
    if (windowWidth <= 0 || windowHeight <= 0) {
        return false;
    }

    // XInput2 event coordinates are in window space for the target window on X11.
    // Prefer pixel-sized window dimensions when available to match XI2 coordinate space.
    if (penDebugEnabled()) {
        static bool logged = false;
        if (!logged) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "X11 tablet window size basis: %s (%dx%d)",
                        sizeBasis ? sizeBasis : "unknown",
                        windowWidth, windowHeight);
            logged = true;
        }
    }
    return StreamUtils::windowPointToNormalizedVideo(m_StreamWidth, m_StreamHeight,
                                                     windowWidth, windowHeight,
                                                     static_cast<float>(x), static_cast<float>(y),
                                                     outX, outY);
}

bool X11TabletManager::readValuator(void* eventData, int index, double* value) const
{
#if defined(HAS_X11) && defined(HAVE_XI)
    if (index < 0 || !eventData) {
        return false;
    }
    auto* ev = reinterpret_cast<XIDeviceEvent*>(eventData);
    if (!XIMaskIsSet(ev->valuators.mask, index)) {
        return false;
    }

    int offset = 0;
    for (int i = 0; i < index; ++i) {
        if (XIMaskIsSet(ev->valuators.mask, i)) {
            offset++;
        }
    }
    *value = ev->valuators.values[offset];
    return true;
#else
    (void)eventData;
    (void)index;
    (void)value;
    return false;
#endif
}

float X11TabletManager::normalizeValuator(double value, double min, double max) const
{
    if (max <= min) {
        return 0.0f;
    }
    double norm = (value - min) / (max - min);
    norm = std::min(std::max(norm, 0.0), 1.0);
    return static_cast<float>(norm);
}

void X11TabletManager::handleDeviceEvent(void* eventData, int eventType, const std::function<void(const StylusEvent&)>& handler)
{
#if defined(HAS_X11) && defined(HAVE_XI)
    auto* ev = reinterpret_cast<XIDeviceEvent*>(eventData);
    if (!ev || !handler) {
        return;
    }

    auto* state = reinterpret_cast<X11TabletState*>(SDL_GetWindowData(m_Window, "moonlight.x11tablet"));
    if (!state) {
        return;
    }

    auto it = state->devices.find(ev->deviceid);
    if (it == state->devices.end()) {
        return;
    }

    DeviceState& dev = it->second;

    bool isButtonPress = eventType == XI_ButtonPress;
    bool isButtonRelease = eventType == XI_ButtonRelease;
    bool isMotion = eventType == XI_Motion;
    bool contactEnded = false;

    uint32_t detail = ev->detail;

    if (isButtonPress || isButtonRelease) {
        if (detail == 1) {
            dev.inContact = isButtonPress;
            contactEnded = isButtonRelease;
        } else {
            dev.penButtons = mapPenButtons(dev.penButtons, detail, isButtonPress);
        }
    }

    uint8_t liEvent = LI_TOUCH_EVENT_MOVE;
    if (isMotion) {
        liEvent = dev.inContact ? LI_TOUCH_EVENT_MOVE : LI_TOUCH_EVENT_HOVER;
    } else if (detail == 1) {
        liEvent = isButtonPress ? LI_TOUCH_EVENT_DOWN : LI_TOUCH_EVENT_UP;
    } else {
        liEvent = dev.inContact ? LI_TOUCH_EVENT_MOVE : LI_TOUCH_EVENT_HOVER;
    }

    float normX = 0.0f;
    float normY = 0.0f;
    if (!normalizeWindowCoords(ev->event_x, ev->event_y, &normX, &normY)) {
        return;
    }

    float pressure = 0.0f;
    if (dev.pressureIndex >= 0) {
        double val = 0.0;
        bool hasSample = readValuator(eventData, dev.pressureIndex, &val);
        if (dev.inContact) {
            if (hasSample) {
                dev.lastPressure = normalizeValuator(val, dev.pressureMin, dev.pressureMax);
                pressure = dev.lastPressure;
            } else if (liEvent == LI_TOUCH_EVENT_MOVE) {
                pressure = dev.lastPressure;
            }
        } else {
            pressure = 0.0f;
        }
    } else {
        pressure = dev.inContact ? 1.0f : 0.0f;
    }
    if (liEvent == LI_TOUCH_EVENT_UP || liEvent == LI_TOUCH_EVENT_HOVER) {
        pressure = 0.0f;
    }
    if (!dev.inContact || contactEnded) {
        dev.lastPressure = 0.0f;
    }

    uint8_t tilt = LI_TILT_UNKNOWN;
    if (dev.tiltXIndex >= 0 || dev.tiltYIndex >= 0) {
        double tiltX = 0.0;
        double tiltY = 0.0;
        bool gotX = readValuator(eventData, dev.tiltXIndex, &tiltX);
        bool gotY = readValuator(eventData, dev.tiltYIndex, &tiltY);
        if (gotX || gotY) {
            double maxX = std::max(std::abs(dev.tiltXMin), std::abs(dev.tiltXMax));
            double maxY = std::max(std::abs(dev.tiltYMin), std::abs(dev.tiltYMax));
            double maxRange = std::max(maxX, maxY);
            if (maxRange > 0.0) {
                double magnitude = std::sqrt(tiltX * tiltX + tiltY * tiltY);
                double normalized = std::min(magnitude / maxRange, 1.0);
                dev.lastTilt = static_cast<float>(normalized * 90.0);
                dev.tiltSeen = true;
            }
        }
        if (dev.tiltSeen) {
            tilt = static_cast<uint8_t>(std::min(std::max(dev.lastTilt, 0.0f), 90.0f));
        }
    }

    uint16_t rotation = LI_ROT_UNKNOWN;
    if (dev.rotationIndex >= 0) {
        double rot = 0.0;
        if (readValuator(eventData, dev.rotationIndex, &rot)) {
            float norm = normalizeValuator(rot, dev.rotationMin, dev.rotationMax);
            dev.lastRotation = static_cast<uint16_t>(norm * 360.0f);
            dev.rotationSeen = true;
        }
        if (dev.rotationSeen) {
            rotation = dev.lastRotation;
        }
    }

    StylusEvent out = {};
    out.eventType = liEvent;
    out.toolType = dev.isEraser ? LI_TOOL_TYPE_ERASER : LI_TOOL_TYPE_PEN;
    out.penButtons = dev.penButtons;
    out.x = normX;
    out.y = normY;
    out.pressure = pressure;
    out.contactMajor = 0.0f;
    out.contactMinor = 0.0f;
    out.rotation = rotation;
    out.tilt = tilt;
    handler(out);
#else
    (void)eventData;
    (void)eventType;
    (void)handler;
#endif
}

void X11TabletManager::pumpEvents(const std::function<void(const StylusEvent&)>& handler)
{
#if defined(HAS_X11) && defined(HAVE_XI)
    if (!m_Initialized || !m_Window) {
        return;
    }

    auto* state = reinterpret_cast<X11TabletState*>(SDL_GetWindowData(m_Window, "moonlight.x11tablet"));
    if (!state || !state->display || !state->xiReady) {
        return;
    }

    auto predicate = [](Display* display, XEvent* event, XPointer arg) -> Bool {
        auto* self = reinterpret_cast<X11TabletManager*>(arg);
        if (!self || event->type != GenericEvent) {
            return False;
        }

        auto* state = reinterpret_cast<X11TabletState*>(SDL_GetWindowData(self->m_Window, "moonlight.x11tablet"));
        if (!state || event->xcookie.extension != state->xiOpcode) {
            return False;
        }

        int evtype = event->xcookie.evtype;
        if (evtype == XI_DeviceChanged || evtype == XI_HierarchyChanged) {
            return True;
        }

        if (evtype != XI_Motion && evtype != XI_ButtonPress && evtype != XI_ButtonRelease) {
            return False;
        }

        if (!XGetEventData(display, &event->xcookie)) {
            return False;
        }

        bool match = false;
        if (event->xcookie.data) {
            auto* dev = reinterpret_cast<XIDeviceEvent*>(event->xcookie.data);
            match = state->devices.find(dev->deviceid) != state->devices.end();
        }
        XFreeEventData(display, &event->xcookie);
        return match ? True : False;
    };

    XEvent ev;
    while (XCheckIfEvent(state->display, &ev, predicate, reinterpret_cast<XPointer>(this))) {
        if (ev.type != GenericEvent || ev.xcookie.extension != state->xiOpcode) {
            continue;
        }

        if (!XGetEventData(state->display, &ev.xcookie)) {
            continue;
        }

        switch (ev.xcookie.evtype) {
        case XI_DeviceChanged:
        case XI_HierarchyChanged:
            refreshDevices();
            break;
        case XI_Motion:
        case XI_ButtonPress:
        case XI_ButtonRelease:
            handleDeviceEvent(ev.xcookie.data, ev.xcookie.evtype, handler);
            break;
        }

        XFreeEventData(state->display, &ev.xcookie);
    }
#else
    (void)handler;
#endif
}
