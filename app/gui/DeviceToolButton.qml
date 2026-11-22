import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

AbstractButton {
    id: control
    property url iconSource
    property bool listItem: false
    property bool iconOnly: false
    hoverEnabled: true
    Accessible.name: text
    padding: listItem ? 12 : 5
    opacity: enabled ? 1 : 0.44
    background: Rectangle {
        radius: 5
        color: control.down ? "#dce8fa" : control.hovered || control.visualFocus ? "#e6effb" :
               control.listItem ? "#f6f8fb" : "transparent"
        border.width: control.visualFocus || control.listItem ? 1 : 0
        border.color: control.visualFocus ? "#2468d5" : "#dce3ea"
    }
    contentItem: RowLayout {
        spacing: 7
        Item { Layout.fillWidth: true; visible: !control.listItem }
        Image {
            Layout.preferredWidth: 19; Layout.preferredHeight: 19
            sourceSize.width: 24; sourceSize.height: 24
            source: control.iconSource
        }
        Label {
            visible: !control.iconOnly
            Layout.fillWidth: control.listItem
            Layout.minimumWidth: 0
            text: control.text
            color: "#233449"
            font.pixelSize: 14
            elide: Text.ElideRight
        }
        Item { Layout.fillWidth: true; visible: !control.listItem }
        Label {
            visible: control.listItem
            text: "\u203a"
            font.pixelSize: 21
            color: "#748397"
            Accessible.ignored: true
        }
    }
    ToolbarToolTip {
        visible: control.visible && control.iconOnly && (control.hovered || control.visualFocus)
        text: control.text
    }
    Keys.onReturnPressed: function(event) { if (!event.isAutoRepeat) clicked() }
    Keys.onEnterPressed: function(event) { if (!event.isAutoRepeat) clicked() }
}
