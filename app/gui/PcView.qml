import QtQuick 2.9
import QtQuick.Controls 2.2

import ComputerModel 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0

CenteredGridView {
    property ComputerModel computerModel : createModel()

    id: pcGrid
    focus: true
    activeFocusOnTab: true
    topMargin: 20
    bottomMargin: 5
    cellWidth: 310; cellHeight: 350;
    objectName: "Computers"

    Component.onCompleted: {
        // Don't show any highlighted item until interacting with them.
        // We do this here instead of onActivated to avoid losing the user's
        // selection when backing out of a different page of the app.
        currentIndex = -1
    }

    StackView.onActivated: {
        // Setup signals on CM
        ComputerManager.computerAddCompleted.connect(addComplete)
        gamesBtn.checked = true
        backgroundImage.visible = true
    }

    StackView.onDeactivating: {
        ComputerManager.computerAddCompleted.disconnect(addComplete)
        backgroundImage.visible = false
    }

    function pairingComplete(error)
    {
        // Close the PIN dialog
        pairDialog.close()

        // Display a failed dialog if we got an error
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.helpText = ""
            errorDialog.open()
        }
    }

    function addComplete(success)
    {
        if (!success) {
            errorDialog.text = "Unable to connect to the specified PC."
            errorDialog.helpText = "Click the Help button for possible solutions."
            errorDialog.open()
        }
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', parent, '')
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(pairingComplete)
        return model
    }

    Row {
        anchors.centerIn: parent
        spacing: 5
        visible: pcGrid.count === 0

        BusyIndicator {
            id: searchSpinner
            visible: StreamingPreferences.enableMdns
        }

        Label {
            height: searchSpinner.height
            elide: Label.ElideRight
            text: StreamingPreferences.enableMdns ? "Searching for PCs with NVIDIA GameStream enabled..."
                                                  : "Automatic PC discovery is disabled. Add your PC manually."
            font.pointSize: 20
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }

    model: computerModel

    delegate: NavigableItemDelegate {
        width: 300; height: 300;
        grid: pcGrid

        Image {
            id: pcIcon
            anchors.horizontalCenter: parent.horizontalCenter
            source: "qrc:/res/ic_tv_white_48px.svg"
            sourceSize {
                width: 200
                height: 200
            }
        }

        Image {
            // TODO: Tooltip
            id: stateIcon
            anchors.horizontalCenter: pcIcon.horizontalCenter
            anchors.verticalCenter: pcIcon.verticalCenter
            anchors.verticalCenterOffset: -10
            visible: !model.statusUnknown && (!model.online || !model.paired)
            source: !model.online ? "qrc:/res/baseline-warning-24px.svg" : "qrc:/res/baseline-lock-24px.svg"
            sourceSize {
                width: 75
                height: 75
            }
        }

        BusyIndicator {
            id: statusUnknownSpinner
            anchors.horizontalCenter: pcIcon.horizontalCenter
            anchors.verticalCenter: pcIcon.verticalCenter
            anchors.verticalCenterOffset: -10
            width: 75
            height: 75
            visible: model.statusUnknown
        }

        Label {
            id: pcNameText
            text: model.name

            width: parent.width
            anchors.top: pcIcon.bottom
            font.pointSize: 36
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        NavigableMenu {
            id: pcContextMenu
            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: "Delete PC"
                onTriggered: {
                    deletePcDialog.pcIndex = index
                    // get confirmation first, actual closing is called from the dialog
                    deletePcDialog.open()
                }
            }
            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: "Wake PC"
                onTriggered: computerModel.wakeComputer(index)
                visible: !model.online && model.wakeable
            }
        }

        onClicked: {
            if (model.online) {
                if (model.paired) {
                    // go to game view
                    var component = Qt.createComponent("AppView.qml")
                    var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name})
                    stackView.push(appView)
                }
                else {
                    if (!model.busy) {
                        var pin = ("0000" + Math.floor(Math.random() * 10000)).slice(-4)

                        // Kick off pairing in the background
                        computerModel.pairComputer(index, pin)

                        // Display the pairing dialog
                        pairDialog.pin = pin
                        pairDialog.open()
                    }
                    else {
                        // cannot pair while something is streaming or attempting to pair
                        errorDialog.text = "You cannot pair while a previous session is still running on the host PC. Quit any running games or reboot the host PC, then try pairing again."
                        errorDialog.helpText = ""
                        errorDialog.open()
                    }
                }
            } else if (!model.online) {
                // Using open() here because it may be activated by keyboard
                pcContextMenu.open()
            }
        }

        onPressAndHold: {
            // popup() ensures the menu appears under the mouse cursor
            if (pcContextMenu.popup) {
                pcContextMenu.popup()
            }
            else {
                // Qt 5.9 doesn't have popup()
                pcContextMenu.open()
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
            pcContextMenu.open()
        }
    }

    ErrorMessageDialog {
        id: errorDialog

        // Using Setup-Guide here instead of Troubleshooting because it's likely that users
        // will arrive here by forgetting to enable GameStream or not forwarding ports.
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide"
    }

    NavigableMessageDialog {
        id: pairDialog
        // don't allow edits to the rest of the window while open
        property string pin : "0000"
        text:"Please enter " + pin + " on your GameStream PC. This dialog will close when pairing is completed."
        standardButtons: Dialog.Cancel
        onRejected: {
            // FIXME: We should interrupt pairing here
        }
    }

    NavigableMessageDialog {
        id: deletePcDialog
        // don't allow edits to the rest of the window while open
        property int pcIndex : -1;
        text:"Are you sure you want to remove this PC?"
        standardButtons: Dialog.Yes | Dialog.No

        function deletePc() {
            console.log("deleting PC pairing for PC at index: " + pcIndex)
            computerModel.deleteComputer(pcIndex);
        }

        onAccepted: deletePc()
    }

    ScrollBar.vertical: ScrollBar {
        parent: pcGrid.parent
        anchors {
            top: parent.top
            right: parent.right
            bottom: parent.bottom
        }
    }
}
