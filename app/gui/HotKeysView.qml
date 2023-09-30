import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2

CenteredGridView {

    id: hotkeysGrid
    focus: true
    activeFocusOnTab: true
    topMargin: 20
    bottomMargin: 5
    cellWidth: 310; cellHeight: 330;
    objectName: qsTr("Hotkeys")

    Item {

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
