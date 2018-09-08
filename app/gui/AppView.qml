import QtQuick 2.9
import QtQuick.Dialogs 1.2
import QtQuick.Controls 2.2

import AppModel 1.0

import ComputerManager 1.0

GridView {
    property int computerIndex
    property AppModel appModel : createModel()

    id: appGrid
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

    onVisibleChanged: {
        if (visible) {
            appModel.computerLost.connect(computerLost)
        }
        else {
            appModel.computerLost.disconnect(computerLost)
        }
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import AppModel 1.0; AppModel {}', parent, '')
        model.initialize(ComputerManager, computerIndex)
        return model
    }

    model: appModel

    delegate: ItemDelegate {
        width: 200; height: 335;

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
                // Right click
                appContextMenu.open()
            }
        }

        Menu {
            id: appContextMenu
            MenuItem {
                text: model.running ? "Resume Game" : "Launch Game"
                onTriggered: {
                    appContextMenu.close()
                    launchOrResumeSelectedApp()
                }
                height: visible ? implicitHeight : 0
            }
            MenuItem {
                text: "Quit Game"
                onTriggered: {
                    quitAppDialog.appName = appModel.getRunningAppName()
                    quitAppDialog.segueToStream = false
                    quitAppDialog.open()
                }
                visible: model.running
                height: visible ? implicitHeight : 0
            }
        }
    }

    MessageDialog {
        id: quitAppDialog
        modality:Qt.WindowModal
        property string appName : "";
        property bool segueToStream : false
        text:"Are you sure you want to quit " + appName +"? Any unsaved progress will be lost."
        standardButtons: StandardButton.Yes | StandardButton.No
        onYes: {
            var component = Qt.createComponent("QuitSegue.qml")
            var params = {"appName": appName}
            if (segueToStream) {
                // Store the session and app name if we're going to stream after
                // successfully quitting the old app.
                params.nextAppName = model.name
                params.nextSession = appModel.createSessionForApp(index)
            }
            else {
                params.nextAppName = null
                params.nextSession = null
            }

            stackView.push(component.createObject(stackView, params))

            // Trigger the quit after pushing the quit segue on screen
            appModel.quitRunningApp()
        }
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
