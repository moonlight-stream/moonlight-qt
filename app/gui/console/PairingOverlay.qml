import QtQuick

// Écran de liaison avec le PC host, pensé comme l'appairage d'une app TV :
// un grand code à saisir une seule fois côté PC, puis tout est automatique.
// Aucune dépendance Moonlight — piloté par ConsoleHome via `pin` / `errorText`.
Item {
    id: root

    property string pin: ""
    property string hostName: ""
    property string errorText: ""

    Column {
        anchors.centerIn: parent
        spacing: 28

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Liaison avec %1").arg(root.hostName !== "" ? root.hostName : qsTr("votre PC"))
            color: "#f4f6fa"
            font.pixelSize: 26
            font.weight: Font.DemiBold
        }

        // Le code, chiffre par chiffre, dans de grandes cases
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 14

            Repeater {
                model: root.pin.length

                Rectangle {
                    width: 62; height: 78; radius: 12
                    color: "#151923"
                    border.color: "#F2802A"
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: root.pin.charAt(index)
                        color: "#ffffff"
                        font.pixelSize: 38
                        font.weight: Font.Bold
                    }
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Saisissez ce code sur votre PC.\nLa console se connectera ensuite automatiquement.")
            color: "#8b909c"
            font.pixelSize: 15
            lineHeight: 1.3
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10
            visible: root.errorText === ""

            Spinner {
                width: 18; height: 18; lineWidth: 2.5
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: qsTr("En attente de votre PC…")
                color: "#6b7280"
                font.pixelSize: 13
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.errorText !== ""
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("La liaison a échoué — nouvelle tentative dans un instant…")
            color: "#e0b020"
            font.pixelSize: 13
        }
    }
}
