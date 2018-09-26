import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2
import QtQuick.Window 2.2

import Session 1.0

Item {
    property Session session
    property string appName
    property string stageText : "Starting " + appName + "..."
    property bool quitAfter : false

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
        // This toast appears for 3 seconds, just shorter than how long
        // Session will wait for it to be displayed. This gives it time
        // to transition to invisible before continuing.
        var toast = Qt.createQmlObject('import QtQuick.Controls 2.3; ToolTip {}', parent, '')
        toast.text = text
        toast.timeout = 3000
        toast.visible = true
    }

    onVisibleChanged: {
        if (visible) {
            // Hide the toolbar before we start loading
            toolBar.visible = false

            // Hook up our signals
            session.stageStarting.connect(stageStarting)
            session.stageFailed.connect(stageFailed)
            session.connectionStarted.connect(connectionStarted)
            session.displayLaunchError.connect(displayLaunchError)
            session.displayLaunchWarning.connect(displayLaunchWarning)

            // Run the streaming session to completion
            session.exec(Screen.virtualX, Screen.virtualY)

            if (quitAfter) {
                Qt.quit()
            } else {
                // Show the Qt window again after streaming
                window.visible = true

                // Exit this view
                stackView.pop()
            }
        }
        else {
            // Show the toolbar again when we become hidden
            toolBar.visible = true
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

    Label {
        text: "Tip: Press Ctrl+Alt+Shift+Q to disconnect your session"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        font.pointSize: 18
        verticalAlignment: Text.AlignVCenter

        wrapMode: Text.Wrap
        color: "white"
    }

    MessageDialog {
        id: errorDialog
        modality:Qt.WindowModal
        icon: StandardIcon.Critical
        standardButtons: StandardButton.Ok | StandardButton.Help
        onHelp: {
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Troubleshooting");
        }
    }
}
