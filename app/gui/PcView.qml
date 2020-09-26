import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerModel 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0
import SdlGamepadKeyNavigation 1.0

CenteredGridView {
    property ComputerModel computerModel : createModel()

    id: pcGrid
    focus: true
    activeFocusOnTab: true
    topMargin: 20
    bottomMargin: 5
    cellWidth: 310; cellHeight: 330;
    objectName: "Ordinateurs"

    Component.onCompleted: {
        // Don't show any highlighted item until interacting with them.
        // We do this here instead of onActivated to avoid losing the user's
        // selection when backing out of a different page of the app.
        currentIndex = -1
    }

    // Note: Any initialization done here that is critical for streaming must
    // also be done in CliStartStreamSegue.qml, since this code does not run
    // for command-line initiated streams.
    StackView.onActivated: {
        // Setup signals on CM
        ComputerManager.computerAddCompleted.connect(addComplete)

        // This is a bit of a hack to do this here as opposed to main.qml, but
        // we need it enabled before calling getConnectedGamepads() and PcView
        // is never destroyed, so it should be okay.
        SdlGamepadKeyNavigation.enable()

        // Highlight the first item if a gamepad is connected
        if (currentIndex == -1 && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            currentIndex = 0
        }
    }

    StackView.onDeactivating: {
        ComputerManager.computerAddCompleted.disconnect(addComplete)
    }

    function pairingComplete(error)
    {
        // Close the PIN dialog
        pairDialog.close()

        // Display a failed dialog if we got an error
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.helpText = ""
            errorDialog.open()
        }
    }

    function addComplete(success, detectedPortBlocking)
    {
        if (!success) {
            errorDialog.text = "Impossible de se connecter au PC spécifié."

            if (detectedPortBlocking) {
                errorDialog.text += "\n\nLa connexion Internet de ce PC bloque le Moonlight. Le streaming sur Internet peut ne pas fonctionner lorsqu'il est connecté à ce réseau."
            }
            else {
                errorDialog.helpText = "Cliquez sur le bouton Aide pour connaître les solutions possibles."
            }

            errorDialog.open()
        }
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', parent, '')
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(pairingComplete)
        model.connectionTestCompleted.connect(testConnectionDialog.connectionTestComplete)
        return model
    }

    Row {
        anchors.centerIn: parent
        spacing: 5
        visible: pcGrid.count === 0

        BusyIndicator {
            id: searchSpinner
            visible: StreamingPreferences.enableMdns
        }

        Label {
            height: searchSpinner.height
            elide: Label.ElideRight
            text: StreamingPreferences.enableMdns ? "Recherche de PC avec NVIDIA GameStream activé..."
                                                  : "La découverte automatique des PC est désactivée. Ajoutez votre PC manuellement."
            font.pointSize: 20
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }

    model: computerModel

    delegate: NavigableItemDelegate {
        width: 300; height: 320;
        grid: pcGrid

        Image {
            id: pcIcon
            anchors.horizontalCenter: parent.horizontalCenter
            source: "qrc:/res/desktop_windows-48px.svg"
            sourceSize {
                width: 200
                height: 200
            }
        }

        Image {
            // TODO: Tooltip
            id: stateIcon
            anchors.horizontalCenter: pcIcon.horizontalCenter
            anchors.verticalCenter: pcIcon.verticalCenter
            anchors.verticalCenterOffset: -15
            visible: !model.statusUnknown && (!model.online || !model.paired)
            source: !model.online ? "qrc:/res/baseline-warning-24px.svg" : "qrc:/res/baseline-lock-24px.svg"
            sourceSize {
                width: 75
                height: 75
            }
        }

        BusyIndicator {
            id: statusUnknownSpinner
            anchors.horizontalCenter: pcIcon.horizontalCenter
            anchors.verticalCenter: pcIcon.verticalCenter
            anchors.verticalCenterOffset: -15
            width: 75
            height: 75
            visible: model.statusUnknown
        }

        Label {
            id: pcNameText
            text: model.name

            width: parent.width
            anchors.top: pcIcon.bottom
            anchors.bottom: parent.bottom
            font.pointSize: 36
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }

        NavigableMenu {
            id: pcContextMenu
            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: "Voir les applications"
                onTriggered: {
                    var component = Qt.createComponent("AppView.qml")
                    var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name})
                    stackView.push(appView)
                }
                visible: model.online && model.paired
            }
            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: "Voir les applications cachées"
                onTriggered: {
                    var component = Qt.createComponent("AppView.qml")
                    var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name, "showHiddenGames": true})
                    stackView.push(appView)
                }
                visible: model.online && model.paired
            }
            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: "Réveiller le PC"
                onTriggered: computerModel.wakeComputer(index)
                visible: !model.online && model.wakeable
            }
            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: "Test Réseau"
                onTriggered: {
                    computerModel.testConnectionForComputer(index)
                    testConnectionDialog.open()
                }
            }

            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: "Renommer le PC"
                onTriggered: {
                    renamePcDialog.pcIndex = index
                    renamePcDialog.originalName = model.name
                    renamePcDialog.open()
                }
            }
            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: "Supprimer le PC"
                onTriggered: {
                    deletePcDialog.pcIndex = index
                    // get confirmation first, actual closing is called from the dialog
                    deletePcDialog.open()
                }
            }
        }

        onClicked: {
            if (model.online) {
                if (model.paired) {
                    // go to game view
                    var component = Qt.createComponent("AppView.qml")
                    var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name})
                    stackView.push(appView)
                }
                else {
                    if (!model.busy) {
                        var pin = computerModel.generatePinString()

                        // Kick off pairing in the background
                        computerModel.pairComputer(index, pin)

                        // Display the pairing dialog
                        pairDialog.pin = pin
                        pairDialog.open()
                    }
                    else {
                        // cannot pair while something is streaming or attempting to pair
                        errorDialog.text = "Vous ne pouvez pas effectuer de couplage alors qu'une session précédente est encore en cours sur le PC hôte. Quittez tous les jeux en cours ou redémarrez le PC hôte, puis essayez à nouveau d'effectuer l'appariement."
                        errorDialog.helpText = ""
                        errorDialog.open()
                    }
                }
            } else if (!model.online) {
                // Using open() here because it may be activated by keyboard
                pcContextMenu.open()
            }
        }

        onPressAndHold: {
            // popup() ensures the menu appears under the mouse cursor
            if (pcContextMenu.popup) {
                pcContextMenu.popup()
            }
            else {
                // Qt 5.9 doesn't have popup()
                pcContextMenu.open()
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton;
            onClicked: {
                parent.onPressAndHold()
            }
        }

        Keys.onMenuPressed: {
            // We must use open() here so the menu is positioned on
            // the ItemDelegate and not where the mouse cursor is
            pcContextMenu.open()
        }
    }

    ErrorMessageDialog {
        id: errorDialog

        // Using Setup-Guide here instead of Troubleshooting because it's likely that users
        // will arrive here by forgetting to enable GameStream or not forwarding ports.
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide"
    }

    NavigableMessageDialog {
        id: pairDialog

        // Pairing dialog must be modal to prevent double-clicks from triggering
        // pairing twice
        modal: true
        closePolicy: Popup.CloseOnEscape

        // don't allow edits to the rest of the window while open
        property string pin : "0000"
        text:"Please enter " + pin + " sur votre GameStream PC. Cette boîte de dialogue se fermera lorsque l'appariement sera terminé."
        standardButtons: Dialog.Cancel
        onRejected: {
            // FIXME: We should interrupt pairing here
        }
    }

    NavigableMessageDialog {
        id: deletePcDialog
        // don't allow edits to the rest of the window while open
        property int pcIndex : -1;
        text:"Êtes-vous sûr de vouloir supprimer ce PC ?"
        standardButtons: Dialog.Yes | Dialog.No

        function deletePc() {
            console.log("suppression de l'appariement PC pour PC à l'index : " + pcIndex)
            computerModel.deleteComputer(pcIndex);
        }

        onAccepted: deletePc()
    }

    NavigableMessageDialog {
        id: testConnectionDialog
        closePolicy: Popup.CloseOnEscape
        standardButtons: Dialog.Ok

        onAboutToShow: {
            testConnectionDialog.text = "Moonlight teste votre connexion réseau pour déterminer si NVIDIA GameStream est bloqué.\n\nCela peut prendre quelques secondes ..."
            showSpinner = true
        }

        function connectionTestComplete(result, blockedPorts)
        {
            if (result === -1) {
                text = "Le test de réseau n'a pas pu être effectué car aucun des serveurs de test de connexion de Moonlight n'était accessible depuis ce PC. Vérifiez votre connexion Internet ou réessayez plus tard."
                imageSrc = "qrc:/res/baseline-warning-24px.svg"
            }
            else if (result === 0) {
                text = "Ce réseau ne semble pas bloquer le Moonlight. Si vous avez toujours des difficultés à vous connecter, vérifiez les paramètres du pare-feu de votre PC. Si vous essayez d'utiliser le streaming sur Internet, installez l'outil d'hébergement Internet Moonlight sur votre PC de jeu et exécutez le testeur de streaming Internet inclus pour vérifier la connexion Internet de votre PC de jeu."
                imageSrc = "qrc:/res/baseline-check_circle_outline-24px.svg"
            }
            else {
                text = "La connexion réseau actuelle de votre PC semble bloquer le Moonlight. Il se peut que la diffusion en continu sur Internet ne fonctionne pas lorsque vous êtes connecté à ce réseau.\n\nLes ports de réseau suivants ont été bloqués :\n"
                text += blockedPorts
                imageSrc = "qrc:/res/baseline-error_outline-24px.svg"
            }

            // Stop showing the spinner and show the image instead
            showSpinner = false
        }
    }

    NavigableDialog {
        id: renamePcDialog
        property string label: "Saisissez le nouveau nom de ce PC :"
        property string originalName
        property int pcIndex : -1;

        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {
            // Force keyboard focus on the textbox so keyboard navigation works
            editText.forceActiveFocus()
        }

        onClosed: {
            editText.clear()
        }

        onAccepted: {
            if (editText.text) {
                computerModel.renameComputer(pcIndex, editText.text)
            }
        }

        ColumnLayout {
            Label {
                text: renamePcDialog.label
                font.bold: true
            }

            TextField {
                id: editText
                placeholderText: renamePcDialog.originalName
                Layout.fillWidth: true
                focus: true

                Keys.onReturnPressed: {
                    renamePcDialog.accept()
                }
            }
        }
    }

    ScrollBar.vertical: ScrollBar {}
}
