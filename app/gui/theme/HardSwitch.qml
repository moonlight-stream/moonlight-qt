import QtQuick 2.9
import QtQuick.Controls
import "."

// 方角开关：方轨 + 方滑块，滑块位移不带回弹。
Switch {
    id: control

    font.family: Theme.fontSans

    indicator: Rectangle {
        implicitWidth: 44
        implicitHeight: 22
        x: control.text ? control.leftPadding : control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2

        radius: 0
        color: control.checked ? Theme.accent : Theme.surface2
        border.width: 1
        border.color: !control.enabled ? Theme.line
                    : control.checked ? Theme.accent
                    : (control.hovered || control.visualFocus ? Theme.accent : Theme.lineStrong)
        opacity: control.enabled ? 1.0 : 0.45

        Behavior on color {
            ColorAnimation { duration: Theme.durFast }
        }

        Rectangle {
            y: 3
            x: control.checked ? parent.width - width - 3 : 3
            width: 16
            height: parent.height - 6
            radius: 0
            color: control.checked ? Theme.ink : Theme.textDim

            Behavior on x {
                NumberAnimation { duration: Theme.durFast; easing.type: Theme.easing }
            }
        }
    }
}
