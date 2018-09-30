#pragma once

#include <QTimer>
#include <QEvent>

#include <SDL.h>

class SdlGamepadKeyNavigation : public QObject
{
    Q_OBJECT

public:
    SdlGamepadKeyNavigation();

    ~SdlGamepadKeyNavigation();

    Q_INVOKABLE void enable();

    Q_INVOKABLE void disable();

    Q_INVOKABLE void setSettingsMode(bool settingsMode);

private:
    void sendKey(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier);

private slots:
    void onPollingTimerFired();

private:
    QTimer* m_PollingTimer;
    QList<SDL_GameController*> m_Gamepads;
    bool m_Enabled;
    bool m_SettingsMode;
};
