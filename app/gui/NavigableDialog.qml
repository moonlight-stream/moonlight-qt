import QtQuick 2.0
import QtQuick.Controls 2.5

Dialog {
    modal: true
    anchors.centerIn: Overlay.overlay

    onClosed: {
        // We must force focus back to the last item. If we don't,
        // gamepad and keyboard navigation will break after a
        // dialog appears.
        stackView.forceActiveFocus()
    }
}
