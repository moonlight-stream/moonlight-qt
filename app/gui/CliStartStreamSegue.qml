import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2

import ComputerManager 1.0

Item {
    visible: false

    function onSearchingComputer(name) {
        stageLabel.text = "Searching computer " + name + "..."
    }

    function onSearchingApp(name) {
        stageLabel.text = "Searching application " + name + "..."
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

    onVisibleChanged: {
        if (visible) {
            toolBar.visible = false
            launcher.searchingComputer.connect(onSearchingComputer)
            launcher.searchingApp.connect(onSearchingApp)
            launcher.sessionCreated.connect(onSessionCreated)
            launcher.failed.connect(onLaunchFailed)
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
            color: "white"
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

}
