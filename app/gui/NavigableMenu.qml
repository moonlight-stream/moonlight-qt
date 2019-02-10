import QtQuick 2.0
import QtQuick.Controls 2.2

Menu {
    onOpened: {
        itemAt(0).forceActiveFocus(Qt.TabFocusReason)
    }
}
