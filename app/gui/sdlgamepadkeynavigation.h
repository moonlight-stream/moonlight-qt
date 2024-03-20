#pragma once

#include <QTimer>
#include <QEvent>

#if HAVE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

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

private slots:
    void onPollingTimerFired();

private:
    QTimer* m_PollingTimer;
#if SDL_VERSION_ATLEAST(3, 0, 0)
    QList<SDL_Gamepad*> m_Gamepads;
#else
    QList<SDL_GameController*> m_Gamepads;
#endif
    bool m_Enabled;
    bool m_UiNavMode;
    bool m_FirstPoll;
    Uint32 m_LastAxisNavigationEventTime;
};
