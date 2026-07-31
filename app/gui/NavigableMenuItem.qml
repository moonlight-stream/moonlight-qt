import QtQuick 2.0
import QtQuick.Controls

import "theme"

MenuItem {
    id: menuItem
    // Qt 5.10 has a menu property, but we need to support 5.9
    // so we must make our own.
    property Menu parentMenu
    // hover 以前是「浅底 + 深字」反白，在深色硬边风格里太跳。改成 surface2 底 +
    // 文字提亮，配左侧一条强调粗条指示当前项。
    property color hoverColor: Theme.surface2
    property color textColor: Theme.textDim
    property color hoverTextColor: Theme.text
    property bool showIcon: false
    property string iconSource: ""
    property int iconSize: 16

    // Ensure focus can't be given to an invisible item
    enabled: visible
    height: visible ? implicitHeight : 0
    focusPolicy: visible ? Qt.TabFocus : Qt.NoFocus
    hoverEnabled: true

    readonly property bool active: hovered || visualFocus

    contentItem: Row {
        spacing: Theme.spaceSm

        // 替代 leftPadding 的空白项，仅在复选框时显示
        Item {
            visible: menuItem.checkable
            width: 16
            height: 1
        }

        Image {
            visible: menuItem.showIcon && menuItem.iconSource !== ""
            source: menuItem.iconSource
            width: menuItem.iconSize
            height: menuItem.iconSize
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: menuItem.text
            color: menuItem.active ? menuItem.hoverTextColor : menuItem.textColor
            font.family: Theme.fontSans
            font.pointSize: Theme.fontRowTitle
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            Behavior on color {
                ColorAnimation { duration: Theme.durFast }
            }
        }
    }

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 34
        radius: 0
        color: menuItem.active ? menuItem.hoverColor : "transparent"

        Rectangle {
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
            width: menuItem.active ? Theme.accentBar : 0
            color: Theme.accent
        }

        Behavior on color {
            ColorAnimation { duration: Theme.durFast }
        }
    }

    onTriggered: {
        // We must close the context menu first or
        // it can steal focus from any dialogs that
        // onTriggered may spawn.
        menu.close()
    }

    Keys.onReturnPressed: {
        triggered()
    }

    Keys.onEnterPressed: {
        triggered()
    }

    Keys.onEscapePressed: {
        menu.close()
    }
}
