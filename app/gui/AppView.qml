import QtQuick 2.9
import QtQuick.Dialogs 1.2
import QtQuick.Controls 2.2

import AppModel 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0

GridView {
    property int computerIndex
    property AppModel appModel : createModel()

    id: appGrid
    focus: true
    activeFocusOnTab: true
    anchors.fill: parent
    anchors.leftMargin: (parent.width % (cellWidth + anchors.rightMargin)) / 2
    anchors.topMargin: 20
    anchors.rightMargin: 5
    anchors.bottomMargin: 5
    cellWidth: 225; cellHeight: 385;

    // The StackView will trigger a visibility change when
    // we're pushed onto it, causing our onVisibleChanged
    // routine to run, but only if we start as invisible
    visible: false

    function computerLost()
    {
        // Go back to the PC view on PC loss
        stackView.pop()
    }

    Component.onCompleted: {
        // Don't show any highlighted item until interacting with them
        currentIndex = -1
    }

    SdlGamepadKeyNavigation {
        id: gamepadKeyNav
    }

    onVisibleChanged: {
        if (visible) {
            appModel.computerLost.connect(computerLost)
            gamepadKeyNav.enable()
        }
        else {
            appModel.computerLost.disconnect(computerLost)
            gamepadKeyNav.disable()
        }
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import AppModel 1.0; AppModel {}', parent, '')
        model.initialize(ComputerManager, computerIndex)
        return model
    }

    model: appModel

    delegate: NavigableItemDelegate {
        width: 200; height: 335;
        grid: appGrid

        Image {
            id: appIcon
            anchors.horizontalCenter: parent.horizontalCenter;
            y: 20
            source: model.boxart
            sourceSize {
                width: 150
                height: 200
            }
        }

        Image {
            id: runningIcon
            anchors.centerIn: appIcon
            visible: model.running
            source: "qrc:/res/baseline-play_circle_filled_white-48px.svg"
            sourceSize {
                width: 75
                height: 75
            }
        }

        Text {
            id: appNameText
            text: model.name
            color: "white"
            width: parent.width
            height: 125
            anchors.top: appIcon.bottom
            font.pointSize: 22
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }

        function launchOrResumeSelectedApp()
        {
            var runningIndex = appModel.getRunningAppIndex()
            if (runningIndex >= 0 && runningIndex !== index) {
                quitAppDialog.appName = appModel.getRunningAppName()
                quitAppDialog.segueToStream = true
                quitAppDialog.nextAppName = model.name
                quitAppDialog.nextAppIndex = index
                quitAppDialog.open()
                return
            }

            var component = Qt.createComponent("StreamSegue.qml")
            var segue = component.createObject(stackView, {"appName": model.name, "session": appModel.createSessionForApp(index)})
            stackView.push(segue)
        }

        onClicked: {
            // Nothing is running or this app is running
            launchOrResumeSelectedApp()
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: {
                // popup() ensures the menu appears under the mouse cursor
                appContextMenu.popup()
            }
        }

        Keys.onMenuPressed: {
            // We must use open() here so the menu is positioned on
            // the ItemDelegate and not where the mouse cursor is
            appContextMenu.open()
        }

        Menu {
            id: appContextMenu
            NavigableMenuItem {
                text: model.running ? "Resume Game" : "Launch Game"
                onTriggered: {
                    appContextMenu.close()
                    launchOrResumeSelectedApp()
                }
            }
            NavigableMenuItem {
                text: "Quit Game"
                onTriggered: {
                    quitAppDialog.appName = appModel.getRunningAppName()
                    quitAppDialog.segueToStream = false
                    quitAppDialog.open()
                }
                visible: model.running
            }
        }
    }

    MessageDialog {
        id: quitAppDialog
        modality:Qt.WindowModal
        property string appName : ""
        property bool segueToStream : false
        property string nextAppName: ""
        property int nextAppIndex: 0
        text:"Are you sure you want to quit " + appName +"? Any unsaved progress will be lost."
        standardButtons: StandardButton.Yes | StandardButton.No

        function quitApp() {
            var component = Qt.createComponent("QuitSegue.qml")
            var params = {"appName": appName}
            if (segueToStream) {
                // Store the session and app name if we're going to stream after
                // successfully quitting the old app.
                params.nextAppName = nextAppName
                params.nextSession = appModel.createSessionForApp(nextAppIndex)
            }
            else {
                params.nextAppName = null
                params.nextSession = null
            }

            stackView.push(component.createObject(stackView, params))

            // Trigger the quit after pushing the quit segue on screen
            appModel.quitRunningApp()
        }

        onYes: quitApp()

        // For keyboard/gamepad navigation
        onAccepted: quitApp()
    }

    ScrollBar.vertical: ScrollBar {
        parent: appGrid.parent
        anchors {
            top: appGrid.top
            left: appGrid.right
            bottom: appGrid.bottom

            leftMargin: -10
        }
    }
}
