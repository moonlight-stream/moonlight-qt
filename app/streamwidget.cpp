#include "streamwidget.h"

#include <QDebug>
#include <QGuiApplication>
#include <QGamepadManager>

#include "Limelight.h"

#define VK_0 0x30
#define VK_A 0x41

// These are real Windows VK_* codes
#ifndef VK_F1
#define VK_F1 0x70
#define VK_NUMPAD0 0x60
#endif

StreamWidget::StreamWidget(QWidget *parent) :
    QWidget(parent),
    m_LastMouseX(0),
    m_LastMouseY(0),
    m_ScrollDeltaX(0),
    m_ActiveGamepadMask(0)
{
    // Ensure we get mouse move events even if no buttons are down
    setMouseTracking(true);

    // Register for QGamepadManager's signals
    QGamepadManager* gpm = QGamepadManager::instance();
    connect(gpm, &QGamepadManager::gamepadConnected, this, &StreamWidget::gamepadConnected);
    connect(gpm, &QGamepadManager::gamepadDisconnected, this, &StreamWidget::gamepadDisconnected);
    connect(gpm, &QGamepadManager::gamepadButtonPressEvent, this, &StreamWidget::gamepadButtonPressEvent);
    connect(gpm, &QGamepadManager::gamepadButtonReleaseEvent, this, &StreamWidget::gamepadButtonReleaseEvent);
    connect(gpm, &QGamepadManager::gamepadAxisEvent, this, &StreamWidget::gamepadAxisEvent);

    // We won't be invoked for existing gamepads, so "connect" them now
    for (int deviceId : gpm->connectedGamepads())
    {
        gamepadConnected(deviceId);
    }
}

void
StreamWidget::mouseMoveEvent(QMouseEvent *event)
{
    int currentX = event->globalX();
    int currentY = event->globalY();

    if (m_LastMouseX != 0 && m_LastMouseY != 0)
    {
        LiSendMouseMoveEvent(m_LastMouseX - currentX,
                             m_LastMouseY - currentY);
    }

    m_LastMouseX = currentX;
    m_LastMouseY = currentY;

    event->accept();
}

void
StreamWidget::handleMouseButtonEvent(QMouseEvent *event)
{
    Qt::MouseButton button = event->button();
    int buttonCode;

    Q_ASSERT(button != Qt::MouseButton::NoButton);
    Q_ASSERT(event->type() == QEvent::Type::MouseButtonPress ||
             event->type() == QEvent::Type::MouseButtonRelease);

    // Accept all mouse button events
    event->accept();

    switch (button)
    {
    case Qt::MouseButton::LeftButton:
        buttonCode = BUTTON_LEFT;
        break;
    case Qt::MouseButton::MiddleButton:
        buttonCode = BUTTON_MIDDLE;
        break;
    case Qt::MouseButton::RightButton:
        buttonCode = BUTTON_RIGHT;
        break;
    default:
        qWarning() << "Unhandled mouse button: " << button;
        return;
    }

    LiSendMouseButtonEvent((event->type() == QEvent::Type::MouseButtonPress) ?
                               BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE,
                           buttonCode);
}

void
StreamWidget::mousePressEvent(QMouseEvent *event)
{
    handleMouseButtonEvent(event);
}

void
StreamWidget::mouseReleaseEvent(QMouseEvent *event)
{
    handleMouseButtonEvent(event);
}

void
StreamWidget::wheelEvent(QWheelEvent *event)
{
    QPoint scrollDelta = event->angleDelta();

    if (!scrollDelta.isNull())
    {
        m_ScrollDeltaX += scrollDelta.x();
    }

    // See if we've accumulated enough to send
    if (m_ScrollDeltaX / 120 != 0)
    {
        // Send the scroll event
        LiSendScrollEvent(m_ScrollDeltaX / 120);

        // Subtract the portion of the total scroll delta we're "consuming"
        m_ScrollDeltaX -= (m_ScrollDeltaX / 120) * 120;
    }

    event->accept();
}

