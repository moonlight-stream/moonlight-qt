#include "sdlgamepadkeynavigation.h"

#include <QKeyEvent>
#include <QGuiApplication>
#include <QWindow>

#include "settings/mappingmanager.h"

#define AXIS_NAVIGATION_REPEAT_DELAY 150

SdlGamepadKeyNavigation::SdlGamepadKeyNavigation(StreamingPreferences* prefs)
    : m_Prefs(prefs),
      m_Enabled(false),
      m_UiNavMode(false),
      m_FirstPoll(false),
      m_HasFocus(false),
      m_LastAxisNavigationEventTime(0)
{
    m_PollingTimer = new QTimer(this);
    connect(m_PollingTimer, &QTimer::timeout, this, &SdlGamepadKeyNavigation::onPollingTimerFired);
}

SdlGamepadKeyNavigation::~SdlGamepadKeyNavigation()
{
    disable();
}

void SdlGamepadKeyNavigation::enable()
{
    if (m_Enabled) {
        return;
    }

    // We have to initialize and uninitialize this in enable()/disable()
    // because we need to get out of the way of the Session class. If it
    // doesn't get to reinitialize the GC subsystem, it won't get initial
    // arrival events. Additionally, there's a race condition between
    // our QML objects being destroyed and SDL being deinitialized that
    // this solves too.
    if (SDLC_FAILURE(SDL_InitSubSystem(SDL_INIT_GAMEPAD))) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) failed: %s",
                     SDL_GetError());
        return;
    }

    MappingManager mappingManager;
    mappingManager.applyMappings();

    // Drop all pending gamepad add events. SDL will generate these for us
    // on first init of the GC subsystem. We can't depend on them due to
    // overlapping lifetimes of SdlGamepadKeyNavigation instances, so we
    // will attach ourselves.
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_EVENT_GAMEPAD_ADDED);

    // Open all currently attached game controllers
    int numGamepads = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&numGamepads);
    for (int i = 0; i < numGamepads; i++) {
        SDL_Gamepad * gc = SDL_OpenGamepad(i);
        if (gc != nullptr) {
            m_Gamepads.append(gc);
        }
    }
    SDL_free(gamepads);

    m_Enabled = true;

    // Start the polling timer if the window is focused
    updateTimerState();
}

