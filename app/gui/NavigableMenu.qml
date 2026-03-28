import QtQuick 2.0
import QtQuick.Controls 2.2

Menu {
    property var initiator

    onOpened: {
        // If the initiating object currently has keyboard focus,
        // give focus to the first visible and enabled menu item
        if (initiator.focus) {
            for (var i = 0; i < count; i++) {
                var item = itemAt(i)
                if (item.visible && item.enabled) {
                    item.forceActiveFocus(Qt.TabFocusReason)
                    break
                }
            }
        }
    }
}
