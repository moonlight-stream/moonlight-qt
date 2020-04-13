#pragma once

#include <QTimer>
#include <QEvent>
#include <QMap>

#include <SDL.h>

class SdlGamepadKeyNavigation : public QObject
{
    Q_OBJECT

public:
    SdlGamepadKeyNavigation();

    ~SdlGamepadKeyNavigation();

    Q_INVOKABLE void enable();

    Q_INVOKABLE void disable();

    Q_INVOKABLE void setUiNavMode(bool settingsMode);

    Q_INVOKABLE int getConnectedGamepads();

private:
    void sendKey(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier);

    bool mapGameController(SDL_JoystickID joystickInstanceId, SDL_GameController *gc);

    bool unMapGameController(SDL_JoystickID joystickInstanceId);

    bool openGameController(int joystickIndex);

    bool closeGameController(SDL_JoystickID joystickInstanceId);

    bool closeGameController(SDL_JoystickID joystickInstanceId, SDL_GameController *gc);

private slots:
    void onPollingTimerFired();

private:
    QTimer* m_PollingTimer;
    QMap<SDL_JoystickID, SDL_GameController*> m_GamepadMap;
    bool m_Enabled;
    bool m_UiNavMode;
    Uint32 m_LastAxisNavigationEventTime;
};
