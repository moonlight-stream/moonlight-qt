import QtQuick

// Overlay affiché PENDANT la préparation d'un lancement piloté par le Companion
// (POST /v1/launch → … → READY), AVANT que le stream ne démarre. Couvre :
//  - "preparing" : CHECKING / ARMING (spinner) ;
//  - "updating"  : mise à jour du jeu en cours (barre de progression, §9/§10) ;
//  - "error"     : LAUNCH_ERROR (message + B pour fermer).
// Le dialog « Mise à jour requise ? » est géré séparément (ConsoleDialog) par
// ConsoleHome ; ici on ne montre que la progression une fois acceptée.
// Présentationnel : ConsoleHome règle phase/pct/message au fil des signaux.
FocusScope {
    id: root

    property string gameName: ""
    property string phase: "preparing"   // preparing | updating | error
    property int    pct: 0               // 0..100 (phase updating)
    property string message: ""

    signal cancelRequested()

    visible: false

    function show(name) {
        gameName = name
        phase = "preparing"
        pct = 0
        message = ""
        visible = true
        forceActiveFocus()
    }

    function hide() { visible = false }

    Keys.onEscapePressed: root.cancelRequested()

    // Voile + blocage de l'arrière-plan
    Rectangle { anchors.fill: parent; color: "#e6000000" }
    MouseArea { anchors.fill: parent }

    Column {
        anchors.centerIn: parent
        width: 460
        spacing: 22

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.gameName
            color: "#f4f6fa"
            font.pixelSize: 24
            font.weight: Font.DemiBold
        }

        // --- preparing : spinner ---
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 14
            visible: root.phase === "preparing"

            Spinner {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 40; height: 40
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Préparation…")
                color: "#8b909c"; font.pixelSize: 15
            }
        }

        // --- updating : barre de progression ---
        Column {
            width: parent.width
            spacing: 12
            visible: root.phase === "updating"

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Mise à jour du jeu…")
                color: "#e8eaf0"; font.pixelSize: 16; font.weight: Font.Medium
            }

            Rectangle {
                width: parent.width; height: 10; radius: 5
                color: "#1d2330"
                border.color: "#2e3542"; border.width: 1

                Rectangle {
                    height: parent.height; radius: parent.radius
                    width: Math.max(0, Math.min(1, root.pct / 100)) * parent.width
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#F68A38" }
                        GradientStop { position: 1.0; color: "#E5721C" }
                    }
                    Behavior on width { NumberAnimation { duration: 200 } }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.pct + " %"
                color: "#8b909c"; font.pixelSize: 13
            }
        }

        // --- error ---
        Column {
            width: parent.width
            spacing: 10
            visible: root.phase === "error"

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Le lancement a échoué")
                color: "#e35b5b"; font.pixelSize: 18; font.weight: Font.Medium
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: root.message
                color: "#8b909c"; font.pixelSize: 14
                wrapMode: Text.Wrap
            }
        }

        // Indice : annuler (sauf pendant un échec, où B ferme aussi)
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.phase === "error" ? qsTr("B Fermer") : qsTr("B Annuler")
            color: "#6b7280"; font.pixelSize: 13
        }
    }
}
