import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2

import StreamingPreferences 1.0

import SdlGamepadKeyNavigation 1.0

import HotkeyManager 1.0

CenteredGridView {
    property string tag: "HotkeyView"

    id: hotkeyGrid
    focus: true
    activeFocusOnTab: true
    topMargin: 20
    bottomMargin: 5
    cellWidth: 310; cellHeight: 330;
    objectName: qsTr("Hotkeys")

    Component.onCompleted: {
        // Don't show any highlighted item until interacting with them.
        // We do this here instead of onActivated to avoid losing the user's
        // selection when backing out of a different page of the app.
        // NOTE:(pv) Sometimes logs `Object or context destroyed during incubation`.
        currentIndex = -1
    }

    // Note: Any initialization done here that is critical for streaming must
    // also be done in CliStartStreamSegue.qml, since this code does not run
    // for command-line initiated streams.
    StackView.onActivated: {
        // This is a bit of a hack to do this here as opposed to main.qml, but
        // we need it enabled before calling getConnectedGamepads() and PcView
        // is never destroyed, so it should be okay.
        SdlGamepadKeyNavigation.enable()

        // Highlight the first item if a gamepad is connected
        if (currentIndex == -1 && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            currentIndex = 0
        }
    }

    model: hotkeyModel

    delegate: NavigableItemDelegate {
        width: 220; height: 287;
        grid: hotkeyGrid

        property alias hotkeyContextMenu: hotkeyContextMenuLoader.item

        Image {
            //
            // Same/similar as AppView.appIcon...
            //
            property bool isPlaceholder: false

            id: appIcon
            anchors.horizontalCenter: parent.horizontalCenter
            y: 10
            source: model.appBoxArt

            onSourceSizeChanged: {
                // Nearly all of Nvidia's official box art does not match the dimensions of placeholder
                // images, however the one known exception is Overcooked. Therefore, we only execute
                // the image size checks if this is not an app collector game. We know the officially
                // supported games all have box art, so this check is not required.
                if (!model.isAppCollectorGame &&
                    ((sourceSize.width == 130 && sourceSize.height == 180) || // GFE 2.0 placeholder image
                     (sourceSize.width == 628 && sourceSize.height == 888) || // GFE 3.0 placeholder image
                     (sourceSize.width == 200 && sourceSize.height == 266)))  // Our no_app_image.png
                {
                    isPlaceholder = true
                }
                else
                {
                    isPlaceholder = false
                }

                width = 200
                height = 267
            }

            // Display a tooltip with the full name if it's truncated
            ToolTip.text: model.appName
            ToolTip.delay: 1000
            ToolTip.timeout: 5000
            ToolTip.visible: (parent.hovered || parent.highlighted) && (!appNameText || appNameText.truncated)

            //
            // Different than AppView.appIcon...
            //

            visible: model.isComputerOnline && model.isComputerPaired
        }

        Image {
            id: computerIcon
            visible: !(model.isComputerOnline && model.isComputerPaired)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            source: "qrc:/res/desktop_windows-48px.svg"
            sourceSize {
                width: 200
                height: 200
            }
        }

        Image {
            // TODO: Tooltip
            id: stateIcon
            visible: !model.isComputerStatusUnknown && !(model.isComputerOnline && model.isComputerPaired)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -40
            source: !model.isComputerOnline ? "qrc:/res/warning_FILL1_wght300_GRAD200_opsz24.svg" : "qrc:/res/baseline-lock-24px.svg"
            sourceSize {
                width: 40
                height: 40
            }
        }

        BusyIndicator {
            id: statusUnknownSpinner
            visible: model.isComputerStatusUnknown
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -40
            width: 40
            height: 40
        }

        Label {
            id: computerNameText
            visible: true // always
            text: model.computerName

            width: parent.width
            anchors.top: parent.top
            anchors.topMargin: 10
            font.pointSize: 22
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }

        Label {
            id: appNameText
            // The boxart has the app name in it, so this is redundant when the computer is online and paired.
            visible: !(model.isComputerOnline && model.isComputerPaired)
            text: model.appName

            anchors.centerIn: parent

            font.pointSize: 22
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }

        Label {
            text: model.hotkeyNumber
            visible: true // always
            style: Text.Outline
            styleColor: "black"

            bottomPadding: 4
            font.pointSize: 36
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight

            ToolTip.text: qsTr("Ctrl+Alt+Shift+%1").arg(model.hotkeyNumber)
            ToolTip.delay: 1000
            ToolTip.timeout: 3000
            ToolTip.visible: hovered
        }

        onClicked: {
            // Only allow clicking on the box art for non-running games.
            // For running games, buttons will appear to resume or quit which
            // will handle starting the game and clicks on the box art will
            // be ignored.
            if (!model.isAppRunning) {
                launchApp(model.computerName, model.appName)
            } else {
                log(tag, "app is already running")
            }
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

        Keys.onReturnPressed: {
            // Open the app context menu if activated via the gamepad or keyboard
            // for running games. If the game isn't running, the above onClicked
            // method will handle the launch.
            if (model.isAppRunning) {
                // This will be keyboard/gamepad driven so use
                // open() instead of popup()
                hotkeyContextMenu.open()
            }
        }

        Keys.onEnterPressed: {
            // Open the app context menu if activated via the gamepad or keyboard
            // for running games. If the game isn't running, the above onClicked
            // method will handle the launch.
            if (model.isAppRunning) {
                // This will be keyboard/gamepad driven so use
                // open() instead of popup()
                hotkeyContextMenu.open()
            }
        }

        Keys.onDeletePressed: {
            hotkeyDelete(model)
        }

        Loader {
            id: hotkeyContextMenuLoader
            asynchronous: true
            sourceComponent: NavigableMenu {
                id: hotkeyContextMenu
                MenuItem {
                    text: qsTr("PC Status: %1").arg(model.isComputerOnline ? qsTr("Online") : qsTr("Offline"))
                    font.bold: true
                    enabled: false
                }
                NavigableMenuItem {
                    parentMenu: hotkeyContextMenu
                    text: model.isAppRunning ? qsTr("Resume Game") : qsTr("Launch Game")
                    onTriggered: launchApp(model.computerName, model.appName)
                }
                // TODO:(pv) drag and drop (that qml is admittedly a bit advanced for me)
            }
        }
    }

    function hotkeyDelete(model) {
        deleteHotkeyDialog.computerName = model.computerName
        deleteHotkeyDialog.appName = model.appName
        deleteHotkeyDialog.open()
    }

    NavigableMessageDialog {
        id: deleteHotkeyDialog
        // don't allow edits to the rest of the window while open
        property string computerName : ""
        property string appName : ""
        text: qsTr("Are you sure you want to remove Hotkey \"%1\" \"%2\"?").arg(computerName).arg(appName)
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            HotkeyManager.remove(computerName, appName)
            displayToast(qsTr("Hotkey \"%1\" \"%2\" removed").arg(computerName).arg(appName))
        }
    }
}
