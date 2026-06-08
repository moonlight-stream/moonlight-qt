import QtQuick

import ComputerModel 1.0
import AppModel 1.0
import ComputerManager 1.0

// Écran d'accueil "Big Picture" de la console.
// Branché sur les vrais modèles Moonlight (ComputerModel + AppModel) ;
// le bouton Jouer crée une vraie session via StreamSegue.qml.
FocusScope {
    id: home
    focus: true

    implicitWidth: 1280
    implicitHeight: 720

    // Fond "écran" sombre
    Rectangle { anchors.fill: parent; color: "#0d0f14" }

    // --- Modèles Moonlight ---
    // Liste des hosts connus (mDNS + manuels). Toujours instancié.
    property var computerModel: Qt.createQmlObject(
        'import ComputerModel 1.0; ComputerModel {}', home, '')

    // Index du host actif (= premier online+paired, fallback 0, -1 si vide).
    // Calculé via l'Instantiator `hostScanner` plus bas.
    property int activeComputerIndex: -1

    // Liste des jeux du host actif. Recréée à chaque changement de host.
    property var appModel: null

    // Représentation "vue" du host actif (lu via l'Instantiator).
    property string activeHostName: ""
    property bool   activeHostOnline: false
    property bool   activeHostPaired: false

    Component.onCompleted: {
        computerModel.initialize(ComputerManager)
    }

    onActiveComputerIndexChanged: rebuildAppModel()

    function rebuildAppModel() {
        if (activeComputerIndex >= 0) {
            appModel = Qt.createQmlObject(
                'import AppModel 1.0; AppModel {}', home, '')
            appModel.initialize(ComputerManager, activeComputerIndex, false)
        } else {
            appModel = null
        }
    }

    function currentApp() {
        if (!appModel || appModel.count === 0) return null
        var idx = Math.max(0, Math.min(carousel.currentIndex, appModel.count - 1))
        var item = hostScannerForApps.objectAt(idx)
        return item ? item : null
    }

    // --- Scanner caché des hosts : met à jour activeComputerIndex en continu ---
    Instantiator {
        id: hostScanner
        model: home.computerModel
        delegate: QtObject {
            readonly property bool isOnline: model.online
            readonly property bool isPaired: model.paired
            readonly property string hostName: model.name
            readonly property bool isCandidate: isOnline && isPaired
            onIsCandidateChanged: home.pickActiveComputer()
        }
        onObjectAdded: function(index, obj) { home.pickActiveComputer() }
        onObjectRemoved: function(index, obj) { home.pickActiveComputer() }
    }

    function pickActiveComputer() {
        // Premier candidat online+paired, sinon premier online, sinon 0, sinon -1.
        var fallback = computerModel.count > 0 ? 0 : -1
        var firstOnline = -1
        for (var i = 0; i < hostScanner.count; i++) {
            var it = hostScanner.objectAt(i)
            if (!it) continue
            if (it.isCandidate) {
                if (activeComputerIndex !== i) activeComputerIndex = i
                updateActiveHostView(it)
                return
            }
            if (firstOnline < 0 && it.isOnline) firstOnline = i
        }
        var pick = firstOnline >= 0 ? firstOnline : fallback
        if (activeComputerIndex !== pick) activeComputerIndex = pick
        var sel = pick >= 0 ? hostScanner.objectAt(pick) : null
        updateActiveHostView(sel)
    }

    function updateActiveHostView(it) {
        if (it) {
            activeHostName = it.hostName
            activeHostOnline = it.isOnline
            activeHostPaired = it.isPaired
        } else {
            activeHostName = ""
            activeHostOnline = false
            activeHostPaired = false
        }
    }

    // Instantiator pour exposer les rôles d'AppModel à currentApp().
    Instantiator {
        id: hostScannerForApps
        model: home.appModel
        delegate: QtObject {
            readonly property string name: model.name
            readonly property bool running: model.running
            readonly property int appid: model.appid
            readonly property url boxart: model.boxart
        }
    }

    // --- Bandeau supérieur ---
    StatusBar {
        id: status
        anchors { top: parent.top; left: parent.left; right: parent.right }
        hostName: home.activeHostName !== "" ? home.activeHostName : qsTr("Recherche…")
        connected: home.activeHostOnline && home.activeHostPaired
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
            text: qsTr("Vos jeux"); color: "#8b909c"; font.pixelSize: 14
        }
        Text {
            anchors.right: parent.right; anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            text: appModel && appModel.count > 0
                  ? (carousel.currentIndex + 1) + " / " + appModel.count
                  : ""
            color: "#6b7280"; font.pixelSize: 14
        }
    }

    // --- Carrousel ---
    GameCarousel {
        id: carousel
        anchors { top: labelRow.bottom; topMargin: 12; left: parent.left; right: parent.right }
        height: focusedH + 30
        model: home.appModel
        focus: true
        visible: appModel && appModel.count > 0

        onLaunchRequested: function(index) {
            if (!appModel) return
            var app = home.currentApp()
            if (!app) return
            var component = Qt.createComponent("qrc:/gui/StreamSegue.qml")
            var segue = component.createObject(stackView, {
                "appName": app.name,
                "session": appModel.createSessionForApp(index),
                "isResume": false
            })
            stackView.push(segue)
        }
    }

    // --- État vide : pas (encore) de host paired / pas de jeux ---
    Item {
        anchors {
            top: labelRow.bottom; topMargin: 12; bottom: detail.top
            left: parent.left; right: parent.right
        }
        visible: !carousel.visible

        Text {
            anchors.centerIn: parent
            color: "#8b909c"; font.pixelSize: 18
            horizontalAlignment: Text.AlignHCenter
            text: !home.activeHostOnline
                  ? qsTr("Recherche de votre PC…")
                  : !home.activeHostPaired
                    ? qsTr("Appairage en cours…")
                    : qsTr("Aucun jeu disponible")
        }
    }

    // --- Bloc détail du jeu sélectionné + bouton Jouer ---
    Item {
        id: detail
        anchors { bottom: legend.top; bottomMargin: 12; left: parent.left; right: parent.right }
        height: 64
        visible: carousel.visible

        Column {
            anchors.left: parent.left; anchors.leftMargin: 30
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            Text {
                text: { var a = home.currentApp(); return a ? a.name : "" }
                color: "white"; font.pixelSize: 22; font.weight: Font.Medium
            }
            Text {
                text: {
                    var a = home.currentApp()
                    if (!a) return ""
                    return a.running
                        ? qsTr("En cours · diffusé depuis votre PC")
                        : qsTr("Diffusé depuis votre PC")
                }
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
                    text: qsTr("Jouer"); color: "#4A1B0C"
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
        id: legend
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
    }

    Keys.onEscapePressed: console.log("Retour")
}
