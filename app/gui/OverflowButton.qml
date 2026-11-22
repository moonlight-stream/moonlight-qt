import QtQuick 2.9
import QtQuick.Controls 2.2

ToolButton {
    id: control
    implicitWidth: 32
    implicitHeight: 32
    padding: 0
    hoverEnabled: true

    contentItem: Item {
        Column {
            anchors.centerIn: parent
            spacing: 3
            Repeater {
                model: 3
                Rectangle {
                    width: 4; height: 4; radius: 2
                    color: "#182334"
                }
            }
        }
    }

    background: Rectangle {
        radius: 6
        color: control.down ? "#cddceb" : control.hovered || control.visualFocus ? "#e1e9f2" : "transparent"
        border.width: control.visualFocus ? 1 : 0
        border.color: "#2468d5"
    }
}
