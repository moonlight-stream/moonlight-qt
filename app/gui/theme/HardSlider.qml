import QtQuick 2.9
import QtQuick.Controls
import "."

// 方角滑条：4px 方槽 + 竖直方块把手，把手带一小截硬投影。
Slider {
    id: control

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 200
        implicitHeight: 4
        width: control.availableWidth
        height: 4

        radius: 0
        color: Theme.surface2
        border.width: 1
        border.color: Theme.line

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 0
            color: control.enabled ? Theme.accent : Theme.textFaint
        }
    }

    handle: Item {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 12
        implicitHeight: 26

        Rectangle {
            x: 2
            y: 2
            width: parent.width
            height: parent.height
            color: Theme.shadowColor
        }

        Rectangle {
            anchors.fill: parent
            radius: 0
            color: control.pressed ? Theme.accentStrong : Theme.accent
            border.width: 1
            border.color: control.visualFocus ? Theme.text : Theme.ink
            opacity: control.enabled ? 1.0 : 0.45
        }
    }
}