void
StreamWidget::handleKeyEvent(QKeyEvent *event)
{
    // Accept all events
    event->accept();

    Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
    char modifierFlags = 0;
    char keyAction = event->type() == QEvent::KeyPress ?
                KEY_ACTION_DOWN : KEY_ACTION_UP;

    // Don't send auto-repeating events
    if (keyAction == KEY_ACTION_DOWN && event->isAutoRepeat())
    {
        return;
    }

    if (modifiers.testFlag(Qt::KeyboardModifier::AltModifier))
    {
        modifierFlags |= MODIFIER_ALT;
    }
    if (modifiers.testFlag(Qt::KeyboardModifier::ControlModifier))
    {
        modifierFlags |= MODIFIER_CTRL;
    }
    if (modifiers.testFlag(Qt::KeyboardModifier::ShiftModifier))
    {
        modifierFlags |= MODIFIER_SHIFT;
    }

    short keyCode;
    Qt::Key key = static_cast<Qt::Key>(event->key());

    if (key >= Qt::Key::Key_0 && key <= Qt::Key::Key_9)
    {
        // In Qt, there's no separate key code for numpad button.
        // Numpad is indicated in the modifier flags.
        keyCode = (key - Qt::Key::Key_0) +
                (event->modifiers().testFlag(Qt::KeyboardModifier::KeypadModifier) ?
                    VK_NUMPAD0 : VK_0);
    }
    else if (key >= Qt::Key::Key_A && key <= Qt::Key::Key_Z)
    {
        keyCode = (key - Qt::Key::Key_A) + VK_A;
    }
    else if (key >= Qt::Key::Key_F1 && key <= Qt::Key::Key_F12)
    {
        keyCode = (key - Qt::Key::Key_F1) + VK_F1;
    }
    else
    {
        switch (key)
        {
        case Qt::Key::Key_Alt:
            // TODO: Tell left and right apart
            keyCode = 0xA4; // 0xA5 (right)
            break;
        case Qt::Key::Key_Backslash:
            keyCode = 0xDC;
            break;
        case Qt::Key::Key_CapsLock:
            keyCode = 0x14;
            break;
        case Qt::Key::Key_Clear:
            keyCode = 0x0C;
            break;
        case Qt::Key::Key_Comma:
            keyCode = 0xBC;
            break;
        case Qt::Key::Key_Control:
            // TODO: left and right
            keyCode = 0xA2; // 0xA3 (right)
            break;
        case Qt::Key::Key_Backspace:
            keyCode = 0x08;
            break;
        case Qt::Key::Key_Return:
            keyCode = 0x0D;
            break;
        case Qt::Key::Key_Equal:
            keyCode = 0xBB;
            break;
        case Qt::Key::Key_Escape:
            keyCode = 0x1B;
            break;
        case Qt::Key::Key_Delete:
            keyCode = 0x2E;
            break;
        case Qt::Key::Key_Insert:
            keyCode = 0x2D;
            break;
        case Qt::Key::Key_BracketLeft:
            keyCode = 0xDB;
            break;
        case Qt::Key::Key_BracketRight:
            keyCode = 0xDD;
            break;
        case Qt::Key::Key_Minus:
            keyCode = 0xBD;
            break;
        case Qt::Key::Key_End:
            keyCode = 0x23;
            break;
        case Qt::Key::Key_Home:
            keyCode = 0x24;
            break;
        case Qt::Key::Key_NumLock:
            keyCode = 0x90;
            break;
        case Qt::Key::Key_PageDown:
            keyCode = 0x22;
            break;
        case Qt::Key::Key_PageUp:
            keyCode = 0x21;
            break;
        case Qt::Key::Key_Period:
            keyCode = 0xBE;
            break;
        case Qt::Key::Key_ScrollLock:
            keyCode = 0x91;
            break;
        case Qt::Key::Key_Semicolon:
            keyCode = 0xBA;
            break;
        case Qt::Key::Key_Shift:
            // TODO: left vs right
            keyCode = 0xA0; // A1 (right)
            break;
        case Qt::Key::Key_Slash:
            keyCode = 0xBF;
            break;
        case Qt::Key::Key_Space:
            keyCode = 0x20;
            break;
        case Qt::Key::Key_SysReq:
            keyCode = 0x9A;
            break;
        case Qt::Key::Key_Tab:
            keyCode = 0x09;
            break;
        case Qt::Key::Key_Left:
            keyCode = 0x25;
            break;
        case Qt::Key::Key_Right:
            keyCode = 0x27;
            break;
        case Qt::Key::Key_Up:
            keyCode = 0x26;
            break;
        case Qt::Key::Key_Down:
            keyCode = 0x28;
            break;
        case Qt::Key::Key_QuoteLeft:
            keyCode = 0xC0;
            break;
        case Qt::Key::Key_Apostrophe:
            keyCode = 0xDE;
            break;
        case Qt::Key::Key_Pause:
            keyCode = 0x13;
            break;
        // FIXME: Numpad keys
        default:
            qWarning() << "Unhandled key: " << key;
            return;
        }
    }

    LiSendKeyboardEvent(0x80 | keyCode, keyAction, modifierFlags);
}

void
StreamWidget::keyPressEvent(QKeyEvent *event)
{
    handleKeyEvent(event);
}

