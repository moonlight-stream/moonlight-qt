import QtQuick

// Légende manette en bas d'écran : pastilles A / B / Y + libellés,
// style "console moderne" (cercles sombres, lettre colorée).
// Les libellés se modifient via la propriété `hints`.
Item {
    id: root

    property var hints: [
        { btn: "A", label: qsTr("Jouer"),       color: "#6cc04a" },
        { btn: "B", label: qsTr("Retour"),      color: "#e35b5b" },
        { btn: "Y", label: qsTr("Paramètres"),  color: "#e0b020" }
    ]

    implicitHeight: 48

    // Filet de séparation en léger dégradé
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#001a1e26" }
            GradientStop { position: 0.15; color: "#2a2f3a" }
            GradientStop { position: 0.85; color: "#2a2f3a" }
            GradientStop { position: 1.0; color: "#001a1e26" }
        }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 28
        anchors.verticalCenter: parent.verticalCenter
        spacing: 24

        Repeater {
            model: root.hints
            delegate: Row {
                spacing: 8
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    width: 22; height: 22; radius: 11
                    color: "#171b24"
                    border.color: "#2e3542"
                    border.width: 1
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        anchors.centerIn: parent
                        text: modelData.btn
                        color: modelData.color
                        font.pixelSize: 12; font.weight: Font.Bold
                    }
                }
                Text {
                    text: modelData.label
                    color: "#9aa0ac"; font.pixelSize: 13
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
