import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import BluetoothDeviceModel 1.0

import BluetoothManager 1.0
import SdlGamepadKeyNavigation 1.0

Item {
    id: bluetoothView
    objectName: qsTr("Bluetooth")
    focus: true

    property BluetoothDeviceModel deviceModel : createModel()

    StackView.onActivated: {
        BluetoothManager.operationFailed.connect(handleOperationFailed)
        BluetoothManager.agentRequest.connect(handleAgentRequest)
        BluetoothManager.agentRequestCancelled.connect(handleAgentRequestCancelled)

        // Highlight first device for gamepad users
        if (deviceList.currentIndex === -1 && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            deviceList.currentIndex = 0
        }
    }

    StackView.onDeactivating: {
        BluetoothManager.operationFailed.disconnect(handleOperationFailed)
        BluetoothManager.agentRequest.disconnect(handleAgentRequest)
        BluetoothManager.agentRequestCancelled.disconnect(handleAgentRequestCancelled)

        // Stop scanning when navigating away
        BluetoothManager.stopDiscovery()
    }

    function handleOperationFailed(deviceName, operation, error)
    {
        errorDialog.text = qsTr("Moonlight was unable to %1 %2.").arg(operation).arg(deviceName) + "\n\n" + error
        errorDialog.open()
    }

    function handleAgentRequest(type, devicePath, deviceName, detail)
    {
        if (type === BluetoothManager.DisplayCode) {
            displayCodeDialog.deviceName = deviceName
            displayCodeDialog.code = detail
            displayCodeDialog.open()
            return
        }

        if (type === BluetoothManager.EnterPinCode || type === BluetoothManager.EnterPasskey) {
            codeEntryDialog.deviceName = deviceName
            codeEntryDialog.requestType = type
            codeEntryDialog.open()
            return
        }

        confirmDialog.deviceName = deviceName
        confirmDialog.passkey = (type === BluetoothManager.ConfirmPasskey) ? detail : ""
        confirmDialog.open()
    }

    function handleAgentRequestCancelled()
    {
        confirmDialog.close()
        codeEntryDialog.close()
        displayCodeDialog.close()
    }

    // Helper functions
    // The action the user likely wants
    function activateDevice(device)
    {
        if (!device.paired) {
            BluetoothManager.pairDevice(device.path)
        }
        else if (device.connected) {
            BluetoothManager.disconnectDevice(device.path)
        }
        else {
            BluetoothManager.connectDevice(device.path)
        }
    }

    function createModel()
    {
        // StackView.push() reparents us after initialization
        var model = Qt.createQmlObject('import BluetoothDeviceModel 1.0; BluetoothDeviceModel {}', bluetoothView, '')
        model.initialize(BluetoothManager)
        return model
    }


    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        RowLayout {
            Layout.fillWidth: true
            spacing: 15

            Label {
                text: BluetoothManager.adapterName ? BluetoothManager.adapterName : qsTr("Bluetooth")
                font.pointSize: 16
                elide: Label.ElideRight
                Layout.fillWidth: true
            }

            Switch {
                text: qsTr("Bluetooth on")
                enabled: BluetoothManager.available
                checked: BluetoothManager.powered
                activeFocusOnTab: true

                // Restore binding and let BlueZ confirm
                function requestPowerChange(desiredState) {
                    checked = Qt.binding(function() { return BluetoothManager.powered })
                    BluetoothManager.setPowered(desiredState)
                }

                onClicked: requestPowerChange(checked)

                Keys.onReturnPressed: requestPowerChange(!BluetoothManager.powered)
                Keys.onEnterPressed: requestPowerChange(!BluetoothManager.powered)
                Keys.onDownPressed: deviceList.forceActiveFocus(Qt.TabFocus)
            }

            BusyIndicator {
                implicitWidth: 30
                implicitHeight: 30
                visible: BluetoothManager.discovering
                running: visible
            }

            Button {
                text: BluetoothManager.discovering ? qsTr("Stop scanning") : qsTr("Scan for devices")
                enabled: BluetoothManager.available && BluetoothManager.powered
                activeFocusOnTab: true

                onClicked: {
                    if (BluetoothManager.discovering) {
                        BluetoothManager.stopDiscovery()
                    }
                    else {
                        BluetoothManager.startDiscovery()
                    }
                }

                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                Keys.onDownPressed: deviceList.forceActiveFocus(Qt.TabFocus)
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !BluetoothManager.available || !BluetoothManager.powered
            wrapMode: Text.Wrap
            font.pointSize: 12
            text: !BluetoothManager.available ? BluetoothManager.statusMessage
                                              : qsTr("Bluetooth is turned off. Turn it on to see nearby devices.")
        }

        ListView {
            id: deviceList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            activeFocusOnTab: true
            keyNavigationEnabled: true
            boundsBehavior: Flickable.OvershootBounds

            model: deviceModel

            ScrollBar.vertical: ScrollBar {}

            Component.onCompleted: {
                // Don't highlight until the user interacts
                currentIndex = -1
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pointSize: 14
                visible: deviceList.count === 0 && BluetoothManager.available && BluetoothManager.powered
                text: BluetoothManager.discovering ? qsTr("Searching for nearby Bluetooth devices…")
                                                   : qsTr("No Bluetooth devices yet. Press 'Scan for devices' and put your device into pairing mode.")
            }

            delegate: ItemDelegate {
                width: deviceList.width
                height: 76

                highlighted: deviceList.activeFocus && ListView.isCurrentItem

                property alias deviceContextMenu : deviceContextMenuLoader.item

                Image {
                    id: deviceIcon
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    source: "qrc:/res/bluetooth.svg"
                    opacity: model.connected ? 1.0 : 0.4
                    sourceSize {
                        width: 40
                        height: 40
                    }
                }

                Column {
                    anchors.left: deviceIcon.right
                    anchors.leftMargin: 15
                    anchors.right: deviceBusyIndicator.left
                    anchors.rightMargin: 15
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Label {
                        width: parent.width
                        text: model.name
                        font.pointSize: 14
                        elide: Text.ElideRight
                    }

                    Label {
                        width: parent.width
                        text: model.address ? (model.statusText + "  •  " + model.address) : model.statusText
                        font.pointSize: 10
                        opacity: 0.7
                        elide: Text.ElideRight
                    }
                }

                BusyIndicator {
                    id: deviceBusyIndicator
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    implicitWidth: 32
                    implicitHeight: 32
                    visible: model.busy
                    running: visible
                }

                Loader {
                    id: deviceContextMenuLoader
                    asynchronous: true
                    sourceComponent: NavigableMenu {
                        id: deviceContextMenu
                        initiator: deviceContextMenuLoader.parent

                        MenuItem {
                            text: model.name
                            font.bold: true
                            enabled: false
                        }
                        NavigableMenuItem {
                            text: qsTr("Pair")
                            visible: !model.paired
                            onTriggered: BluetoothManager.pairDevice(model.path)
                        }
                        NavigableMenuItem {
                            text: qsTr("Connect")
                            visible: model.paired && !model.connected
                            onTriggered: BluetoothManager.connectDevice(model.path)
                        }
                        NavigableMenuItem {
                            text: qsTr("Disconnect")
                            visible: model.connected
                            onTriggered: BluetoothManager.disconnectDevice(model.path)
                        }
                        NavigableMenuItem {
                            text: qsTr("Trust this device")
                            visible: model.paired && !model.trusted
                            onTriggered: BluetoothManager.setDeviceTrusted(model.path, true)
                        }
                        NavigableMenuItem {
                            text: qsTr("Stop trusting this device")
                            visible: model.paired && model.trusted
                            onTriggered: BluetoothManager.setDeviceTrusted(model.path, false)
                        }
                        NavigableMenuItem {
                            text: qsTr("Cancel pairing")
                            visible: model.busy && !model.paired
                            onTriggered: BluetoothManager.cancelPairing(model.path)
                        }
                        NavigableMenuItem {
                            text: qsTr("Remove this device")
                            visible: model.paired
                            onTriggered: {
                                removeDeviceDialog.devicePath = model.path
                                removeDeviceDialog.deviceName = model.name
                                removeDeviceDialog.open()
                            }
                        }
                    }
                }

                onClicked: activateDevice(model)

                onPressAndHold: {
                    // popup() positions the menu under cursor
                    if (deviceContextMenu.popup) {
                        deviceContextMenu.popup()
                    }
                    else {
                        // Qt 5.9 doesn't have popup()
                        deviceContextMenu.open()
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: {
                        parent.pressAndHold()
                    }
                }

                Keys.onMenuPressed: {
                    // open() positions the menu on delegate
                    deviceContextMenu.open()
                }

                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()

                Keys.onUpPressed: {
                    if (deviceList.currentIndex === 0) {
                        // Move focus to the toolbar
                        deviceList.nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
                    }
                    else {
                        deviceList.decrementCurrentIndex()
                    }
                }

                Keys.onDownPressed: {
                    deviceList.incrementCurrentIndex()
                }

                Keys.onDeletePressed: {
                    if (model.paired) {
                        removeDeviceDialog.devicePath = model.path
                        removeDeviceDialog.deviceName = model.name
                        removeDeviceDialog.open()
                    }
                }
            }
        }
    }

    // ErrorMessageDialog's Help link is irrelevant here
    NavigableMessageDialog {
        id: errorDialog
        standardButtons: Dialog.Ok
        imageSrc: "qrc:/res/baseline-error_outline-24px.svg"
    }

    NavigableMessageDialog {
        id: removeDeviceDialog
        property string devicePath : ""
        property string deviceName : ""

        standardButtons: Dialog.Yes | Dialog.No
        text: qsTr("Are you sure you want to remove '%1'? You will have to pair it again to use it.").arg(deviceName)

        onAccepted: BluetoothManager.removeDevice(devicePath)
    }

    // Shown when BlueZ asks for approval
    NavigableMessageDialog {
        id: confirmDialog
        property string deviceName : ""
        property string passkey : ""

        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.Yes | Dialog.No
        imageSrc: "qrc:/res/baseline-help_outline-24px.svg"
        text: passkey ? (qsTr("Confirm that '%1' is showing this code:").arg(deviceName) + "\n\n" + passkey)
                      : qsTr("Allow '%1' to pair with this computer?").arg(deviceName)

        onAccepted: BluetoothManager.respondToAgentRequest(true)
        onRejected: BluetoothManager.respondToAgentRequest(false)
    }

    // Code must be typed on device
    NavigableMessageDialog {
        id: displayCodeDialog
        property string deviceName : ""
        property string code : ""

        standardButtons: Dialog.Ok
        imageSrc: "qrc:/res/baseline-help_outline-24px.svg"
        text: qsTr("Enter this code on '%1' to finish pairing:").arg(deviceName) + "\n\n" + code
    }

    // Shown when BlueZ needs a code
    NavigableDialog {
        id: codeEntryDialog
        property string deviceName : ""
        property int requestType : BluetoothManager.EnterPinCode

        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {
            // Force focus for keyboard navigation
            codeEntryText.forceActiveFocus()
        }

        onClosed: {
            codeEntryText.clear()
        }

        onAccepted: BluetoothManager.respondToAgentRequest(true, codeEntryText.text)
        onRejected: BluetoothManager.respondToAgentRequest(false, "")

        ColumnLayout {
            Label {
                text: (codeEntryDialog.requestType === BluetoothManager.EnterPasskey)
                          ? qsTr("Enter the passkey shown on '%1':").arg(codeEntryDialog.deviceName)
                          : qsTr("Enter the PIN code for '%1':").arg(codeEntryDialog.deviceName)
                font.bold: true
                wrapMode: Text.Wrap
                Layout.maximumWidth: 400
            }

            TextField {
                id: codeEntryText
                Layout.fillWidth: true
                focus: true

                // Passkeys numeric, PIN codes alphanumeric
                inputMethodHints: (codeEntryDialog.requestType === BluetoothManager.EnterPasskey)
                                      ? Qt.ImhDigitsOnly : Qt.ImhNone

                Keys.onReturnPressed: codeEntryDialog.accept()
                Keys.onEnterPressed: codeEntryDialog.accept()
            }
        }
    }
}
