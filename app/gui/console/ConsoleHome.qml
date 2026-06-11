import QtQuick

import ComputerModel 1.0
import AppModel 1.0
import ComputerManager 1.0

// Écran d'accueil "Big Picture" de la console.
// Branché sur les vrais modèles Moonlight (ComputerModel + AppModel) :
//  - découverte du host (mDNS, polling lancé par main.qml) ;
//  - appairage automatique style "app TV" (code affiché, accept côté PC) ;
//  - lancement direct si le host est en mode direct-launch (cf. AppView.qml) ;
//  - bouton Jouer = vraie session via StreamSegue.qml.
FocusScope {
    id: home
    focus: true

    implicitWidth: 1280
    implicitHeight: 720

    // --- Fond : dégradé sombre + halo ambiant + vignette ---
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#12151d" }
            GradientStop { position: 0.55; color: "#0d0f15" }
            GradientStop { position: 1.0; color: "#090a0e" }
        }
    }

    Canvas {
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            // Halo orange très diffus derrière le carrousel
            var glow = ctx.createRadialGradient(width / 2, height * 0.46, 0,
                                                width / 2, height * 0.46, width * 0.55)
            glow.addColorStop(0, "rgba(242, 128, 42, 0.07)")
            glow.addColorStop(1, "rgba(242, 128, 42, 0)")
            ctx.fillStyle = glow
            ctx.fillRect(0, 0, width, height)
            // Vignette : assombrit doucement les bords de l'écran
            var vig = ctx.createRadialGradient(width / 2, height / 2, height * 0.35,
                                               width / 2, height / 2, width * 0.75)
            vig.addColorStop(0, "rgba(0, 0, 0, 0)")
            vig.addColorStop(1, "rgba(0, 0, 0, 0.4)")
            ctx.fillStyle = vig
            ctx.fillRect(0, 0, width, height)
        }
    }

    // --- Modèles Moonlight ---
    // Liste des hosts connus (mDNS + manuels).
    // IMPORTANT : initialize() remplit le modèle SANS émettre de reset ;
    // il faut donc créer + initialiser AVANT d'assigner la propriété (sinon
    // les vues se lient à un modèle vu comme vide, pour toujours).
    property var computerModel: null

    // Index du host actif (= premier online+paired, fallback 0, -1 si vide).
    // Calculé via l'Instantiator `hostScanner` plus bas.
    property int activeComputerIndex: -1

    // Liste des jeux du host actif. Recréée à chaque changement de host.
    property var appModel: null

    // Représentation "vue" du host actif (lu via l'Instantiator).
    property string activeHostName: ""
    property bool   activeHostOnline: false
    property bool   activeHostPaired: false

    // --- État de l'appairage automatique ---
    property string pairingPin: ""
    property bool   pairingInFlight: false
    property string pairingError: ""

    // Lancement direct (mode "direct launch" du host) : une seule fois par session.
    property bool directLaunchDone: false

    Component.onCompleted: {
        // Mode console : masquer la barre d'outils Material de main.qml
        // (élément "bureau" interdit par le principe zéro-friction).
        // Fait d'ici pour ne pas modifier le fichier upstream.
        var win = home.Window.window
        if (win && win.header)
            win.header.visible = false

        var m = Qt.createQmlObject(
            'import ComputerModel 1.0; ComputerModel {}', home, '')
        m.initialize(ComputerManager)
        m.pairingCompleted.connect(handlePairingCompleted)
        computerModel = m
    }

    onActiveComputerIndexChanged: rebuildAppModel()
    onActiveHostOnlineChanged: maybeStartPairing()
    onActiveHostPairedChanged: maybeStartPairing()

    function rebuildAppModel() {
        if (activeComputerIndex >= 0) {
            // Même contrainte que pour computerModel : initialiser AVANT
            // d'assigner, sinon les vues se lient à un modèle "vide".
            var m = Qt.createQmlObject(
                'import AppModel 1.0; AppModel {}', home, '')
            m.initialize(ComputerManager, activeComputerIndex, false)
            appModel = m
        } else {
            appModel = null
        }
    }

    function currentApp() {
        if (!appModel || appModel.count === 0) return null
        var idx = Math.max(0, Math.min(carousel.currentIndex, appModel.count - 1))
        var item = appViewer.objectAt(idx)
        return item ? item : null
    }

    // --- Appairage automatique ("silent pair" côté console, cf CLAUDE.md §9 6b) ---
    // Dès qu'un host en ligne mais non appairé devient actif, on génère un PIN
    // et on lance l'appairage en arrière-plan. L'utilisateur ne voit qu'un code
    // de liaison plein écran (PairingOverlay), à saisir une fois côté PC.
    // À terme, le host auto-acceptera et cet écran ne s'affichera jamais.
    function maybeStartPairing() {
        if (pairingInFlight) return
        if (activeComputerIndex < 0 || !activeHostOnline || activeHostPaired) return
        pairingError = ""
        pairingPin = computerModel.generatePinString()
        pairingInFlight = true
        console.info("[console-ui] Appairage automatique avec \"" + activeHostName + "\"")
        computerModel.pairComputer(activeComputerIndex, pairingPin)
    }

    function handlePairingCompleted(error) {
        pairingInFlight = false
        if (error !== undefined && error !== null) {
            // Échec (refus, timeout, jeu en cours…) : nouvelle tentative avec
            // un nouveau code après une courte pause.
            pairingError = "" + error
            pairingRetryTimer.restart()
        } else {
            pairingPin = ""
            pairingError = ""
        }
    }

    Timer {
        id: pairingRetryTimer
        interval: 5000
        onTriggered: home.maybeStartPairing()
    }

    // --- Scanner caché des hosts : met à jour activeComputerIndex en continu ---
    Instantiator {
        id: hostScanner
        model: home.computerModel
        delegate: QtObject {
            readonly property bool isOnline: model.online
            readonly property bool isPaired: model.paired
            readonly property string hostName: model.name
            // Chaque changement de rôle relance la sélection : un host qui
            // passe online (même non appairé) doit rafraîchir l'affichage.
            onIsOnlineChanged: home.pickActiveComputer()
            onIsPairedChanged: home.pickActiveComputer()
            onHostNameChanged: home.pickActiveComputer()
        }
        onObjectAdded: function(index, obj) { home.pickActiveComputer() }
        onObjectRemoved: function(index, obj) { home.pickActiveComputer() }
    }

    function pickActiveComputer() {
        // Premier candidat online+paired, sinon premier online, sinon 0, sinon -1.
        var fallback = (computerModel && computerModel.count > 0) ? 0 : -1
        var firstOnline = -1
        for (var i = 0; i < hostScanner.count; i++) {
            var it = hostScanner.objectAt(i)
            if (!it) continue
            if (it.isOnline && it.isPaired) {
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

    // Instantiator pour exposer les rôles d'AppModel à currentApp() / launchApp().
    Instantiator {
        id: appViewer
        model: home.appModel
        delegate: QtObject {
            readonly property string name: model.name
            readonly property bool running: model.running
            readonly property int appid: model.appid
            readonly property url boxart: model.boxart
        }
        // À la première arrivée des jeux : lancement direct éventuel (§9 6a).
        onObjectAdded: function(index, obj) { home.maybeDirectLaunch() }
    }

    function maybeDirectLaunch() {
        if (directLaunchDone || !appModel) return
        var idx = appModel.getDirectLaunchAppIndex()
        if (idx >= 0) {
            directLaunchDone = true
            carousel.currentIndex = idx
            launchApp(idx)
        }
    }

    function launchApp(index) {
        if (!appModel) return
        var item = appViewer.objectAt(index)
        if (!item) return
        var component = Qt.createComponent("qrc:/gui/StreamSegue.qml")
        var segue = component.createObject(stackView, {
            "appName": item.name,
            "session": appModel.createSessionForApp(index),
            "isResume": item.running
        })
        stackView.push(segue)
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
        visible: carousel.visible
        Text {
            anchors.left: parent.left; anchors.leftMargin: 28
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("VOS JEUX")
            color: "#8b909c"; font.pixelSize: 12
            font.letterSpacing: 2; font.weight: Font.DemiBold
        }
        Text {
            anchors.right: parent.right; anchors.rightMargin: 28
            anchors.verticalCenter: parent.verticalCenter
            text: appModel && appModel.count > 0
                  ? (carousel.currentIndex + 1) + " / " + appModel.count
                  : ""
            color: "#6b7280"; font.pixelSize: 13
        }
    }

    // --- Carrousel ---
    GameCarousel {
        id: carousel
        anchors { top: labelRow.bottom; topMargin: 10; left: parent.left; right: parent.right }
        height: focusedH + 46
        model: home.appModel
        focus: true
        visible: appModel && appModel.count > 0
        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 250 } }

        onLaunchRequested: function(index) { home.launchApp(index) }
    }

    // --- États intermédiaires : recherche / appairage / chargement ---
    Item {
        id: emptyState
        anchors {
            top: status.bottom; bottom: detail.top
            left: parent.left; right: parent.right
        }
        visible: !carousel.visible

        // Recherche du PC ou chargement de la bibliothèque
        Column {
            anchors.centerIn: parent
            spacing: 22
            visible: !pairingScreen.visible

            Spinner {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 44; height: 44
            }
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "#e8eaf0"; font.pixelSize: 20; font.weight: Font.Medium
                    text: !home.activeHostOnline
                          ? qsTr("Recherche de votre PC…")
                          : qsTr("Chargement de vos jeux…")
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "#6b7280"; font.pixelSize: 14
                    visible: !home.activeHostOnline
                    text: qsTr("Vérifiez que votre PC est allumé et sur le même réseau.")
                }
            }
        }

        // Liaison (code PIN à saisir côté PC)
        PairingOverlay {
            id: pairingScreen
            anchors.fill: parent
            visible: home.activeHostOnline && !home.activeHostPaired
                     && home.pairingPin !== ""
            hostName: home.activeHostName
            pin: home.pairingPin
            errorText: home.pairingError
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
            id: playButton
            anchors.right: parent.right; anchors.rightMargin: 30
            anchors.verticalCenter: parent.verticalCenter
            width: playRow.width + 36; height: 42; radius: 10
            scale: playMouse.pressed ? 0.96 : 1.0
            Behavior on scale { NumberAnimation { duration: 80 } }
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#F68A38" }
                GradientStop { position: 1.0; color: "#E5721C" }
            }

            // Liseré clair en haut, pour le relief
            Rectangle {
                anchors { top: parent.top; left: parent.left; right: parent.right }
                anchors.margins: 1
                height: 1; radius: parent.radius
                color: "#40ffffff"
            }

            Row {
                id: playRow
                anchors.centerIn: parent
                spacing: 9
                Canvas {
                    width: 13; height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = "#3A1505"
                        ctx.beginPath()
                        ctx.moveTo(1, 1); ctx.lineTo(12, 7); ctx.lineTo(1, 13)
                        ctx.closePath(); ctx.fill()
                    }
                }
                Text {
                    text: {
                        var a = home.currentApp()
                        return a && a.running ? qsTr("Reprendre") : qsTr("Jouer")
                    }
                    color: "#3A1505"
                    font.pixelSize: 15; font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: playMouse
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
