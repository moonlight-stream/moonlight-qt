#include "sdlgamepadkeynavigation.h"

#include <QKeyEvent>
#include <QGuiApplication>
#include <QWindow>

#include "settings/mappingmanager.h"

SdlGamepadKeyNavigation::SdlGamepadKeyNavigation()
    : m_Enabled(false),
      m_SettingsMode(false)
{
    m_PollingTimer = new QTimer(this);
    connect(m_PollingTimer, SIGNAL(timeout()), this, SLOT(onPollingTimerFired()));
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
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
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
    SDL_FlushEvent(SDL_CONTROLLERDEVICEADDED);

    // Open all currently attached game controllers
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameController* gc = SDL_GameControllerOpen(i);
            if (gc != nullptr) {
                m_Gamepads.append(gc);
            }
        }
    }

    // Poll every 50 ms for a new joystick event
    m_PollingTimer->start(50);

    m_Enabled = true;
}

void SdlGamepadKeyNavigation::disable()
{
    if (!m_Enabled) {
        return;
    }

    m_PollingTimer->stop();

    while (!m_Gamepads.isEmpty()) {
        SDL_GameControllerClose(m_Gamepads[0]);
        m_Gamepads.removeAt(0);
    }

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);

    m_Enabled = false;
}

void SdlGamepadKeyNavigation::onPollingTimerFired()
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        {
            QEvent::Type type =
                    event.type == SDL_CONTROLLERBUTTONDOWN ?
                        QEvent::Type::KeyPress : QEvent::Type::KeyRelease;

            switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                if (m_SettingsMode) {
                    // Back-tab
                    sendKey(type, Qt::Key_Tab, Qt::ShiftModifier);
                }
                else {
                    sendKey(type, Qt::Key_Up);
                }
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                if (m_SettingsMode) {
                    sendKey(type, Qt::Key_Tab);
                }
                else {
                    sendKey(type, Qt::Key_Down);
                }
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                sendKey(type, Qt::Key_Left);
                if (m_SettingsMode) {
                    // Some settings controls respond to left/right (like the slider)
                    // and others respond to up/down (like combo boxes). They seem to
                    // be mutually exclusive though so let's just send both.
                    sendKey(type, Qt::Key_Up);
                }
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                sendKey(type, Qt::Key_Right);
                if (m_SettingsMode) {
                    // Some settings controls respond to left/right (like the slider)
                    // and others respond to up/down (like combo boxes). They seem to
                    // be mutually exclusive though so let's just send both.
                    sendKey(type, Qt::Key_Down);
                }
                break;
            case SDL_CONTROLLER_BUTTON_A:
                if (m_SettingsMode) {
                    sendKey(type, Qt::Key_Space);
                }
                else {
                    sendKey(type, Qt::Key_Return);
                }
                break;
            case SDL_CONTROLLER_BUTTON_B:
                sendKey(type, Qt::Key_Escape);
                break;
            case SDL_CONTROLLER_BUTTON_X:
            case SDL_CONTROLLER_BUTTON_Y:
            case SDL_CONTROLLER_BUTTON_START:
                sendKey(type, Qt::Key_Menu);
                break;
            default:
                break;
            }
            break;
        }
        case SDL_CONTROLLERDEVICEADDED:
            SDL_GameController* gc = SDL_GameControllerOpen(event.cdevice.which);
            if (gc != nullptr) {
                m_Gamepads.append(gc);
            }
            break;
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

void SdlGamepadKeyNavigation::setSettingsMode(bool settingsMode)
{
    m_SettingsMode = settingsMode;
}
