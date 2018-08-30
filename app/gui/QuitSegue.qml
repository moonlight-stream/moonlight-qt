import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2

import ComputerManager 1.0
import Session 1.0

Item {
    property string appName
    property Session nextSession : null
    property string nextAppName : ""

    property string stageText : "Quitting " + appName + "..."

    anchors.fill: parent

    // The StackView will trigger a visibility change when
    // we're pushed onto it, causing our onVisibleChanged
    // routine to run, but only if we start as invisible
    visible: false

    function quitAppCompleted(error)
    {
        // Display a failed dialog if we got an error
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.open()
        }

        // Exit this view
        stackView.pop()

        // If we're supposed to launch another game after this, do so now
        if (error === undefined && nextSession !== null) {
            var component = Qt.createComponent("StreamSegue.qml")
            var segue = component.createObject(stackView, {"appName": nextAppName, "session": nextSession})
            stackView.push(segue)
        }
    }

    onVisibleChanged: {
        if (visible) {
            // Connect the quit completion signal
            ComputerManager.quitAppCompleted.connect(quitAppCompleted)
        }
        else {
            // Disconnect the signal
            ComputerManager.quitAppCompleted.disconnect(quitAppCompleted)
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: 5

        BusyIndicator {
            id: stageSpinner
        }

        Label {
            id: stageLabel
            height: stageSpinner.height
            text: stageText
            font.pointSize: 20
            verticalAlignment: Text.AlignVCenter

            wrapMode: Text.Wrap
            color: "white"
        }
    }

    MessageDialog {
        id: errorDialog
        modality:Qt.WindowModal
        icon: StandardIcon.Critical
        standardButtons: StandardButton.Ok
    }
}
