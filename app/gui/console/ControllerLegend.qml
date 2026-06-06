import QtQuick

// Légende manette en bas d'écran : pastilles A / B / Y + libellés.
// Les libellés se modifient via la propriété `hints`.
Item {
    id: root

    property var hints: [
        { btn: "A", label: "Jouer",       bg: "#6cc04a", fg: "#10250a" },
        { btn: "B", label: "Retour",      bg: "#e35b5b", fg: "#501313" },
        { btn: "Y", label: "Param\u00e8tres", bg: "#e0b020", fg: "#412402" }
    ]

    implicitHeight: 46

    // Filet de séparation
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: "#1a1e26"
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        spacing: 22

        Repeater {
            model: root.hints
            delegate: Row {
                spacing: 7
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    width: 20; height: 20; radius: 10
                    color: modelData.bg
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        anchors.centerIn: parent
                        text: modelData.btn
                        color: modelData.fg
                        font.pixelSize: 12; font.weight: Font.Medium
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
