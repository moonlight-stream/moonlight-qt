import QtQuick

// Boîte de confirmation modale style console, 100 % navigable manette :
// gauche/droite pour choisir, A (Entrée) valide le bouton en focus,
// B (Échap) annule. Aucune dépendance Moonlight.
FocusScope {
    id: root

    property string title: ""
    property string message: ""
    property string confirmLabel: qsTr("Confirmer")
    property string cancelLabel: qsTr("Annuler")

    signal confirmed()
    signal closed()

    visible: false

    // 0 = annuler, 1 = confirmer. Annuler par défaut : le choix sûr.
    property int focusedButton: 0

    function open() {
        focusedButton = 0
        visible = true
        forceActiveFocus()
    }

    function close() {
        visible = false
        closed()
    }

    Keys.onEscapePressed: close()
    Keys.onLeftPressed: focusedButton = 0
    Keys.onRightPressed: focusedButton = 1
    Keys.onReturnPressed: activate()
    Keys.onEnterPressed: activate()

    function activate() {
        if (focusedButton === 1) {
            visible = false
            confirmed()
            closed()
        } else {
            close()
        }
    }

    // Voile sombre + blocage des clics vers l'arrière-plan
    Rectangle { anchors.fill: parent; color: "#d9000000" }
    MouseArea { anchors.fill: parent }

    Rectangle {
        anchors.centerIn: parent
        width: 480
        height: content.height + 60
        radius: 16
        color: "#151923"
        border.color: "#2e3542"
        border.width: 1

        Column {
            id: content
            anchors.centerIn: parent
            width: parent.width - 64
            spacing: 16

            Text {
                width: parent.width
                text: root.title
                color: "#f4f6fa"
                font.pixelSize: 20
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: root.message
                color: "#8b909c"
                font.pixelSize: 14
                lineHeight: 1.3
                wrapMode: Text.Wrap
            }

            Row {
                anchors.right: parent.right
                spacing: 12

                Rectangle {
                    width: cancelText.width + 36; height: 40; radius: 10
                    color: "#1d2330"
                    border.color: root.focusedButton === 0 ? "#F2802A" : "#2e3542"
                    border.width: root.focusedButton === 0 ? 2 : 1
                    Text {
                        id: cancelText
                        anchors.centerIn: parent
                        text: root.cancelLabel
                        color: "#cdd2da"
                        font.pixelSize: 14; font.weight: Font.Medium
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.close()
                    }
                }

                Rectangle {
                    width: confirmText.width + 36; height: 40; radius: 10
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#F68A38" }
                        GradientStop { position: 1.0; color: "#E5721C" }
                    }
                    border.color: root.focusedButton === 1 ? "#ffffff" : "transparent"
                    border.width: root.focusedButton === 1 ? 2 : 0
                    Text {
                        id: confirmText
                        anchors.centerIn: parent
                        text: root.confirmLabel
                        color: "#3A1505"
                        font.pixelSize: 14; font.weight: Font.DemiBold
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { root.focusedButton = 1; root.activate() }
                    }
                }
            }
        }
    }
}
