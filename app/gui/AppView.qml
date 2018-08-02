import QtQuick 2.9
import QtQuick.Dialogs 1.3
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
    cellWidth: 225; cellHeight: 350;
    focus: true

    // Cache delegates for 1000px in both directions to improve
    // scrolling and resizing performance
    cacheBuffer: 1000

    // The StackView will trigger a visibility change when
    // we're pushed onto it, causing our onVisibleChanged
    // routine to run, but only if we start as invisible
    visible: false

    onVisibleChanged: {
        if (visible) {
            // Start polling when this view is shown
            ComputerManager.startPolling()
        }
        else {
            // Stop polling when this view is not on top
            ComputerManager.stopPollingAsync()
        }
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import AppModel 1.0; AppModel {}', parent, '')
        model.initialize(ComputerManager, computerIndex)
        return model
    }

    model: appModel

    delegate: Item {
        width: 200; height: 300;

        Image {
            id: appIcon
            anchors.horizontalCenter: parent.horizontalCenter;
            source: model.boxart
            sourceSize {
                width: 150
                height: 200
            }
        }

        Text {
            id: appNameText
            text: {
                if (model.running) {
                    return "<font color=\"green\">Running</font><font color=\"white\"> - </font>" + model.name
                }
                else {
                    return model.name
                }
            }

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
            var segue = component.createObject(stackView)
            segue.appName = model.name
            segue.session = appModel.createSessionForApp(index)
            stackView.push(segue)
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
                var segue = component.createObject(stackView)
                segue.appName = appName
                if (segueToStream) {
                    // Store the session and app name if we're going to stream after
                    // successfully quitting the old app.
                    segue.nextAppName = model.name
                    segue.nextSession = appModel.createSessionForApp(index)
                }
                else {
                    segue.nextAppName = null
                    segue.nextSession = null
                }

                stackView.push(segue)

                // Trigger the quit after pushing the quit segue on screen
                appModel.quitRunningApp()
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: {
                if (mouse.button === Qt.LeftButton) {
                    // Nothing is running or this app is running
                    launchOrResumeSelectedApp()
                }
                else {
                    // Right click
                    appContextMenu.open()
                }
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
