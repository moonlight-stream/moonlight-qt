import QtQuick 2.0
import QtQuick.Controls 2.2

import ComputerManager 1.0

Item {
    readonly property int lookupSteps: 2
    readonly property string streamProgressText: quitAppDialog.visible ? qsTr("Waiting for confirmation...") :
                                                  stageLabel.visible ? stageLabel.text : ""

    function onSearchingComputer() {
        stageLabel.step = 1
        stageLabel.text = qsTr("Establishing connection to PC...")
    }

    function onSearchingApp() {
        stageLabel.step = 2
        stageLabel.text = qsTr("Loading app list...")
    }

    function onSessionCreated(appName, session) {
        var component = Qt.createComponent("StreamSegue.qml")
        var segue = component.createObject(stackView, {
            "appName": appName,
            "session": session,
            "lookupSteps": lookupSteps,
            "quitAfter": true
        })
        // Decoder preparation starts on activation. Both views use the same progress card.
        stackView.push(segue, StackView.Immediate)
    }

    function onLaunchFailed(message) {
        stageLabel.visible = false
        errorDialog.text = message
        errorDialog.open()
        console.error(message)
    }

    function onAppQuitRequired(appName) {
        stageLabel.visible = false
        quitAppDialog.appName = appName
        quitAppDialog.open()
    }

    StackView.onActivated: {
        if (!launcher.isExecuted()) {
            toolBar.visible = false

            launcher.searchingComputer.connect(onSearchingComputer)
            launcher.searchingApp.connect(onSearchingApp)
            launcher.sessionCreated.connect(onSessionCreated)
            launcher.failed.connect(onLaunchFailed)
            launcher.appQuitRequired.connect(onAppQuitRequired)
            launcher.execute(ComputerManager)
        }
    }

    StreamStatusCard {
        id: stageLabel
        text: qsTr("Establishing connection to PC...")
        step: 1
        stepCount: lookupSteps + 3
    }

    ErrorMessageDialog {
        id: errorDialog

        onClosed: {
            Qt.quit();
        }
    }

    NavigableMessageDialog {
        id: quitAppDialog
        text:qsTr("Are you sure you want to quit %1? Any unsaved progress will be lost.").arg(appName)
        standardButtons: Dialog.Yes | Dialog.No
        property string appName : ""

        function quitApp() {
            var component = Qt.createComponent("QuitSegue.qml")
            var params = {"appName": appName, "quitRunningAppFn": function() { launcher.quitRunningApp() }}
            stackView.push(component.createObject(stackView, params))
        }

        onAccepted: quitApp()
        onRejected: Qt.quit()
    }
}
