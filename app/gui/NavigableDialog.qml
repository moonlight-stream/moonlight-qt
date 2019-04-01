import QtQuick 2.0
import QtQuick.Controls 2.2

Dialog {
    property Item originalFocusItem

    x: Math.round((window.width - width) / 2)
    y: Math.round((window.height - height) / 2)

    onAboutToShow: {
        originalFocusItem = window.activeFocusItem
    }

    onOpened: {
        // Force focus on the dialog to ensure keyboard navigation works
        forceActiveFocus()
    }

    onClosed: {
        // We must force focus back to the last item for platforms without
        // support for more than one active window like Steam Link. If
        // we don't, gamepad and keyboard navigation will break after a
        // dialog appears.
        originalFocusItem.forceActiveFocus()
    }
}
