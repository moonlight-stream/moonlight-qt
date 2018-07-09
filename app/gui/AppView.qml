import QtQuick 2.9
import QtQuick.Controls 2.2

import AppModel 1.0

import ComputerManager 1.0

GridView {
    property int computerIndex
    property AppModel appModel : createModel()

    anchors.fill: parent
    anchors.leftMargin: (parent.width % (cellWidth + anchors.rightMargin)) / 2
    anchors.topMargin: 5
    anchors.rightMargin: 5
    anchors.bottomMargin: 5
    cellWidth: 225; cellHeight: 350;
    focus: true

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

        Component.onCompleted: {
            if (model.running) {
                appNameText.text = "<font color=\"green\">Running</font><font color=\"white\"> - </font>" + appNameText.text
            }
        }

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
            text: model.name
            color: "white"

            width: parent.width
            height: 100
            anchors.top: appIcon.bottom
            font.pointSize: 26
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                // TODO: Check if a different game is running

                var component = Qt.createComponent("StreamSegue.qml")
                var segue = component.createObject(stackView)
                segue.appname = model.name
                segue.session = appModel.createSessionForApp(index)
                stackView.push(segue)
            }
        }
    }
}
