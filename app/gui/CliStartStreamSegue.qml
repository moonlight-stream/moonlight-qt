import QtQuick 2.0
import QtQuick.Controls 2.2

import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0

Item {
    function onSearchingComputer() {
        stageLabel.text = "Connexion en cours au pc..."
    }

    function onSearchingApp() {
        stageLabel.text = "Chargement de la liste des applications..."
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

    StackView.onActivated: {
        if (!launcher.isExecuted()) {
            toolBar.visible = false

            // Normally this is enabled by PcView, but we will won't
            // load PcView when streaming from the command-line.
            SdlGamepadKeyNavigation.enable()

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

    ErrorMessageDialog {
        id: errorDialog

        onClosed: {
            Qt.quit();
        }
    }

    NavigableMessageDialog {
        id: quitAppDialog
        text:"Êtes-vous sûr de vouloir arrêter " + appName +"? Tout progrès non sauvegardé sera perdu."
        standardButtons: Dialog.Yes | Dialog.No
        property string appName : ""

        function quitApp() {
            var component = Qt.createComponent("QuitSegue.qml")
            var params = {"appName": appName}
            stackView.push(component.createObject(stackView, params))
            // Trigger the quit after pushing the quit segue on screen
            launcher.quitRunningApp()
        }

        onAccepted: quitApp()
        onRejected: Qt.quit()
    }
}
