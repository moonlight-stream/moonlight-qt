import QtQuick 2.0
import QtQuick.Controls 2.2

MenuItem {
    // Qt 5.10 has a menu property, but we need to support 5.9
    // so we must make our own.
    property Menu parentMenu

    // Ensure focus can't be given to an invisible item
    enabled: visible
    height: visible ? implicitHeight : 0
    focusPolicy: visible ? Qt.TabFocus : Qt.NoFocus

    Keys.onReturnPressed: {
        triggered()
    }

    Keys.onEscapePressed: {
        parentMenu.close()
    }
}
