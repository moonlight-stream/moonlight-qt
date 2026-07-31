import QtQuick
import QtQuick.Controls

import "theme"

AbstractButton {
    id: control

    property string controlType
    property string accessibleName
    property color highlightColor: Theme.accent

    width: 44
    height: 40
    focusPolicy: Qt.NoFocus
    hoverEnabled: true

    Accessible.name: accessibleName
    Accessible.role: Accessible.Button

    ToolTip.delay: 700
    ToolTip.timeout: 2500
    ToolTip.visible: hovered
    ToolTip.text: accessibleName

    background: Rectangle {
        color: control.down ? Qt.darker(control.highlightColor, 1.18)
                            : (control.hovered ? control.highlightColor : "transparent")

        Rectangle {
            anchors.left: parent.left
            width: 1
            height: parent.height
            color: control.hovered ? Qt.rgba(15 / 255, 17 / 255, 21 / 255, 0.28)
                                   : Theme.line
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: control.hovered ? 3 : 0
            color: Theme.ink
        }

        Behavior on color {
            ColorAnimation { duration: Theme.durFast }
        }
    }

    contentItem: Item {
        readonly property color strokeColor:
            (control.hovered || control.down) ? Theme.ink : Theme.textDim

        Rectangle {
            visible: control.controlType === "minimize"
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 4
            width: 13
            height: 2
            color: parent.strokeColor
        }

        Rectangle {
            visible: control.controlType === "maximize"
            anchors.centerIn: parent
            width: 12
            height: 10
            color: "transparent"
            border.width: 2
            border.color: parent.strokeColor
        }

        Item {
            visible: control.controlType === "restore"
            anchors.centerIn: parent
            width: 15
            height: 13

            Rectangle {
                x: 4
                y: 0
                width: 11
                height: 9
                color: control.hovered ? control.highlightColor : Theme.ink
                border.width: 2
                border.color: parent.parent.strokeColor
            }

            Rectangle {
                x: 0
                y: 4
                width: 11
                height: 9
                color: control.hovered ? control.highlightColor : Theme.ink
                border.width: 2
                border.color: parent.parent.strokeColor
            }
        }

        Item {
            visible: control.controlType === "close"
            anchors.centerIn: parent
            width: 14
            height: 14

            Rectangle {
                anchors.centerIn: parent
                width: 17
                height: 2
                rotation: 45
                color: parent.parent.strokeColor
                antialiasing: true
            }

            Rectangle {
                anchors.centerIn: parent
                width: 17
                height: 2
                rotation: -45
                color: parent.parent.strokeColor
                antialiasing: true
            }
        }
    }
}
