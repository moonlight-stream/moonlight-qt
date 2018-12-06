import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2

import ComputerManager 1.0
import Session 1.0

Item {
    anchors.fill: parent

    function onSearchingComputer() {
        stageLabel.text = "Establishing connection to PC..."
    }

    function onQuittingApp() {
        stageLabel.text = "Quitting app..."
    }

    function onFailure(message) {
        errorDialog.text = message
        errorDialog.open()
    }

    // The StackView will trigger a visibility change when
    // we're pushed onto it, causing our onVisibleChanged
    // routine to run, but only if we start as invisible
    visible: false

    onVisibleChanged: {
        if (visible && !launcher.isExecuted()) {
            toolBar.visible = false
            launcher.searchingComputer.connect(onSearchingComputer)
            launcher.quittingApp.connect(onQuittingApp)
            launcher.failed.connect(onFailure)
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
            text: stageText
            font.pointSize: 20
            verticalAlignment: Text.AlignVCenter

            wrapMode: Text.Wrap
        }
    }

    MessageDialog {
        id: errorDialog
        modality:Qt.WindowModal
        icon: StandardIcon.Critical
        standardButtons: StandardButton.Ok

        onVisibleChanged: {
            if (!visible) {
                Qt.quit()
            }
        }
    }
}
