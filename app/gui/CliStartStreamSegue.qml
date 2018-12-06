import QtQml 2.2
import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2

import ComputerManager 1.0

Item {
    visible: false

    function onSearchingComputer() {
        stageLabel.text = "Establishing connection to PC..."
    }

    function onSearchingApp() {
        stageLabel.text = "Loading app list..."
    }

    function onSessionCreated(appName, session) {
        var component = Qt.createComponent("StreamSegue.qml")
        var segue = component.createObject(stackView, {
            "appName": appName,
            "session": session,
            "quitAfter": true
        })
        stackView.push(segue)
    }

    function onLaunchFailed(message) {
        errorDialog.text = message
        errorDialog.open()
    }

    function onAppQuitRequired(appName) {
        quitAppDialog.appName = appName
        quitAppDialog.open()
    }

    onVisibleChanged: {
        if (visible && !launcher.isExecuted()) {
            toolBar.visible = false
            launcher.searchingComputer.connect(onSearchingComputer)
            launcher.searchingApp.connect(onSearchingApp)
            launcher.sessionCreated.connect(onSessionCreated)
            launcher.failed.connect(onLaunchFailed)
            launcher.appQuitRequired.connect(onAppQuitRequired)
            launcher.execute(ComputerManager)
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
            font.pointSize: 20
            verticalAlignment: Text.AlignVCenter

            wrapMode: Text.Wrap
        }
    }

    MessageDialog {
        id: errorDialog
        modality:Qt.WindowModal
        icon: StandardIcon.Critical
        standardButtons: StandardButton.Ok | StandardButton.Help
        onHelp: {
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Troubleshooting");
        }
        onVisibleChanged: {
            if (!visible) {
                Qt.quit();
            }
        }
    }

    MessageDialog {
        id: quitAppDialog
        modality:Qt.WindowModal
        text:"Are you sure you want to quit " + appName +"? Any unsaved progress will be lost."
        standardButtons: StandardButton.Yes | StandardButton.No
        property string appName : ""

        function quitApp() {
            var component = Qt.createComponent("QuitSegue.qml")
            var params = {"appName": appName}
            stackView.push(component.createObject(stackView, params))
            // Trigger the quit after pushing the quit segue on screen
            launcher.quitRunningApp()
        }

        onYes: quitApp()

        // For keyboard/gamepad navigation
        onAccepted: quitApp()

        // Exit process if app quit is rejected (reacts also to closing of the
        // dialog from title bar's close button).
        // Note: this depends on undocumented behavior of visibleChanged()
        // signal being emitted before yes() or accepted() has been emitted.
        onVisibleChanged: {
            if (!visible) {
                quitTimer.start()
            }
        }
        Component.onCompleted: {
            yes.connect(quitTimer.stop)
            accepted.connect(quitTimer.stop)
        }
    }

    Timer {
        id: quitTimer
        interval: 100
        onTriggered: Qt.quit()
    }
}
