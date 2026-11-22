import QtQuick 2.9
import QtQuick.Controls 2.2

Switch {
    id: control
    width: parent.width
    font.pixelSize: 14
    padding: 8; leftPadding: 12; rightPadding: 12
    spacing: 12
    hoverEnabled: true
    indicator: Rectangle {
        width: 36; height: 20; radius: 10
        x: control.width - width - control.rightPadding
        anchors.verticalCenter: parent.verticalCenter
        color: !control.enabled ? "#dce2e8" : control.checked ? "#167384" : "#788c97"
        Rectangle {
            width: 14; height: 14; radius: 7
            anchors.verticalCenter: parent.verticalCenter
            x: control.checked ? parent.width - width - 3 : 3
            color: "white"
        }
    }
    contentItem: Label {
        text: control.text; font: control.font
        rightPadding: control.indicator.width + control.spacing
        wrapMode: Text.Wrap
        color: control.enabled ? "#25374c" : "#69788a"
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 6
        color: control.hovered ? "#eff6f7" : "white"
        border.color: control.visualFocus ? "#167384" : "#dbe5e8"
    }
}
