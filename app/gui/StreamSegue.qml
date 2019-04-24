import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Window 2.2

import SdlGamepadKeyNavigation 1.0
import Session 1.0

Item {
    property Session session
    property string appName
    property string stageText : "Starting " + appName + "..."
    property bool quitAfter : false

    function stageStarting(stage)
    {
        // Update the spinner text
        stageText = "Starting " + stage + "..."
    }

    function stageFailed(stage, errorCode)
    {
        // Display the error dialog after Session::exec() returns
        streamSegueErrorDialog.text = "Starting " + stage + " failed: Error " + errorCode
    }

    function connectionStarted()
    {
        // Hide the UI contents so the user doesn't
        // see them briefly when we pop off the StackView
        stageSpinner.visible = false
        stageLabel.visible = false
        hintText.visible = false

        // Hide the window now that streaming has begun
        window.visible = false
    }

    function displayLaunchError(text)
    {
        // Display the error dialog after Session::exec() returns
        streamSegueErrorDialog.text = text
    }

    function displayLaunchWarning(text)
    {
        // This toast appears for 3 seconds, just shorter than how long
        // Session will wait for it to be displayed. This gives it time
        // to transition to invisible before continuing.
        var toast = Qt.createQmlObject('import QtQuick.Controls 2.2; ToolTip {}', parent, '')
        toast.text = text
        toast.timeout = 3000
        toast.visible = true
    }

    function quitStarting()
    {
        // Avoid the push transition animation
        var component = Qt.createComponent("QuitSegue.qml")
        stackView.replace(stackView.currentItem, component.createObject(stackView, {"appName": appName}), StackView.Immediate)

        // Show the Qt window again to show quit segue
        window.visible = true
    }

    function sessionFinished()
    {
        // Enable GUI gamepad usage now
        SdlGamepadKeyNavigation.enable()

        if (quitAfter) {
            if (streamSegueErrorDialog.text) {
                // Quit when the error dialog is acknowledged
                streamSegueErrorDialog.quitAfter = quitAfter
                streamSegueErrorDialog.open()
            }
            else {
                // Quit immediately
                Qt.quit()
            }
        } else {
            // Exit this view
            stackView.pop()

            // Show the Qt window again after streaming
            window.visible = true

            // Display any launch errors. We do this after
            // the Qt UI is visible again to prevent losing
            // focus on the dialog which would impact gamepad
            // users.
            if (streamSegueErrorDialog.text) {
                streamSegueErrorDialog.quitAfter = quitAfter
                streamSegueErrorDialog.open()
            }
        }
    }

    StackView.onDeactivating: {
        // Show the toolbar again when popped off the stack
        toolBar.visible = true

        // Enable GUI gamepad usage now
        SdlGamepadKeyNavigation.enable()
    }

    StackView.onActivated: {
        // Hide the toolbar before we start loading
        toolBar.visible = false

        // Hook up our signals
        session.stageStarting.connect(stageStarting)
        session.stageFailed.connect(stageFailed)
        session.connectionStarted.connect(connectionStarted)
        session.displayLaunchError.connect(displayLaunchError)
        session.displayLaunchWarning.connect(displayLaunchWarning)
        session.quitStarting.connect(quitStarting)
        session.sessionFinished.connect(sessionFinished)

        // Kick off the stream
        streamLoader.active = true
    }

    Loader {
        id: streamLoader
        active: false
        asynchronous: true

        onLoaded: {
            // Set the hint text. We do this here rather than
            // in the hintText control itself to synchronize
            // with Session.exec() which requires no concurrent
            // gamepad usage.
            hintText.text = SdlGamepadKeyNavigation.getConnectedGamepads() > 0 ?
                      "Tip: Press Start+Select+L1+R1 to disconnect your session" :
                      "Tip: Press Ctrl+Alt+Shift+Q to disconnect your session"

            // Stop GUI gamepad usage now
            SdlGamepadKeyNavigation.disable()

            // Run the streaming session to completion
            session.exec(Screen.virtualX, Screen.virtualY)
        }

        sourceComponent: Item {}
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
        }
    }

    Label {
        id: hintText
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        font.pointSize: 18
        verticalAlignment: Text.AlignVCenter

        wrapMode: Text.Wrap
    }
}
