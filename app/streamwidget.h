#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QScrollEvent>
#include <QKeyEvent>
#include <QGamepad>

class GamepadState
{
public:
    GamepadState() :
        ButtonFlags(0),
        LeftStickX(0),
        LeftStickY(0),
        RightStickX(0),
        RightStickY(0),
        LeftTrigger(0),
        RightTrigger(0) {}

    short Index;
    short ButtonFlags;
    short LeftStickX, LeftStickY;
    short RightStickX, RightStickY;
    unsigned char LeftTrigger, RightTrigger;
};

class StreamWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StreamWidget(QWidget *parent = nullptr);

protected:
    void
    mouseMoveEvent(QMouseEvent *event);

    void
    wheelEvent(QWheelEvent *event);

    void
    mousePressEvent(QMouseEvent *event);

    void
    mouseReleaseEvent(QMouseEvent *event);

    void
    keyPressEvent(QKeyEvent *event);

    void
    keyReleaseEvent(QKeyEvent *event);

    void
    gamepadConnected(int deviceId);

    void
    gamepadDisconnected(int deviceId);

    void
    gamepadAxisEvent(int deviceId, QGamepadManager::GamepadAxis axis, double value);

    void
    gamepadButtonPressEvent(int deviceId, QGamepadManager::GamepadButton button, double value);

    void
    gamepadButtonReleaseEvent(int deviceId, QGamepadManager::GamepadButton button);

signals:

public slots:

private:
    void
    handleMouseButtonEvent(QMouseEvent *event);

    void
    handleKeyEvent(QKeyEvent *event);

    void
    handleGamepadButtonEvent(int deviceId, QGamepadManager::GamepadButton button, double value);

    int m_LastMouseX, m_LastMouseY;
    int m_ScrollDeltaX;
    short m_ActiveGamepadMask;
    QMap<int, GamepadState*> m_Gamepads;
};
