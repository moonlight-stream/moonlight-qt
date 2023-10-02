import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2

import StreamingPreferences 1.0

import HotkeyModel 1.0

CenteredGridView {

    function createHotkeyModel() {
        var model = Qt.createQmlObject('import HotkeyModel 1.0; HotkeyModel {}', parent, '')
        model.initialize(StreamingPreferences)
        return model
    }

    property HotkeyModel hotkeyModel : createHotkeyModel()

    id: hotkeysGrid
    focus: true
    activeFocusOnTab: true
    topMargin: 20
    bottomMargin: 5
    cellWidth: 310; cellHeight: 330;

    objectName: qsTr("Hotkeys")

    model: hotkeyModel

    delegate: NavigableItemDelegate {
        width: 220; height: 287;
        grid: hotkeysGrid

        Label {
            id: pcNameText
            text: model.pc

            width: parent.width
            anchors.top: parent.top
            font.pointSize: 22
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }

        Label {
            id: indexText
            text: model.hotkeyNumber

            width: parent.width
            bottomPadding: 4
            anchors.verticalCenter: parent.verticalCenter
            font.pointSize: 48
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }

        // TODO: Make this a global hotkey active even when HotkeysView is not visible...
        Shortcut {
            sequence: "Ctrl+Alt+Shift+" + model.hotkeyNumber
            onActivated: {
                errorDialog.text = "TODO: Ctrl+Alt+Shift+" + model.hotkeyNumber + " pressed; Launch APP " + model.name + " on " + model.pc
                errorDialog.open()
            }
        }

        Label {
            id: appNameText
            text: model.name

            width: parent.width
            bottomPadding: 4
            anchors.bottom: parent.bottom
            font.pointSize: 22
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }

    }

    ErrorMessageDialog {
        id: errorDialog
    }

    NavigableDialog {
        id: hotkeyDialog
        property string label: qsTr("Hotkey:")

        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {

        }

        onClosed: {

        }

        onAccepted: {

        }

        ColumnLayout {
            Label {
                text: hotkeyDialog.label
                font.bold: true
            }

            TextField {

            }
        }
    }
}
