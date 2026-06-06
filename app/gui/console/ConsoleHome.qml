import QtQuick

// Écran d'accueil "Big Picture" de la console.
// Autonome : modèle de démo intégré, aucune dépendance au backend Moonlight.
// Aperçu rapide :  qml6 ConsoleHome.qml
FocusScope {
    id: home
    focus: true

    // Taille par défaut pour l'aperçu autonome.
    // En intégration réelle, le StackView de Moonlight redimensionne ce composant.
    implicitWidth: 1280
    implicitHeight: 720

    // Fond "écran" sombre
    Rectangle { anchors.fill: parent; color: "#0d0f14" }

    // --- Modèle de démo (à remplacer par appModel de Moonlight) ---
    ListModel {
        id: gamesModel
        ListElement { title: "Neon Circuit"; genre: "Course";     art: "#1d4d4a" }
        ListElement { title: "Stellar Drift"; genre: "Action-RPG"; art: "#4a3a24" }
        ListElement { title: "Ashen Vale";    genre: "Aventure";   art: "#5a3324" }
        ListElement { title: "Hollow Tide";   genre: "Strat\u00e9gie";  art: "#20405e" }
        ListElement { title: "Iron Bloom";    genre: "Plateforme"; art: "#2d4a1e" }
        ListElement { title: "Dust & Echo";   genre: "Ind\u00e9";       art: "#4b2238" }
        ListElement { title: "Pale Lantern";  genre: "Survie";     art: "#4d3a12" }
    }

    function currentGame() {
        return gamesModel.get(Math.max(0, carousel.currentIndex))
    }

    // --- Bandeau supérieur ---
    StatusBar {
        id: status
        anchors { top: parent.top; left: parent.left; right: parent.right }
        hostName: "PC de Marco"
        connected: true
        batteryPercent: 72
    }

    // --- Étiquette de section + compteur ---
    Item {
        id: labelRow
        anchors { top: status.bottom; left: parent.left; right: parent.right }
        height: 28
        Text {
            anchors.left: parent.left; anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            text: "Vos jeux"; color: "#8b909c"; font.pixelSize: 14
        }
        Text {
            anchors.right: parent.right; anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            text: (carousel.currentIndex + 1) + " / " + gamesModel.count
            color: "#6b7280"; font.pixelSize: 14
        }
    }

    // --- Carrousel ---
    GameCarousel {
        id: carousel
        anchors { top: labelRow.bottom; topMargin: 12; left: parent.left; right: parent.right }
        height: focusedH + 30
        model: gamesModel
        currentIndex: 1
        focus: true

        onLaunchRequested: function(index) {
            // POINT D'INTÉGRATION MOONLIGHT :
            // remplacer ces deux lignes par la création de session réelle, p.ex.
            //   var session = appModel.createSessionForApp(index)
            //   stackView.push("qrc:/gui/StreamSegue.qml", { session: session, appName: gamesModel.get(index).title })
            console.log("Lancer le jeu :", gamesModel.get(index).title)
            launchToast.show(gamesModel.get(index).title)
        }
    }

    // --- Bloc détail du jeu sélectionné + bouton Jouer ---
    Item {
        id: detail
        anchors { top: carousel.bottom; topMargin: 16; left: parent.left; right: parent.right }
        height: 64

        Column {
            anchors.left: parent.left; anchors.leftMargin: 30
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            Text {
                text: home.currentGame().title
                color: "white"; font.pixelSize: 22; font.weight: Font.Medium
            }
            Text {
                text: home.currentGame().genre + " \u00b7 diffus\u00e9 depuis votre PC"
                color: "#8b909c"; font.pixelSize: 13
            }
        }

        Rectangle {
            anchors.right: parent.right; anchors.rightMargin: 30
            anchors.verticalCenter: parent.verticalCenter
            width: playRow.width + 32; height: 40; radius: 8
            color: "#F2802A"

            Row {
                id: playRow
                anchors.centerIn: parent
                spacing: 8
                Canvas {
                    width: 14; height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.fillStyle = "#4A1B0C"
                        ctx.beginPath()
                        ctx.moveTo(2, 1); ctx.lineTo(13, 7); ctx.lineTo(2, 13)
                        ctx.closePath(); ctx.fill()
                    }
                }
                Text {
                    text: "Jouer"; color: "#4A1B0C"
                    font.pixelSize: 14; font.weight: Font.Medium
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: carousel.launchRequested(carousel.currentIndex)
            }
        }
    }

    // --- Légende manette ---
    ControllerLegend {
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
    }

    // --- Petit toast de démo (sera retiré en intégration réelle) ---
    Rectangle {
        id: launchToast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 72
        radius: 8
        color: "#1b1e26"
        border.color: "#F2802A"; border.width: 1
        width: toastText.width + 32; height: 40
        opacity: 0
        visible: opacity > 0
        Text { id: toastText; anchors.centerIn: parent; color: "white"; font.pixelSize: 14 }
        function show(name) { toastText.text = "Lancement de " + name + "\u2026"; toastAnim.restart() }
        SequentialAnimation {
            id: toastAnim
            NumberAnimation { target: launchToast; property: "opacity"; to: 1; duration: 150 }
            PauseAnimation { duration: 1100 }
            NumberAnimation { target: launchToast; property: "opacity"; to: 0; duration: 300 }
        }
    }

    Keys.onEscapePressed: console.log("Retour")
}