void SdlGamepadKeyNavigation::disable()
{
    if (!m_Enabled) {
        return;
    }

    m_Enabled = false;
    updateTimerState();
    Q_ASSERT(!m_PollingTimer->isActive());

    while (!m_Gamepads.isEmpty()) {
        SDL_CloseGamepad(m_Gamepads[0]);
        m_Gamepads.removeAt(0);
    }

    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

void SdlGamepadKeyNavigation::notifyWindowFocus(bool hasFocus)
{
    m_HasFocus = hasFocus;
    updateTimerState();
}

void SdlGamepadKeyNavigation::onPollingTimerFired()
{
    SDL_Event event;

    // Discard any pending button events on the first poll to avoid picking up
    // stale input data from the stream session (like the quit combo).
    if (m_FirstPoll) {
        SDL_PumpEvents();
        SDL_FlushEvent(SDL_EVENT_GAMEPAD_BUTTON_DOWN);
        SDL_FlushEvent(SDL_EVENT_GAMEPAD_BUTTON_UP);

        m_FirstPoll = false;
    }

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT :
            // SDL may send us a quit event since we initialize
            // the video subsystem on startup. If we get one,
            // forward it on for Qt to take care of.
            QCoreApplication::instance()->quit();
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN :
        case SDL_EVENT_GAMEPAD_BUTTON_UP :
        {
            QEvent::Type type =
                    event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ?
                        QEvent::Type::KeyPress : QEvent::Type::KeyRelease;

            // Swap face buttons if needed
            if (m_Prefs->swapFaceButtons) {
                switch (event.gbutton.button) {
                case SDL_GAMEPAD_BUTTON_SOUTH :
                    event.gbutton.button = SDL_GAMEPAD_BUTTON_EAST;
                    break;
                case SDL_GAMEPAD_BUTTON_EAST :
                    event.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
                    break;
                case SDL_GAMEPAD_BUTTON_WEST :
                    event.gbutton.button = SDL_GAMEPAD_BUTTON_NORTH;
                    break;
                case SDL_GAMEPAD_BUTTON_NORTH :
                    event.gbutton.button = SDL_GAMEPAD_BUTTON_WEST;
                    break;
                }
            }

            switch (event.gbutton.button) {
            case SDL_GAMEPAD_BUTTON_DPAD_UP :
                if (m_UiNavMode) {
                    // Back-tab
                    sendKey(type, Qt::Key_Tab, Qt::ShiftModifier);
                }
                else {
                    sendKey(type, Qt::Key_Up);
                }
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN :
                if (m_UiNavMode) {
                    sendKey(type, Qt::Key_Tab);
                }
                else {
                    sendKey(type, Qt::Key_Down);
                }
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT :
                sendKey(type, Qt::Key_Left);
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT :
                sendKey(type, Qt::Key_Right);
                break;
            case SDL_GAMEPAD_BUTTON_SOUTH :
                if (m_UiNavMode) {
                    sendKey(type, Qt::Key_Space);
                }
                else {
                    sendKey(type, Qt::Key_Return);
                }
                break;
            case SDL_GAMEPAD_BUTTON_EAST :
                sendKey(type, Qt::Key_Escape);
                break;
            case SDL_GAMEPAD_BUTTON_WEST :
                sendKey(type, Qt::Key_Menu);
                break;
            case SDL_GAMEPAD_BUTTON_NORTH :
            case SDL_GAMEPAD_BUTTON_START :
                // HACK: We use this keycode to inform main.qml
                // to show the settings when Key_Menu is handled
                // by the control in focus.
                sendKey(type, Qt::Key_Hangup);
                break;
            default:
                break;
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_ADDED :
#if SDL_VERSION_ATLEAST(3, 0, 0)
            SDL_Gamepad* gc = SDL_OpenGamepad(event.gdevice.which);
            if (gc != nullptr) {
                SDL_assert(!m_Gamepads.contains(gc));
                m_Gamepads.append(gc);
            }
#else
            SDL_Gamepad* gc = SDL_GameControllerOpen(event.cdevice.which);
            if (gc != nullptr) {
                // SDL_CONTROLLERDEVICEADDED can be reported multiple times for the same
                // gamepad in rare cases, because SDL2 doesn't fixup the device index in
                // the SDL_CONTROLLERDEVICEADDED event if an unopened gamepad disappears
                // before we've processed the add event.
                if (!m_Gamepads.contains(gc)) {
                    m_Gamepads.append(gc);
                }
                else {
                    // We already have this game controller open
                    SDL_CloseGamepad(gc);
                }
            }
#endif
            break;
        }
    }

    // Handle analog sticks by polling
    for (auto gc : m_Gamepads) {
        short leftX = SDL_GetGamepadAxis(gc, SDL_GAMEPAD_AXIS_LEFTX);
        short leftY = SDL_GetGamepadAxis(gc, SDL_GAMEPAD_AXIS_LEFTY);
        if (SDL_GetTicks() - m_LastAxisNavigationEventTime < AXIS_NAVIGATION_REPEAT_DELAY) {
            // Do nothing
        }
        else if (leftY < -30000) {
            if (m_UiNavMode) {
                // Back-tab
                sendKey(QEvent::Type::KeyPress, Qt::Key_Tab, Qt::ShiftModifier);
                sendKey(QEvent::Type::KeyRelease, Qt::Key_Tab, Qt::ShiftModifier);
            }
            else {
                sendKey(QEvent::Type::KeyPress, Qt::Key_Up);
                sendKey(QEvent::Type::KeyRelease, Qt::Key_Up);
            }

            m_LastAxisNavigationEventTime = SDL_GetTicks();
        }
        else if (leftY > 30000) {
            if (m_UiNavMode) {
                sendKey(QEvent::Type::KeyPress, Qt::Key_Tab);
                sendKey(QEvent::Type::KeyRelease, Qt::Key_Tab);
            }
            else {
                sendKey(QEvent::Type::KeyPress, Qt::Key_Down);
                sendKey(QEvent::Type::KeyRelease, Qt::Key_Down);
            }

            m_LastAxisNavigationEventTime = SDL_GetTicks();
        }
        else if (leftX < -30000) {
            sendKey(QEvent::Type::KeyPress, Qt::Key_Left);
            sendKey(QEvent::Type::KeyRelease, Qt::Key_Left);
            m_LastAxisNavigationEventTime = SDL_GetTicks();
        }
        else if (leftX > 30000) {
            sendKey(QEvent::Type::KeyPress, Qt::Key_Right);
            sendKey(QEvent::Type::KeyRelease, Qt::Key_Right);
            m_LastAxisNavigationEventTime = SDL_GetTicks();
        }
    }
}

void SdlGamepadKeyNavigation::sendKey(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    QGuiApplication* app = static_cast<QGuiApplication*>(QGuiApplication::instance());
    QWindow* focusWindow = app->focusWindow();
    if (focusWindow != nullptr) {
        QKeyEvent keyPressEvent(type, key, modifiers);
        app->sendEvent(focusWindow, &keyPressEvent);
    }
}

void SdlGamepadKeyNavigation::updateTimerState()
{
    if (m_PollingTimer->isActive() && (!m_HasFocus || !m_Enabled)) {
        m_PollingTimer->stop();
    }
    else if (!m_PollingTimer->isActive() && m_HasFocus && m_Enabled) {
        // Flush events on the first poll
        m_FirstPoll = true;

        // Poll every 50 ms for a new joystick event
        m_PollingTimer->start(50);
    }
}

void SdlGamepadKeyNavigation::setUiNavMode(bool uiNavMode)
{
    m_UiNavMode = uiNavMode;
}

int SdlGamepadKeyNavigation::getConnectedGamepads()
{
    Q_ASSERT(m_Enabled);

    int count = 0;
    SDL_free(SDL_GetGamepads(&count));
    return count;
}
