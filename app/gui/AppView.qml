import QtQuick 2.9
import QtQuick.Controls 2.2

import AppModel 1.0

import ComputerManager 1.0

GridView {
    property int computerIndex
    property AppModel appModel : createModel()

    anchors.fill: parent
    anchors.leftMargin: 5
    anchors.topMargin: 5
    anchors.rightMargin: 5
    anchors.bottomMargin: 5
    cellWidth: 225; cellHeight: 350;
    focus: true

    Component.onCompleted: {
        // Start polling when this view is shown
        ComputerManager.startPolling()
    }

    Component.onDestruction: {
        // Stop polling when this view is destroyed
        ComputerManager.stopPollingAsync()
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
                var session = appModel.createSessionForApp(index)

                // Don't poll while the stream is running
                ComputerManager.stopPollingAsync()

                // Run the streaming session to completion
                session.exec()

                // Start polling again
                ComputerManager.startPolling()
            }
        }
    }
}
