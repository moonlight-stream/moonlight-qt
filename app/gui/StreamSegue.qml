import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.3

import Session 1.0

Item {
    property Session session
    property string appname
    property string stageText : "Starting " + appname + "..."

    anchors.fill: parent

    // The StackView will trigger a visibility change when
    // we're pushed onto it, causing our onVisibleChanged
    // routine to run, but only if we start as invisible
    visible: false

    function stageStarting(stage)
    {
        // Update the spinner text
        stageText = "Starting " + stage + "..."
    }

    function stageFailed(stage, errorCode)
    {
        errorDialog.text = "Starting " + stage + " failed: Error " + errorCode
        errorDialog.open()
    }

    function connectionStarted()
    {
        // Hide the UI contents so the user doesn't
        // see them briefly when we pop off the StackView
        stageSpinner.visible = false
        stageLabel.visible = false

        // Hide the window now that streaming has begun
        window.visible = false
    }

    function displayLaunchError(text)
    {
        errorDialog.text = text
        errorDialog.open()
    }

    function displayLaunchWarning(text)
    {
        // TODO: toast
    }

    onVisibleChanged: {
        if (visible) {
            // Hook up our signals
            session.stageStarting.connect(stageStarting)
            session.stageFailed.connect(stageFailed)
            session.connectionStarted.connect(connectionStarted)
            session.displayLaunchError.connect(displayLaunchError)
            session.displayLaunchWarning.connect(displayLaunchWarning)

            // Run the streaming session to completion
            session.exec()

            // Show the Qt window again after streaming
            window.visible = true

            // Exit this view
            stackView.pop()
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
