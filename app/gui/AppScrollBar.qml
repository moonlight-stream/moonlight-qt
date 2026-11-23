import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Controls.Material 2.2

ScrollBar {
    id: bar
    readonly property bool dark: Material.theme === Material.Dark
    readonly property color thumbColor: dark ? "#899cb3" : "#8093aa"
    readonly property color hoverColor: dark ? "#b6c7db" : "#526b88"
    implicitWidth: 16
    implicitHeight: 16
    padding: 4
    hoverEnabled: interactive
    minimumSize: Math.min(1, 24 / Math.max(1, orientation === Qt.Vertical ? availableHeight : availableWidth))
    visible: policy === ScrollBar.AlwaysOn || (policy === ScrollBar.AsNeeded && size < 1)

    // Material fades the thumb and track together when scrolling starts or stops.
    contentItem: Rectangle {
        radius: Math.min(width, height) / 2
        color: bar.pressed ? bar.Material.accent : bar.hovered ? bar.hoverColor : bar.thumbColor
        opacity: 0
        Behavior on color { ColorAnimation { duration: 100 } }
    }
    background: Item {
        opacity: 0
        visible: bar.interactive
        Rectangle {
            anchors.fill: parent
            anchors.margins: bar.padding
            radius: Math.min(width, height) / 2
            color: bar.dark ? "#344252" : "#e1e8f0"
        }
    }
}
