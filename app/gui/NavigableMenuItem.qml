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
