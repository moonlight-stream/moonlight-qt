import QtQuick 2.0
import QtQuick.Controls 2.5

Dialog {
    property string accessibleName: title || ""

    modal: true
    anchors.centerIn: Overlay.overlay

    // Accessibility support for screen readers
    Accessible.role: Accessible.Dialog
    Accessible.name: accessibleName

    onClosed: {
        // We must force focus back to the last item. If we don't,
        // gamepad and keyboard navigation will break after a
        // dialog appears.
        stackView.forceActiveFocus()
    }
}
