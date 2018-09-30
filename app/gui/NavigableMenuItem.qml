import QtQuick 2.0
import QtQuick.Controls 2.2

MenuItem {
    // Ensure focus can't be given to an invisible item
    enabled: visible
    height: visible ? implicitHeight : 0

    Keys.onReturnPressed: {
        triggered()
    }
}
