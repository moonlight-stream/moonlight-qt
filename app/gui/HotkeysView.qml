import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2

import StreamingPreferences 1.0

import HotkeyModel 1.0
import HotkeyManager 1.0

CenteredGridView {
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
            id: computerNameText
            text: model.computerName

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

        Label {
            id: appNameText
            text: model.appName

            width: parent.width
            bottomPadding: 4
            anchors.bottom: parent.bottom
            font.pointSize: 22
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }

        onClicked: {
            launchApp(model.computerName, model.appName)
        }

        onPressAndHold: {
            // popup() ensures the menu appears under the mouse cursor
            if (hotkeyContextMenu.popup) {
                hotkeyContextMenu.popup()
            }
            else {
                // Qt 5.9 doesn't have popup()
                hotkeyContextMenu.open()
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton;
            onClicked: {
                parent.onPressAndHold()
            }
        }

        Keys.onMenuPressed: {
            // We must use open() here so the menu is positioned on
            // the ItemDelegate and not where the mouse cursor is
            hotkeyContextMenu.open()
        }

        Loader {
            id: hotkeyContextMenuLoader
            asynchronous: true
            sourceComponent: NavigableMenu {
                id: hotkeyContextMenu
                NavigableMenuItem {
                    parentMenu: hotkeyContextMenu
                    text: qsTr("Launch Game")
                    onTriggered: launchApp(model.computerName, model.appName)
                }
            }
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
