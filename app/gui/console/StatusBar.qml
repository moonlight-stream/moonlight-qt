import QtQuick

// Bandeau supérieur : wordmark + état de connexion au PC host (gauche),
// signal / batterie / horloge (droite). Aucune dépendance Moonlight.
Item {
    id: root

    property string hostName: "PC de Marco"
    property bool connected: true
    property int batteryPercent: 72
    // Marque affichée en haut à gauche — sera remplacée au rebranding.
    property string brand: "MOONLIGHT"

    implicitHeight: 52

    // --- Gauche : wordmark + chip d'état de connexion ---
    Row {
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        // Logo : carré orange arrondi avec triangle "play"
        Rectangle {
            width: 22; height: 22; radius: 6
            anchors.verticalCenter: parent.verticalCenter
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#F68A38" }
                GradientStop { position: 1.0; color: "#E5721C" }
            }
            Canvas {
                anchors.centerIn: parent
                width: 10; height: 10
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.fillStyle = "#3A1505"
                    ctx.beginPath()
                    ctx.moveTo(1, 0); ctx.lineTo(9, 5); ctx.lineTo(1, 10)
                    ctx.closePath(); ctx.fill()
                }
            }
        }

        Text {
            text: root.brand
            color: "#cdd2da"
            font.pixelSize: 13
            font.letterSpacing: 3
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }

        // Chip d'état : point (pulsant pendant la recherche) + host + état
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: statusRow.width + 26
            height: 30
            radius: 15
            color: "#0affffff"
            border.color: "#14ffffff"
            border.width: 1

            Row {
                id: statusRow
                anchors.centerIn: parent
                spacing: 8

                Item {
                    width: 9; height: 9
                    anchors.verticalCenter: parent.verticalCenter

                    // Halo autour du point quand tout est OK
                    Rectangle {
                        anchors.centerIn: parent
                        width: 15; height: 15; radius: 7.5
                        color: "transparent"
                        border.color: "#4034d399"
                        border.width: 1.5
                        visible: root.connected
                    }
                    Rectangle {
                        id: dot
                        anchors.centerIn: parent
                        width: 9; height: 9; radius: 4.5
                        color: root.connected ? "#34d399" : "#e0b020"
                        opacity: root.connected ? 1 : pulse
                        property real pulse: 1
                        SequentialAnimation on pulse {
                            running: !root.connected
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.25; duration: 700; easing.type: Easing.InOutQuad }
                            NumberAnimation { to: 1.0; duration: 700; easing.type: Easing.InOutQuad }
                        }
                    }
                }
                Text {
                    text: root.hostName
                    color: "#e8eaf0"; font.pixelSize: 13
                    font.weight: Font.Medium
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: root.connected ? qsTr("CONNECTÉ") : qsTr("CONNEXION…")
                    color: root.connected ? "#34d399" : "#8b909c"
                    font.pixelSize: 10
                    font.letterSpacing: 1
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    // --- Droite : signal, batterie, horloge ---
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        spacing: 18

        // Barres de signal (4 barres croissantes)
        Item {
            width: 4 * 4 + 3 * 3
            height: 17
            anchors.verticalCenter: parent.verticalCenter
            Row {
                anchors.bottom: parent.bottom
                spacing: 3
                Repeater {
                    model: 4
                    Rectangle {
                        width: 4
                        height: 5 + index * 4
                        radius: 1
                        color: root.connected ? "#cdd2da" : "#5b6170"
                        anchors.bottom: parent.bottom
                    }
                }
            }
        }

        // Batterie : jauge + pourcentage
        Row {
            spacing: 7
            anchors.verticalCenter: parent.verticalCenter

            Text {
                text: root.batteryPercent + " %"
                color: "#9aa0ac"; font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
            Row {
                spacing: 2
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
                        color: root.batteryPercent > 20 ? "#34d399" : "#e35b5b"
                    }
                }
                Rectangle {
                    width: 2.5; height: 6; radius: 1; color: "#8b909c"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Séparateur fin
        Rectangle {
            width: 1; height: 16
            color: "#1f242e"
            anchors.verticalCenter: parent.verticalCenter
        }

        // Horloge (mise à jour chaque seconde)
        Text {
            id: clock
            color: "#e8eaf0"; font.pixelSize: 14
            font.weight: Font.Medium
            anchors.verticalCenter: parent.verticalCenter
            text: Qt.formatTime(new Date(), "HH:mm")
            Timer {
                interval: 1000; running: true; repeat: true
                onTriggered: clock.text = Qt.formatTime(new Date(), "HH:mm")
            }
        }
    }
}
