import QtQuick

// Bandeau supérieur : état de connexion au PC host (gauche),
// signal / batterie / horloge (droite). Aucune dépendance Moonlight.
Item {
    id: root

    property string hostName: "PC de Marco"
    property bool connected: true
    property int batteryPercent: 72

    implicitHeight: 48

    // --- Gauche : état de connexion ---
    Row {
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Rectangle {
            width: 9; height: 9; radius: 4.5
            anchors.verticalCenter: parent.verticalCenter
            color: root.connected ? "#34d399" : "#e0b020"
        }
        Text {
            text: root.hostName
            color: "#e8eaf0"; font.pixelSize: 15
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.connected ? "\u00b7 connect\u00e9" : "\u00b7 hors ligne"
            color: "#6b7280"; font.pixelSize: 14
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // --- Droite : signal, batterie, horloge ---
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        // Barres de signal (4 barres croissantes)
        Row {
            spacing: 3
            anchors.verticalCenter: parent.verticalCenter
            Repeater {
                model: 4
                Rectangle {
                    width: 4
                    height: 5 + index * 4
                    radius: 1
                    color: "#8b909c"
                    anchors.bottom: parent.bottom
                }
            }
        }

        // Batterie
        Row {
            spacing: 3
            anchors.verticalCenter: parent.verticalCenter
            Rectangle {
                width: 26; height: 13; radius: 3
                color: "transparent"
                border.color: "#8b909c"; border.width: 1.5
                anchors.verticalCenter: parent.verticalCenter
                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    height: 7
                    width: (parent.width - 4) * Math.max(0, Math.min(100, root.batteryPercent)) / 100
                    radius: 1.5
                    color: root.batteryPercent > 20 ? "#8b909c" : "#e35b5b"
                }
            }
            Rectangle {
                width: 2.5; height: 6; radius: 1; color: "#8b909c"
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Horloge (mise à jour chaque seconde)
        Text {
            id: clock
            color: "#cdd2da"; font.pixelSize: 14
            anchors.verticalCenter: parent.verticalCenter
            text: Qt.formatTime(new Date(), "HH:mm")
            Timer {
                interval: 1000; running: true; repeat: true
                onTriggered: clock.text = Qt.formatTime(new Date(), "HH:mm")
            }
        }
    }
}
