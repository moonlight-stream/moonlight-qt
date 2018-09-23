import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2

import ComputerModel 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0

GridView {
    property ComputerModel computerModel : createModel()

    id: pcGrid
    focus: true
    activeFocusOnTab: true
    anchors.fill: parent
    anchors.leftMargin: (parent.width % (cellWidth + anchors.rightMargin)) / 2
    anchors.topMargin: 20
    anchors.rightMargin: 5
    anchors.bottomMargin: 5
    cellWidth: 350; cellHeight: 350;
    objectName: "Computers"

    // The StackView will trigger a visibility change when
    // we're pushed onto it, causing our onVisibleChanged
    // routine to run, but only if we start as invisible
    visible: false

    StreamingPreferences {
        id: prefs
    }

    Component.onCompleted: {
        // Setup signals on CM
        ComputerManager.computerAddCompleted.connect(addComplete)

        if (prefs.isRunningWayland()) {
            waylandDialog.open()
        }
        else if (prefs.isWow64()) {
            wow64Dialog.open()
        }
        else if (!prefs.hasAnyHardwareAcceleration()) {
            noHwDecoderDialog.open()
        }
    }

    function pairingComplete(error)
    {
        // Close the PIN dialog
        pairDialog.close()

        // Display a failed dialog if we got an error
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.open()
        }
    }

    function addComplete(success)
    {
        if (!success) {
            errorDialog.text = "Unable to connect to the specified PC. Click the Help button for possible solutions."
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

    model: computerModel

    delegate: NavigableItemDelegate {
        width: 300; height: 300;
        grid: pcGrid

        Image {
            id: pcIcon
            anchors.horizontalCenter: parent.horizontalCenter
            source: {
                model.addPc ? "qrc:/res/ic_add_to_queue_white_48px.svg" : "qrc:/res/ic_tv_white_48px.svg"
            }
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
            visible: !model.addPc && !model.statusUnknown && (!model.online || !model.paired)
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
            visible: !model.addPc && model.statusUnknown
        }

        Text {
            id: pcNameText
            text: model.name
            color: "white"

            width: parent.width
            anchors.top: pcIcon.bottom
            font.pointSize: 36
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Menu {
            id: pcContextMenu
            MenuItem {
                text: "Wake PC"
                onTriggered: computerModel.wakeComputer(index)
                visible: !model.addPc && !model.online && model.wakeable
                height: visible ? implicitHeight : 0
            }
            MenuItem {
                text: "Delete PC"
                onTriggered: {
                    deletePcDialog.pcIndex = index
                    // get confirmation first, actual closing is called from the dialog
                    deletePcDialog.open()
                }
            }
        }

        onClicked: {
            if (model.addPc) {
                addPcDialog.open()
            }
            else if (model.online) {
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
                        errorDialog.text = "This PC is currently busy. Make sure to quit any running games and try again."
                        errorDialog.open()
                    }
                }
            } else if (!model.online) {
                pcContextMenu.open()
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton;
            onClicked: {
                // right click
                if (!model.addPc) { // but only for actual PCs, not the add-pc option
                    pcContextMenu.open()
                }
            }
        }
    }

    MessageDialog {
        id: errorDialog
        // don't allow edits to the rest of the window while open
        modality:Qt.WindowModal
        icon: StandardIcon.Critical
        standardButtons: StandardButton.Ok | StandardButton.Help
        onHelp: {
            // Using Setup-Guide here instead of Troubleshooting because it's likely that users
            // will arrive here by forgetting to enable GameStream or not forwarding ports.
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide");
        }
    }

    MessageDialog {
        id: noHwDecoderDialog
        modality:Qt.WindowModal
        icon: StandardIcon.Warning
        standardButtons: StandardButton.Ok | StandardButton.Help
        text: "No functioning hardware accelerated H.264 video decoder was detected by Moonlight. " +
              "Your streaming performance may be severely degraded in this configuration. " +
              "Click the Help button for more information on solving this problem."
        onHelp: {
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems");
        }
    }

    MessageDialog {
        id: waylandDialog
        modality:Qt.WindowModal
        icon: StandardIcon.Warning
        standardButtons: StandardButton.Ok | StandardButton.Help
        text: "Moonlight does not support hardware acceleration on Wayland. Continuing on Wayland may result in poor streaming performance. " +
              "Please switch to an X session for optimal performance."
        onHelp: {
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems");
        }
    }

    MessageDialog {
        id: wow64Dialog
        modality:Qt.WindowModal
        icon: StandardIcon.Warning
        standardButtons: StandardButton.Ok | StandardButton.Cancel
        text: "This PC is running a 64-bit version of Windows. Please download the x64 version of Moonlight for the best streaming performance."
        onAccepted: {
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-qt/releases");
        }
    }

    MessageDialog {
        id: pairDialog
        // don't allow edits to the rest of the window while open
        modality:Qt.WindowModal
        property string pin : "0000"
        text:"Please enter " + pin + " on your GameStream PC. This dialog will close when pairing is completed."
        standardButtons: StandardButton.Cancel
        onRejected: {
            // FIXME: We should interrupt pairing here
        }
    }

    MessageDialog {
        id: deletePcDialog
        // don't allow edits to the rest of the window while open
        modality:Qt.WindowModal
        property int pcIndex : -1;
        text:"Are you sure you want to remove this PC?"
        standardButtons: StandardButton.Yes | StandardButton.No
        onYes: {
            console.log("deleting PC pairing for PC at index: " + pcIndex)
            computerModel.deleteComputer(pcIndex);
        }
    }

    Dialog {
        id: addPcDialog
        property string label: "Enter the IP address of your GameStream PC"
        property string hint: "192.168.1.100"
        property alias editText : editTextItem

        standardButtons: StandardButton.Ok | StandardButton.Cancel
        onVisibleChanged: {
            editTextItem.focus = true
            editTextItem.selectAll()
        }
        onAccepted: {
            ComputerManager.addNewHost(editText.text, false)
        }

        ColumnLayout {
            Text {
                id: addPcDialogLabel
                text: addPcDialog.label
            }
            Rectangle {
                implicitWidth: parent.parent.width
                height: editTextItem.height

                TextInput {
                    id: editTextItem
                    inputMethodHints: Qt.ImhPreferUppercase
                    text: addPcDialog.hint
                }
            }
        }
    }

    ScrollBar.vertical: ScrollBar {
        parent: pcGrid.parent
        anchors {
            top: pcGrid.top
            left: pcGrid.right
            bottom: pcGrid.bottom

            leftMargin: -10
        }
    }
}
