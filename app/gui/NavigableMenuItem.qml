import QtQuick 2.0
import QtQuick.Controls 2.2

MenuItem {
    // Ensure focus can't be given to an invisible item
    enabled: visible
    height: visible ? implicitHeight : 0
    focusPolicy: visible ? Qt.TabFocus : Qt.NoFocus

    // Accessibility support for screen readers
    Accessible.role: Accessible.MenuItem
    Accessible.name: text
    Accessible.onPressAction: triggered()

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
