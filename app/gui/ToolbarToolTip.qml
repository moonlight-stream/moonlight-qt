import QtQuick 2.9
import QtQuick.Controls 2.2

ToolTip {
    id: tip
    visible: parent.visible && (parent.hovered || parent.visualFocus)
    delay: 600
    timeout: 3000
    y: parent.height + 4
    padding: 6
    leftPadding: 10
    rightPadding: 10
    contentItem: Label {
        text: tip.text
        wrapMode: Text.Wrap
        font.pixelSize: 13
        color: "#20364e"
    }
    background: Rectangle {
        radius: 6
        color: "#f8fafc"
        border.color: "#cbd6e2"
    }
}
