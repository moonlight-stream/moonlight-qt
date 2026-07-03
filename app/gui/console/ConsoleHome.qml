import QtQuick
import QtCore
// Nécessaire pour la propriété attachée StackView.onActivated
import QtQuick.Controls

import ComputerModel 1.0
import AppModel 1.0
import ComputerManager 1.0
import StreamingPreferences 1.0
import CompanionClient 1.0

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
    property bool   activeHostStatusUnknown: true

    // --- État de l'appairage automatique ---
    property string pairingPin: ""
    property bool   pairingInFlight: false
    property string pairingError: ""

    // Lancement direct (mode "direct launch" du host) : une seule fois par session.
    property bool directLaunchDone: false

    // Marqueur : le profil de stream par défaut n'est appliqué qu'au premier
    // démarrage console ; ensuite l'écran Paramètres (bouton Y) fait foi.
    Settings {
        id: consoleConfig
        category: "ConsoleUi"
        property bool streamProfileInitialized: false
    }

    Component.onCompleted: {
        hideUpstreamChrome()

        // Démarre la découverte du HostCompanion (mDNS _hostcompanion._tcp).
        // Si un host appairé est déjà connu, le client se reconnecte tout seul
        // (events + bibliothèque) dès qu'il est joignable.
        CompanionClient.startDiscovery()

        // Profil de stream "console" par défaut : 1080p60, 30 Mbps (cible §7 :
        // 30–50 Mbps stables ; le défaut Moonlight pour du 1080p60 est en
        // dessous). Appliqué une seule fois — modifiable ensuite via le
        // bouton Y (ConsoleSettings).
        if (!consoleConfig.streamProfileInitialized) {
            StreamingPreferences.width = 1920
            StreamingPreferences.height = 1080
            StreamingPreferences.fps = 60
            StreamingPreferences.bitrateKbps = Math.max(
                StreamingPreferences.getDefaultBitrate(1920, 1080, 60,
                                                       StreamingPreferences.enableYUV444),
                30000)
            StreamingPreferences.save()
            consoleConfig.streamProfileInitialized = true
        }

        var m = Qt.createQmlObject(
            'import ComputerModel 1.0; ComputerModel {}', home, '')
        m.initialize(ComputerManager)
        m.pairingCompleted.connect(handlePairingCompleted)
        computerModel = m
    }

    // StreamSegue/QuitSegue ré-affichent la toolbar upstream en se dépilant :
    // on re-masque le chrome à chaque retour sur l'accueil.
    StackView.onActivated: {
        hideUpstreamChrome()
        carousel.forceActiveFocus()
    }

    function hideUpstreamChrome() {
        // Mode console : masquer la barre d'outils Material de main.qml
        // (élément "bureau" interdit par le principe zéro-friction).
        // Fait d'ici pour ne pas modifier le fichier upstream.
        var win = home.Window.window
        if (win && win.header)
            win.header.visible = false
    }

    // --- Boutons console au niveau de l'accueil ---
    // Y/Start (Hangup) et X (Menu) ouvrent NOS paramètres ; sans ça, les
    // événements remonteraient à main.qml qui ouvrirait la SettingsView
    // Material du bureau. B (Échap) est consommé : rien derrière l'accueil.
    Keys.onMenuPressed: settingsOverlay.open()
    Keys.onHangupPressed: settingsOverlay.open()
    Keys.onEscapePressed: { /* accueil : rien à fermer */ }

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

    // NB : AppModel/ComputerModel (C++) n'exposent PAS de propriété `count` ;
    // on passe toujours par le count des Instantiator (appViewer/hostScanner).
    function currentApp() {
        if (appViewer.count === 0) return null
        var idx = Math.max(0, Math.min(carousel.currentIndex, appViewer.count - 1))
        var item = appViewer.objectAt(idx)
        return item ? item : null
    }

    // --- Appairage automatique ("silent pair" côté console, cf CLAUDE.md §9 6b) ---
    // Dès qu'un host en ligne mais non appairé devient actif, on génère un PIN
    // et on lance l'appairage en arrière-plan. L'utilisateur ne voit qu'un code
    // de liaison plein écran (PairingOverlay), à saisir une fois côté PC.
    // À terme, le host auto-acceptera et cet écran ne s'affichera jamais.
    //
    // Garde-fou : au boot, le host passe online AVANT que son pairState soit
    // confirmé (statusUnknown / bref "non appairé" le temps du serverinfo
    // HTTPS). On exige donc un état "online + non appairé" STABLE pendant
    // 2,5 s avant de déclencher — sinon on enverrait une demande de PIN
    // parasite à un host déjà appairé.
    function maybeStartPairing() {
        if (pairingNeeded())
            pairingArmTimer.start()
        else
            pairingArmTimer.stop()
    }

    function pairingNeeded() {
        return !pairingInFlight
            && activeComputerIndex >= 0
            && activeHostOnline
            && !activeHostPaired
            && !activeHostStatusUnknown
    }

    Timer {
        id: pairingArmTimer
        interval: 2500
        onTriggered: {
            if (!home.pairingNeeded()) return
            home.pairingError = ""
            home.pairingPin = home.computerModel.generatePinString()
            home.pairingInFlight = true
            console.info("[console-ui] Appairage automatique avec \"" + home.activeHostName + "\"")
            home.computerModel.pairComputer(home.activeComputerIndex, home.pairingPin)
            // Si le Companion est déjà appairé, il soumet le PIN à Apollo à notre
            // place → l'utilisateur n'a rien à faire côté PC (§6.4). Sinon, repli :
            // le PairingOverlay affiche le PIN à saisir sur le PC.
            if (CompanionClient.paired)
                CompanionClient.submitMoonlightPin(home.pairingPin)
        }
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
            readonly property bool isUnknown: model.statusUnknown
            readonly property string hostName: model.name
            // Chaque changement de rôle relance la sélection : un host qui
            // passe online (même non appairé) doit rafraîchir l'affichage.
            onIsOnlineChanged: home.pickActiveComputer()
            onIsPairedChanged: home.pickActiveComputer()
            onIsUnknownChanged: home.pickActiveComputer()
            onHostNameChanged: home.pickActiveComputer()
        }
        onObjectAdded: function(index, obj) { home.pickActiveComputer() }
        onObjectRemoved: function(index, obj) { home.pickActiveComputer() }
    }

    function pickActiveComputer() {
        // Premier candidat online+paired, sinon premier online, sinon 0, sinon -1.
        var fallback = hostScanner.count > 0 ? 0 : -1
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
            activeHostStatusUnknown = it.isUnknown
        } else {
            activeHostName = ""
            activeHostOnline = false
            activeHostPaired = false
            activeHostStatusUnknown = true
        }
        maybeStartPairing()
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

    // Index du jeu sélectionné en attente d'un READY du Companion (mapping après maj).
    property int pendingLaunchIndex: -1

    function findAppIndexByName(name) {
        for (var i = 0; i < appViewer.count; i++) {
            var it = appViewer.objectAt(i)
            if (it && it.name === name) return i
        }
        return -1
    }

    function humanSize(bytes) {
        if (!bytes || bytes <= 0) return ""
        var u = ["o", "Ko", "Mo", "Go", "To"]
        var i = 0; var v = bytes
        while (v >= 1024 && i < u.length - 1) { v /= 1024; i++ }
        return (i >= 2 ? v.toFixed(1) : Math.round(v)) + " " + u[i]
    }

    function launchApp(index) {
        if (!appModel) return
        if (launchOverlay.visible) return   // un lancement est déjà en cours
        var item = appViewer.objectAt(index)
        if (!item) return

        // Un AUTRE jeu tourne déjà sur le host : demander confirmation
        // (équivalent console du quitAppDialog de AppView.qml). Chemin direct.
        var runningId = appModel.getRunningAppId()
        if (runningId !== 0 && runningId !== item.appid) {
            confirmDialog.mode = "quitLaunch"
            confirmDialog.pendingLaunchIndex = index
            confirmDialog.title = qsTr("Un jeu est déjà en cours")
            confirmDialog.message = qsTr("%1 est en cours sur votre PC. Le fermer et lancer %2 ? Toute progression non sauvegardée sera perdue.")
                .arg(appModel.getRunningAppName()).arg(item.name)
            confirmDialog.confirmLabel = qsTr("Fermer et jouer")
            confirmDialog.open()
            return
        }

        // Chemin Companion = LE différenciateur : POST /v1/launch → (maj si besoin)
        // → READY → stream. On NE démarre PAS le stream nous-mêmes ici (§4 host).
        // Repli direct si le Companion est absent ou ne connaît pas ce jeu.
        if (CompanionClient.paired && CompanionClient.eventsConnected) {
            var gameId = CompanionClient.gameIdForName(item.name)
            if (gameId !== "") {
                home.pendingLaunchIndex = index
                launchOverlay.show(item.name)
                CompanionClient.launch(gameId)
                return
            }
        }

        directLaunch(index)
    }

    // Lancement direct historique (Apollo sans orchestration Companion) — repli.
    function directLaunch(index) {
        if (!appModel) return
        var item = appViewer.objectAt(index)
        if (!item) return
        var runningId = appModel.getRunningAppId()
        var component = Qt.createComponent("qrc:/gui/StreamSegue.qml")
        var segue = component.createObject(stackView, {
            "appName": item.name,
            "session": appModel.createSessionForApp(index),
            "isResume": runningId === item.appid
        })
        stackView.push(segue)
    }

    // READY reçu du Companion : l'app Apollo du jeu est prête (virtual display armé)
    // → on démarre enfin la session Moonlight sur cette app (mapping par nom, §4).
    function streamReadyApp(apolloAppId) {
        launchOverlay.hide()
        var idx = findAppIndexByName(apolloAppId)
        if (idx < 0) idx = home.pendingLaunchIndex
        home.pendingLaunchIndex = -1
        if (idx < 0 || !appModel) return
        var item = appViewer.objectAt(idx)
        var component = Qt.createComponent("qrc:/gui/StreamSegue.qml")
        var segue = component.createObject(stackView, {
            "appName": item ? item.name : "",
            "session": appModel.createSessionForApp(idx),
            "isResume": false
        })
        stackView.push(segue)
    }

    // Ferme le jeu en cours puis enchaîne sur le nouveau, via le QuitSegue
    // upstream (gère le quit côté host et le chaînage nextSession).
    function quitAndLaunch(nextIndex) {
        if (!appModel) return
        var item = appViewer.objectAt(nextIndex)
        var component = Qt.createComponent("qrc:/gui/QuitSegue.qml")
        stackView.push(component.createObject(stackView, {
            "appName": appModel.getRunningAppName(),
            "quitRunningAppFn": function() { appModel.quitRunningApp() },
            "nextAppName": item ? item.name : null,
            "nextSession": item ? appModel.createSessionForApp(nextIndex) : null
        }))
    }

    // --- Bandeau supérieur ---
    StatusBar {
        id: status
        anchors { top: parent.top; left: parent.left; right: parent.right }
        hostName: home.activeHostName !== "" ? home.activeHostName : qsTr("Recherche…")
        connected: home.activeHostOnline && home.activeHostPaired
        // batteryPercent / signalStrength : laissés cachés tant qu'il n'y a
        // pas de vraie source de données (UPower viendra avec le proto).
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
            text: appViewer.count > 0
                  ? (carousel.currentIndex + 1) + " / " + appViewer.count
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
        visible: appViewer.count > 0
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
            // Repli : visible seulement si le Companion n'est PAS appairé (sinon il
            // soumet le PIN à Apollo tout seul → aucun code à saisir côté PC).
            visible: home.activeHostOnline && !home.activeHostPaired
                     && home.pairingPin !== "" && !CompanionClient.paired
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

    // --- Légende manette (contextuelle) ---
    ControllerLegend {
        id: legend
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        hints: confirmDialog.visible
               ? [ { btn: "A", label: qsTr("Valider"),  color: "#6cc04a" },
                   { btn: "B", label: qsTr("Annuler"),  color: "#e35b5b" } ]
               : settingsOverlay.visible
                 ? [ { btn: "A", label: qsTr("Modifier"),   color: "#6cc04a" },
                     { btn: "B", label: qsTr("Fermer"),     color: "#e35b5b" } ]
                 : carousel.visible
                   ? [ { btn: "A", label: qsTr("Jouer"),      color: "#6cc04a" },
                       { btn: "Y", label: qsTr("Paramètres"), color: "#e0b020" } ]
                   : [ { btn: "Y", label: qsTr("Paramètres"), color: "#e0b020" } ]
    }

    // --- Overlays console (au-dessus de tout) ---
    ConsoleSettings {
        id: settingsOverlay
        anchors.fill: parent
        hostName: home.activeHostName
        onForgetRequested: {
            confirmDialog.mode = "forget"
            confirmDialog.title = qsTr("Oublier ce PC ?")
            confirmDialog.message = qsTr("« %1 » sera supprimé de la console. Il faudra refaire la liaison (code à saisir sur le PC).").arg(home.activeHostName)
            confirmDialog.confirmLabel = qsTr("Oublier")
            confirmDialog.open()
        }
        onClosed: carousel.forceActiveFocus()
    }

    ConsoleDialog {
        id: confirmDialog
        anchors.fill: parent
        property string mode: ""
        property int pendingLaunchIndex: -1
        onConfirmed: {
            if (mode === "forget") {
                settingsOverlay.close()
                if (home.activeComputerIndex >= 0)
                    home.computerModel.deleteComputer(home.activeComputerIndex)
            } else if (mode === "quitLaunch") {
                home.quitAndLaunch(pendingLaunchIndex)
            }
        }
        onClosed: {
            if (settingsOverlay.visible)
                settingsOverlay.forceActiveFocus()
            else
                carousel.forceActiveFocus()
        }
    }

    // --- Mise à jour requise (dialog dédié, piloté par le Companion, §9.1) ---
    ConsoleDialog {
        id: updateDialog
        anchors.fill: parent
        z: 60
        confirmLabel: qsTr("Mettre à jour")
        cancelLabel: qsTr("Annuler")
        // Distingue « confirmé » de « simplement fermé » : ConsoleDialog émet
        // confirmed() PUIS closed() quand on valide → sans ce flag on enverrait
        // accept=true puis accept=false.
        property bool answered: false
        onConfirmed: { answered = true; CompanionClient.respondUpdate(true) }
        onClosed: {
            if (!answered) {
                CompanionClient.respondUpdate(false)   // refus = retour carrousel (§9.1 ABORTED)
                launchOverlay.hide()
                home.pendingLaunchIndex = -1
                carousel.forceActiveFocus()
            } else if (launchOverlay.visible) {
                launchOverlay.forceActiveFocus()       // garde B actif pendant la maj
            }
        }
    }

    // --- Overlay de lancement (préparation / maj en cours / erreur) ---
    LaunchOverlay {
        id: launchOverlay
        anchors.fill: parent
        z: 50
        onCancelRequested: {
            if (phase !== "error")
                CompanionClient.cancelLaunch()
            home.pendingLaunchIndex = -1
            hide()
            carousel.forceActiveFocus()
        }
    }

    // --- Saisie du code d'appairage Companion (6 chiffres, host → console) ---
    CompanionPairing {
        id: companionPairing
        anchors.fill: parent
        z: 70
        // « Découvert mais pas appairé » : on a un fingerprint live mais pas de token.
        visible: CompanionClient.certFingerprint !== "" && !CompanionClient.paired
        hostName: home.activeHostName !== "" ? home.activeHostName : CompanionClient.hostName
        // À l'apparition : on demande au host de GÉNÉRER + AFFICHER son code (pair/start).
        // L'utilisateur le lit sur le PC et le saisit ici ; la validation = pair/confirm.
        onVisibleChanged: if (visible) {
            reset()
            forceActiveFocus()
            CompanionClient.startPairing(qsTr("Console"))
        }
        onSubmitted: function(code) { CompanionClient.confirmPairing(code) }
        onCancelled: { /* la découverte continue ; rien à fermer côté console */ }
    }

    // --- Branchement des événements Companion (REST + WS) ---
    Connections {
        target: CompanionClient

        function onPairingFailed(code) {
            companionPairing.busy = false
            companionPairing.errorText = (code === "PAIR_EXPIRED")
                ? qsTr("Code expiré — relancez la liaison depuis le PC.")
                : qsTr("Code incorrect — réessayez.")
        }
        function onPairingSucceeded() {
            companionPairing.errorText = ""
            // Si un appairage Moonlight est en cours, soumettre son PIN maintenant.
            if (home.pairingInFlight && home.pairingPin !== "")
                CompanionClient.submitMoonlightPin(home.pairingPin)
        }

        function onLaunchStateChanged(sessionId, gameId, state) {
            if (!launchOverlay.visible) return
            if (state === "CHECKING" || state === "ARMING" || state === "UP_TO_DATE")
                launchOverlay.phase = "preparing"
            else if (state === "STREAMING")
                launchOverlay.hide()
        }
        function onUpdateRequired(sessionId, gameId, sizeBytes) {
            updateDialog.title = qsTr("Mise à jour requise")
            updateDialog.message = sizeBytes > 0
                ? qsTr("Ce jeu doit être mis à jour (%1) avant d'y jouer.").arg(home.humanSize(sizeBytes))
                : qsTr("Ce jeu doit être mis à jour avant d'y jouer.")
            updateDialog.answered = false
            updateDialog.open()
        }
        function onUpdateProgress(sessionId, pct, bytesDone, bytesTotal) {
            launchOverlay.phase = "updating"
            launchOverlay.pct = pct
        }
        function onLaunchReady(sessionId, apolloAppId) {
            home.streamReadyApp(apolloAppId)
        }
        function onLaunchFailed(sessionId, code, message) {
            home.pendingLaunchIndex = -1
            launchOverlay.phase = "error"
            launchOverlay.message = (message && message !== "") ? message : code
            launchOverlay.visible = true
            launchOverlay.forceActiveFocus()   // B ferme l'écran d'erreur
        }
    }
}