void
StreamWidget::keyReleaseEvent(QKeyEvent *event)
{
    handleKeyEvent(event);
}

void
StreamWidget::gamepadAxisEvent(int deviceId, QGamepadManager::GamepadAxis axis, double value)
{
    GamepadState* gamepad = m_Gamepads.value(deviceId, nullptr);
    Q_ASSERT(gamepad != nullptr);

    switch (axis)
    {
    case QGamepadManager::GamepadAxis::AxisLeftX:
        gamepad->LeftStickX = value * 0x7FFF;
        break;
    case QGamepadManager::GamepadAxis::AxisLeftY:
        gamepad->LeftStickY = value * 0x7FFF;
        break;
    case QGamepadManager::GamepadAxis::AxisRightX:
        gamepad->RightStickX = value * 0x7FFF;
        break;
    case QGamepadManager::GamepadAxis::AxisRightY:
        gamepad->RightStickY = value * 0x7FFF;
        break;
    default:
        qWarning() << "Unhandled axis: " << axis;
        Q_ASSERT(false);
        return;
    }
}

void
StreamWidget::handleGamepadButtonEvent(int deviceId, QGamepadManager::GamepadButton button, double value)
{
    GamepadState* gamepad = m_Gamepads.value(deviceId, nullptr);
    Q_ASSERT(gamepad != nullptr);


    // Triggers are special cases
    if (button == QGamepadManager::GamepadButton::ButtonL2)
    {
        gamepad->LeftTrigger = value * 0xFF;
    }
    else if (button == QGamepadManager::GamepadButton::ButtonR2)
    {
        gamepad->RightTrigger = value * 0xFF;
    }
    else
    {
        short buttonFlag;
        switch (button)
        {
        case QGamepadManager::GamepadButton::ButtonA:
            buttonFlag = A_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonB:
            buttonFlag = B_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonX:
            buttonFlag = X_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonY:
            buttonFlag = Y_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonSelect:
            buttonFlag = BACK_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonStart:
            buttonFlag = PLAY_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonGuide:
            buttonFlag = SPECIAL_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonUp:
            buttonFlag = UP_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonDown:
            buttonFlag = DOWN_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonLeft:
            buttonFlag = LEFT_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonRight:
            buttonFlag = RIGHT_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonL3:
            buttonFlag = LS_CLK_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonR3:
            buttonFlag = RS_CLK_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonL1:
            buttonFlag = LB_FLAG;
            break;
        case QGamepadManager::GamepadButton::ButtonR1:
            buttonFlag = RB_FLAG;
            break;
        default:
            qWarning() << "Unhandled button: " << button;
            return;
        }

        if (value != 0)
        {
            gamepad->ButtonFlags |= buttonFlag;
        }
        else
        {
            gamepad->ButtonFlags &= ~buttonFlag;
        }
    }

    LiSendMultiControllerEvent(gamepad->Index,
                               m_ActiveGamepadMask,
                               gamepad->ButtonFlags,
                               gamepad->LeftTrigger,
                               gamepad->RightTrigger,
                               gamepad->LeftStickX,
                               gamepad->LeftStickY,
                               gamepad->RightStickX,
                               gamepad->RightStickY);
}

void
StreamWidget::gamepadButtonPressEvent(int deviceId, QGamepadManager::GamepadButton button, double value)
{
    handleGamepadButtonEvent(deviceId, button, value);
}

void
StreamWidget::gamepadButtonReleaseEvent(int deviceId, QGamepadManager::GamepadButton button)
{
    handleGamepadButtonEvent(deviceId, button, 0);
}

void
StreamWidget::gamepadConnected(int deviceId)
{
    Q_ASSERT(!m_Gamepads.contains(deviceId));

    // Find an unallocated gamepad index and reserve it
    GamepadState* gamepad = new GamepadState();
    for (int i = 0; i < 4; i++)
    {
        if ((m_ActiveGamepadMask & (1 << i)) == 0)
        {
            m_ActiveGamepadMask |= (1 << i);
            gamepad->Index = i;
            break;
        }
    }

    qDebug() << "New gamepad connected " << deviceId << " at index " << gamepad->Index;

    m_Gamepads.insert(deviceId, gamepad);
}

void
StreamWidget::gamepadDisconnected(int deviceId)
{
    GamepadState* gamepad = m_Gamepads.take(deviceId);
    Q_ASSERT(gamepad != nullptr);

    qDebug() << "Gamepad disconnected " << deviceId << " at index " << gamepad->Index;

    // Clear the allocated gamepad index
    m_ActiveGamepadMask &= ~(1 << gamepad->Index);

    delete gamepad;
}
