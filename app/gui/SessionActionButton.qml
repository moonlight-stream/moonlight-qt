import QtQuick 2.9
import QtQuick.Controls 2.2

ToolButton {
    id: control
    implicitWidth: 32
    implicitHeight: 32
    padding: 0
    hoverEnabled: true
    display: AbstractButton.IconOnly
    Accessible.name: text
    icon.width: 24
    icon.height: 24
    background: Rectangle {
        radius: 6
        color: control.icon.color
        opacity: control.down ? 0.22 : control.hovered || control.visualFocus ? 0.13 : 0
    }
    ToolbarToolTip { text: control.text }
}
