import QtQuick 2.0
import QtQuick.Controls 2.2

MenuItem {
    id: menuItem
    // Qt 5.10 has a menu property, but we need to support 5.9
    // so we must make our own.
    property Menu parentMenu
    property color hoverColor: "#e0e0e0"
    property color textColor: "#CCCCCC"
    property color hoverTextColor: "#101010"
    property bool showIcon: false
    property string iconSource: ""
    property int iconSize: 16

    // Ensure focus can't be given to an invisible item
    enabled: visible
    height: visible ? implicitHeight : 0
    focusPolicy: visible ? Qt.TabFocus : Qt.NoFocus
    
    contentItem: Row {
        spacing: 8
        
        // 替代 leftPadding 的空白项
        Item {
            width: 8
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
            font: menuItem.font
            color: menuItem.hovered ? menuItem.hoverTextColor : menuItem.textColor
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            
            Behavior on color {
                ColorAnimation { duration: 150 }
            }
        }
    }
    
    background: Rectangle {
        implicitWidth: 200
        color: menuItem.hovered ? menuItem.hoverColor : "transparent"
        radius: 2
        
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    onTriggered: {
        // We must close the context menu first or
        // it can steal focus from any dialogs that
        // onTriggered may spawn.
        parentMenu.close()
    }

    Keys.onReturnPressed: {
        triggered()
    }

    Keys.onEnterPressed: {
        triggered()
    }

    Keys.onEscapePressed: {
        parentMenu.close()
    }
}
