import QtQuick

// Saisie du code d'appairage Companion à 6 chiffres.
// ⚠️ Sens INVERSE de PairingOverlay.qml : ici le HOST affiche le code (toast tray
// du HostCompanion) et l'utilisateur le SAISIT sur la console (décision 2026-06-28).
// 100 % navigable manette : ←/→ change de case, ↑/↓ change le chiffre, A valide,
// B annule. Le clavier 0-9 / Retour arrière marche aussi (confort dev).
// Aucune dépendance Moonlight — piloté par ConsoleHome via les signaux ci-dessous.
FocusScope {
    id: root

    property string hostName: ""
    property string errorText: ""
    property bool   busy: false        // confirm en cours (attente réponse host)

    signal submitted(string code)
    signal cancelled()

    // 6 chiffres, case active surlignée.
    property var digits: [0, 0, 0, 0, 0, 0]
    property int active: 0

    function reset() {
        digits = [0, 0, 0, 0, 0, 0]
        active = 0
        errorText = ""
        busy = false
    }

    function setDigit(i, v) {
        var d = digits.slice()
        d[i] = ((v % 10) + 10) % 10
        digits = d
    }

    function code() { return digits.join("") }

    Keys.onLeftPressed:  active = Math.max(0, active - 1)
    Keys.onRightPressed: active = Math.min(5, active + 1)
    Keys.onUpPressed:    setDigit(active, digits[active] + 1)
    Keys.onDownPressed:  setDigit(active, digits[active] - 1)
    Keys.onReturnPressed: submit()
    Keys.onEnterPressed:  submit()
    Keys.onEscapePressed: root.cancelled()
    Keys.onPressed: function(event) {
        if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            setDigit(active, event.key - Qt.Key_0)
            if (active < 5) active = active + 1
            event.accepted = true
        } else if (event.key === Qt.Key_Backspace) {
            active = Math.max(0, active - 1)
            event.accepted = true
        }
    }

    function submit() {
        if (busy) return
        busy = true
        root.submitted(code())
    }

    Column {
        anchors.centerIn: parent
        spacing: 26

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Liaison avec %1").arg(root.hostName !== "" ? root.hostName : qsTr("votre PC"))
            color: "#f4f6fa"
            font.pixelSize: 26
            font.weight: Font.DemiBold
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Saisissez le code à 6 chiffres affiché sur votre PC.")
            color: "#8b909c"
            font.pixelSize: 15
        }

        // Les 6 cases
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 14

            Repeater {
                model: 6

                Rectangle {
                    width: 62; height: 78; radius: 12
                    color: index === root.active ? "#1d2330" : "#151923"
                    border.color: index === root.active ? "#F2802A" : "#2e3542"
                    border.width: index === root.active ? 3 : 2

                    Text {
                        anchors.centerIn: parent
                        text: root.digits[index]
                        color: index === root.active ? "#ffffff" : "#cdd2da"
                        font.pixelSize: 38
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.active = index
                    }
                }
            }
        }

        // Indice manette / état
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10
            visible: root.errorText === ""

            Spinner {
                width: 18; height: 18; lineWidth: 2.5
                anchors.verticalCenter: parent.verticalCenter
                visible: root.busy
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.busy ? qsTr("Vérification…")
                                : qsTr("↑↓ choisir · ←→ se déplacer · A valider")
                color: "#6b7280"
                font.pixelSize: 13
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.errorText !== ""
            horizontalAlignment: Text.AlignHCenter
            text: root.errorText
            color: "#e0b020"
            font.pixelSize: 14
        }
    }
}
