import QtQuick 2.0
import QtQuick.Dialogs 1.2

MessageDialog {
    property Item originalFocusItem

    onVisibleChanged: {
        if (!isWindow) {
            if (visible) {
                originalFocusItem = window.activeFocusItem
            }
            else {
                // We must force focus back to the last item for platforms without
                // support for more than one active window like Steam Link. If
                // we don't, gamepad and keyboard navigation will break after a
                // dialog appears.
                originalFocusItem.forceActiveFocus()
            }
        }
    }
}
