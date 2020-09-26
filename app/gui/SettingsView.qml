import QtQuick 2.9
import QtQuick.Controls 2.2

import StreamingPreferences 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import SystemProperties 1.0

Flickable {
    id: settingsPage
    objectName: "Paramètres"

    boundsBehavior: Flickable.OvershootBounds

    contentWidth: settingsColumn1.width > settingsColumn2.width ? settingsColumn1.width : settingsColumn2.width
    contentHeight: settingsColumn1.height > settingsColumn2.height ? settingsColumn1.height : settingsColumn2.height

    ScrollBar.vertical: ScrollBar {
        anchors {
            left: parent.right
            leftMargin: -10
        }
    }

    StackView.onActivated: {
        // This enables Tab and BackTab based navigation rather than arrow keys.
        // It is required to shift focus between controls on the settings page.
        SdlGamepadKeyNavigation.setUiNavMode(true)

        // Highlight the first item if a gamepad is connected
        if (SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            resolutionComboBox.forceActiveFocus(Qt.TabFocus)
        }
    }

    StackView.onDeactivating: {
        SdlGamepadKeyNavigation.setUiNavMode(false)

        // Save the prefs so the Session can observe the changes
        StreamingPreferences.save()
    }

    Column {
        padding: 10
        id: settingsColumn1
        width: settingsPage.width / 2
        spacing: 15

        GroupBox {
            id: basicSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<font color=\"skyblue\">Paramètres de base</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                Label {
                    width: parent.width
                    id: resFPStitle
                    text: qsTr("Résolution et FPS")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                Label {
                    width: parent.width
                    id: resFPSdesc
                    text: qsTr("Le fait de fixer des valeurs trop élevées pour votre PC ou votre connexion réseau peut entraîner un décalage, des bégaiements ou des erreurs.")
                    font.pointSize: 9
                    wrapMode: Text.Wrap
                }

                Row {
                    spacing: 5
                    width: parent.width

                    AutoResizingComboBox {
                        // ignore setting the index at first, and actually set it when the component is loaded
                        Component.onCompleted: {
                            // Add native resolutions for all attached displays
                            var done = false
                            for (var displayIndex = 0; !done; displayIndex++) {
                                for (var displayResIndex = 0; displayResIndex < 2; displayResIndex++) {
                                    var screenRect;

                                    // Some platforms have different desktop resolutions
                                    // and native resolutions (like macOS with Retina displays)
                                    if (displayResIndex === 0) {
                                        screenRect = SystemProperties.getDesktopResolution(displayIndex)
                                    }
                                    else {
                                        screenRect = SystemProperties.getNativeResolution(displayIndex)
                                    }

                                    if (screenRect.width === 0) {
                                        // Exceeded max count of displays
                                        done = true
                                        break
                                    }

                                    var indexToAdd = 0
                                    for (var j = 0; j < resolutionComboBox.count; j++) {
                                        var existing_width = parseInt(resolutionListModel.get(j).video_width);
                                        var existing_height = parseInt(resolutionListModel.get(j).video_height);

                                        if (screenRect.width === existing_width && screenRect.height === existing_height) {
                                            // Duplicate entry, skip
                                            indexToAdd = -1
                                            break
                                        }
                                        else if (screenRect.width * screenRect.height > existing_width * existing_height) {
                                            // Candidate entrypoint after this entry
                                            indexToAdd = j + 1
                                        }
                                    }

                                    // Insert this display's resolution if it's not a duplicate
                                    if (indexToAdd >= 0) {
                                        resolutionListModel.insert(indexToAdd,
                                                                   {
                                                                       "text": "Native ("+screenRect.width+"x"+screenRect.height+")",
                                                                       "video_width": ""+screenRect.width,
                                                                       "video_height": ""+screenRect.height
                                                                   })
                                    }
                                }
                            }

                            // Prune resolutions that are over the decoder's maximum
                            var max_pixels = SystemProperties.maximumResolution.width * SystemProperties.maximumResolution.height;
                            if (max_pixels > 0) {
                                for (var j = 0; j < resolutionComboBox.count; j++) {
                                    var existing_width = parseInt(resolutionListModel.get(j).video_width);
                                    var existing_height = parseInt(resolutionListModel.get(j).video_height);

                                    if (existing_width * existing_height > max_pixels) {
                                        resolutionListModel.remove(j)
                                        j--
                                    }
                                }
                            }

                            // load the saved width/height, and iterate through the ComboBox until a match is found
                            // and set it to that index.
                            var saved_width = StreamingPreferences.width
                            var saved_height = StreamingPreferences.height
                            currentIndex = 0
                            for (var i = 0; i < resolutionListModel.count; i++) {
                                var el_width = parseInt(resolutionListModel.get(i).video_width);
                                var el_height = parseInt(resolutionListModel.get(i).video_height);

                                // Pick the highest value lesser or equal to the saved resolution
                                if (saved_width * saved_height >= el_width * el_height) {
                                    currentIndex = i
                                }
                            }

                            // Persist the selected value
                            activated(currentIndex)
                        }

                        id: resolutionComboBox
                        maximumWidth: parent.width / 2
                        textRole: "text"
                        model: ListModel {
                            id: resolutionListModel
                            // Other elements may be added at runtime
                            // based on attached display resolution
                            ListElement {
                                text: "720p"
                                video_width: "1280"
                                video_height: "720"
                            }
                            ListElement {
                                text: "1080p"
                                video_width: "1920"
                                video_height: "1080"
                            }
                            ListElement {
                                text: "1440p"
                                video_width: "2560"
                                video_height: "1440"
                            }
                            ListElement {
                                text: "4K"
                                video_width: "3840"
                                video_height: "2160"
                            }
                        }
                        // ::onActivated must be used, as it only listens for when the index is changed by a human
                        onActivated : {
                            var selectedWidth = parseInt(resolutionListModel.get(currentIndex).video_width)
                            var selectedHeight = parseInt(resolutionListModel.get(currentIndex).video_height)

                            // Only modify the bitrate if the values actually changed
                            if (StreamingPreferences.width !== selectedWidth || StreamingPreferences.height !== selectedHeight) {
                                StreamingPreferences.width = selectedWidth
                                StreamingPreferences.height = selectedHeight

                                StreamingPreferences.bitrateKbps = StreamingPreferences.getDefaultBitrate(StreamingPreferences.width,
                                                                                                          StreamingPreferences.height,
                                                                                                          StreamingPreferences.fps);
                                slider.value = StreamingPreferences.bitrateKbps
                            }
                        }
                    }

                    AutoResizingComboBox {
                        function createModel() {
                            var fpsListModel = Qt.createQmlObject('import QtQuick 2.0; ListModel {}', parent, '')

                            var max_fps = SystemProperties.maximumStreamingFrameRate

                            // Default entries
                            fpsListModel.append({"text": "30 FPS", "video_fps": "30"})
                            fpsListModel.append({"text": "60 FPS", "video_fps": "60"})

                            // Add unsupported FPS values that come before the display max FPS
                            if (StreamingPreferences.unsupportedFps) {
                                if (max_fps > 90) {
                                    fpsListModel.append({"text": "90 FPS (Non pris en charge)", "video_fps": "90"})
                                }
                                if (max_fps > 120) {
                                    fpsListModel.append({"text": "120 FPS (Non pris en charge)", "video_fps": "120"})
                                }
                            }

                            // Use 64 as the cutoff for adding a separate option to
                            // handle wonky displays that report just over 60 Hz.
                            if (max_fps > 64) {
                                // Mark any FPS value greater than 120 as unsupported
                                if (StreamingPreferences.unsupportedFps && max_fps > 120) {
                                    fpsListModel.append({"text": max_fps+" FPS (Non pris en charge)", "video_fps": ""+max_fps})
                                }
                                else if (max_fps > 120) {
                                    fpsListModel.append({"text": "120 FPS", "video_fps": "120"})
                                }
                                else {
                                    fpsListModel.append({"text": max_fps+" FPS", "video_fps": ""+max_fps})
                                }
                            }

                            // Add unsupported FPS values that come after the display max FPS
                            if (StreamingPreferences.unsupportedFps) {
                                if (max_fps < 90) {
                                    fpsListModel.append({"text": "90 FPS (Non pris en charge)", "video_fps": "90"})
                                }
                                if (max_fps < 120) {
                                    fpsListModel.append({"text": "120 FPS (Non pris en charge)", "video_fps": "120"})
                                }
                            }

                            return fpsListModel
                        }

                        function reinitialize() {
                            model = createModel()

                            var saved_fps = StreamingPreferences.fps
                            currentIndex = 0
                            for (var i = 0; i < model.count; i++) {
                                var el_fps = parseInt(model.get(i).video_fps);

                                // Pick the highest value lesser or equal to the saved FPS
                                if (saved_fps >= el_fps) {
                                    currentIndex = i
                                }
                            }

                            // Persist the selected value
                            activated(currentIndex)
                        }

                        // ignore setting the index at first, and actually set it when the component is loaded
                        Component.onCompleted: {
                            reinitialize()
                        }

                        id: fpsComboBox
                        maximumWidth: parent.width / 2
                        textRole: "text"
                        // ::onActivated must be used, as it only listens for when the index is changed by a human
                        onActivated : {
                            var selectedFps = parseInt(model.get(currentIndex).video_fps)

                            // Only modify the bitrate if the values actually changed
                            if (StreamingPreferences.fps !== selectedFps) {
                                StreamingPreferences.fps = selectedFps

                                StreamingPreferences.bitrateKbps = StreamingPreferences.getDefaultBitrate(StreamingPreferences.width,
                                                                                                          StreamingPreferences.height,
                                                                                                          StreamingPreferences.fps);
                                slider.value = StreamingPreferences.bitrateKbps
                            }
                        }
                    }
                }

                Label {
                    width: parent.width
                    id: bitrateTitle
                    text: qsTr("Bitrate vidéo: ")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                Label {
                    width: parent.width
                    id: bitrateDesc
                    text: qsTr("Baissez le bitrate sur les connexions plus lentes. Augmentez le bitrate pour améliorer la qualité de l'image.")
                    font.pointSize: 9
                    wrapMode: Text.Wrap
                }

                Slider {
                    id: slider

                    value: StreamingPreferences.bitrateKbps

                    stepSize: 500
                    from : 500
                    to: 150000

                    snapMode: "SnapOnRelease"
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)

                    onValueChanged: {
                        bitrateTitle.text = "Bitrate vidéo: " + (value / 1000.0) + " Mbps"
                        StreamingPreferences.bitrateKbps = value
                    }
                }

                Label {
                    width: parent.width
                    id: windowModeTitle
                    text: qsTr("Mode d'affichage")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        // Set the recommended option based on the OS
                        for (var i = 0; i < windowModeListModel.count; i++) {
                             var thisWm = windowModeListModel.get(i).val;
                             if (thisWm === StreamingPreferences.recommendedFullScreenMode) {
                                 windowModeListModel.get(i).text += " (Recommandé)"
                                 windowModeListModel.move(i, 0, 1);
                                 break
                             }
                        }

                        currentIndex = 0

                        if (SystemProperties.hasWindowManager && !SystemProperties.rendererAlwaysFullScreen) {
                            var savedWm = StreamingPreferences.windowMode
                            for (var i = 0; i < windowModeListModel.count; i++) {
                                 var thisWm = windowModeListModel.get(i).val;
                                 if (savedWm === thisWm) {
                                     currentIndex = i
                                     break
                                 }
                            }
                        }

                        activated(currentIndex)
                    }

                    id: windowModeComboBox
                    enabled: SystemProperties.hasWindowManager && !SystemProperties.rendererAlwaysFullScreen
                    hoverEnabled: true
                    textRole: "text"
                    model: ListModel {
                        id: windowModeListModel
                        ListElement {
                            text: "Plein écran"
                            val: StreamingPreferences.WM_FULLSCREEN
                        }
                        ListElement {
                            text: "fenêtré sans bordure"
                            val: StreamingPreferences.WM_FULLSCREEN_DESKTOP
                        }
                        ListElement {
                            text: "Fenêtré"
                            val: StreamingPreferences.WM_WINDOWED
                        }
                    }
                    onActivated: {
                        StreamingPreferences.windowMode = windowModeListModel.get(currentIndex).val
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "Le plein écran offre généralement les meilleures performances, mais les fenêtres sans bordure peuvent fonctionner mieux avec des fonctionnalités telles que MacOS Spaces, Alt+Tab, les outils de capture d'écran, les superpositions à l'écran, etc."
                }

                CheckBox {
                    id: vsyncCheck
                    width: parent.width
                    hoverEnabled: true
                    text: "V-Sync"
                    font.pointSize:  12
                    checked: StreamingPreferences.enableVsync
                    onCheckedChanged: {
                        StreamingPreferences.enableVsync = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "La désactivation de V-Sync permet une latence de rendu des sous-images, mais elle peut afficher des déchirures visibles"
                }

                CheckBox {
                    id: framePacingCheck
                    width: parent.width
                    hoverEnabled: true
                    text: "Frame pacing"
                    font.pointSize:  12
                    enabled: StreamingPreferences.enableVsync
                    checked: StreamingPreferences.enableVsync && StreamingPreferences.framePacing
                    onCheckedChanged: {
                        StreamingPreferences.framePacing = checked
                    }
                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "Le rythme des images réduit le micro-bégaiement en retardant les images qui arrivent trop tôt"
                }
            }
        }

        GroupBox {

            id: audioSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<font color=\"skyblue\">Réglages audio</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                Label {
                    width: parent.width
                    id: resAudioTitle
                    text: qsTr("Configuration audio")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_audio = StreamingPreferences.audioConfig
                        currentIndex = 0
                        for (var i = 0; i < audioListModel.count; i++) {
                            var el_audio = audioListModel.get(i).val;
                            if (saved_audio === el_audio) {
                                currentIndex = i
                                break
                            }
                        }
                        activated(currentIndex)
                    }

                    id: audioComboBox
                    textRole: "text"
                    model: ListModel {
                        id: audioListModel
                        ListElement {
                            text: "Stereo"
                            val: StreamingPreferences.AC_STEREO
                        }
                        ListElement {
                            text: "5.1 surround son"
                            val: StreamingPreferences.AC_51_SURROUND
                        }
                        ListElement {
                            text: "7.1 surround son"
                            val: StreamingPreferences.AC_71_SURROUND
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        StreamingPreferences.audioConfig = audioListModel.get(currentIndex).val
                    }
                }

            }
        }

        GroupBox {
            id: uiSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<font color=\"skyblue\">Paramètres de l'interface utilisateur</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: startMaximizedCheck
                    width: parent.width
                    text: "Maximiser la fenêtre Moonlight au démarrage"
                    font.pointSize: 12
                    enabled: SystemProperties.hasWindowManager
                    checked: !StreamingPreferences.startWindowed || !SystemProperties.hasWindowManager
                    onCheckedChanged: {
                        StreamingPreferences.startWindowed = !checked
                    }
                }

                CheckBox {
                    id: connectionWarningsCheck
                    width: parent.width
                    text: "Afficher les avertissements sur la qualité de la connexion"
                    font.pointSize: 12
                    checked: StreamingPreferences.connectionWarnings
                    onCheckedChanged: {
                        StreamingPreferences.connectionWarnings = checked
                    }
                }
                CheckBox {
                    visible: SystemProperties.hasDiscordIntegration
                    id: discordPresenceCheck
                    width: parent.width
                    text: "Intégration de la présence de discorde"
                    font.pointSize: 12
                    checked: StreamingPreferences.richPresence
                    onCheckedChanged: {
                        StreamingPreferences.richPresence = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "Met à jour votre statut Discorde pour afficher le nom du jeu que vous jouez en streaming."
                }
            }
        }
    }

    Column {
        padding: 10
        rightPadding: 20
        anchors.left: settingsColumn1.right
        id: settingsColumn2
        width: settingsPage.width / 2
        spacing: 15

        GroupBox {
            id: gamepadSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<font color=\"skyblue\">Paramètres des entrées</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: singleControllerCheck
                    width: parent.width
                    text: "Forcé la manette de jeu #1 toujours présente"
                    font.pointSize:  12
                    checked: !StreamingPreferences.multiController
                    onCheckedChanged: {
                        StreamingPreferences.multiController = !checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "Oblige une seule manette de jeu à toujours rester connectée à l'hôte, même si aucune manette de jeu n'est réellement connectée à ce PC.\n" +
                                  "N'activez cette option que lorsque vous diffusez en continu un jeu qui ne prend pas en charge la connexion des manettes de jeu après le démarrage."
                }

                CheckBox {
                    id: absoluteMouseCheck
                    hoverEnabled: true
                    width: parent.width
                    text: "Optimiser la souris pour le bureau à distance plutôt que pour les jeux"
                    font.pointSize:  12
                    enabled: SystemProperties.hasWindowManager
                    checked: StreamingPreferences.absoluteMouseMode && SystemProperties.hasWindowManager
                    onCheckedChanged: {
                        StreamingPreferences.absoluteMouseMode = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "Cela permet de contrôler la souris sans avoir à capturer le curseur du client. Cela ne fonctionne pas dans la plupart des jeux.\n
                                   Vous pouvez le basculer pendant la diffusion en utilisant Ctrl+Alt+Maj+M."
                }

                CheckBox {
                    id: absoluteTouchCheck
                    hoverEnabled: true
                    width: parent.width
                    text: "Utiliser l'écran tactile comme trackpad"
                    font.pointSize:  12
                    checked: !StreamingPreferences.absoluteTouchMode
                    onCheckedChanged: {
                        StreamingPreferences.absoluteTouchMode = !checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "Lorsqu'on le vérifie, l'écran tactile agit comme un pavé tactile. Lorsqu'il n'est pas coché, l'écran tactile contrôle directement le pointeur de la souris."
                }

                CheckBox {
                    id: gamepadMouseCheck
                    hoverEnabled: true
                    width: parent.width
                    text: "Prise en charge du mode souris de la manette de jeu"
                    font.pointSize:  12
                    checked: StreamingPreferences.gamepadMouse
                    onCheckedChanged: {
                        StreamingPreferences.gamepadMouse = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 3000
                    ToolTip.visible: hovered
                    ToolTip.text: "Lorsqu'il est activé, le maintien du bouton Start permet de passer en mode souris"
                }

                CheckBox {
                    id: swapMouseButtonsCheck
                    hoverEnabled: true
                    width: parent.width
                    text: "Échanger les boutons de la souris"
                    font.pointSize:  12
                    checked: StreamingPreferences.swapMouseButtons
                    onCheckedChanged: {
                        StreamingPreferences.swapMouseButtons = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 3000
                    ToolTip.visible: hovered
                    ToolTip.text: "Lorsque la case est cochée, permuter les boutons gauche et droit de la souris"
                }
            }
        }

        GroupBox {
            id: hostSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<font color=\"skyblue\">Paramètres de l'hôte</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: optimizeGameSettingsCheck
                    width: parent.width
                    text: "Optimiser les paramètres du jeu pour la diffusion en continu"
                    font.pointSize:  12
                    checked: StreamingPreferences.gameOptimizations
                    onCheckedChanged: {
                        StreamingPreferences.gameOptimizations = checked
                    }
                }

                CheckBox {
                    id: audioPcCheck
                    width: parent.width
                    text: "Lecture audio sur le PC hôte"
                    font.pointSize:  12
                    checked: StreamingPreferences.playAudioOnHost
                    onCheckedChanged: {
                        StreamingPreferences.playAudioOnHost = checked
                    }
                }

                CheckBox {
                    id: quitAppAfter
                    width: parent.width
                    text: "Quitter l'application sur le PC hôte après avoir terminé le flux"
                    font.pointSize: 12
                    checked: StreamingPreferences.quitAppAfter
                    onCheckedChanged: {
                        StreamingPreferences.quitAppAfter = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "Cela fermera l'application ou le jeu que vous êtes en train de diffuser lorsque vous aurez terminé votre flux. Vous perdrez tout progrès non sauvegardé !"
                }
            }
        }

        GroupBox {
            id: advancedSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<font color=\"skyblue\">Paramètres avancés</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                Label {
                    width: parent.width
                    id: resVDSTitle
                    text: qsTr("Décodeur vidéo")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_vds = StreamingPreferences.videoDecoderSelection
                        currentIndex = 0
                        for (var i = 0; i < decoderListModel.count; i++) {
                            var el_vds = decoderListModel.get(i).val;
                            if (saved_vds === el_vds) {
                                currentIndex = i
                                break
                            }
                        }
                        activated(currentIndex)
                    }

                    id: decoderComboBox
                    textRole: "text"
                    model: ListModel {
                        id: decoderListModel
                        ListElement {
                            text: "Automatique (Recommandé)"
                            val: StreamingPreferences.VDS_AUTO
                        }
                        ListElement {
                            text: "Forcer le décodage logiciels"
                            val: StreamingPreferences.VDS_FORCE_SOFTWARE
                        }
                        ListElement {
                            text: "Forcer le décodage du matériel"
                            val: StreamingPreferences.VDS_FORCE_HARDWARE
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        StreamingPreferences.videoDecoderSelection = decoderListModel.get(currentIndex).val
                    }
                }

                Label {
                    width: parent.width
                    id: resVCCTitle
                    text: qsTr("Codec vidéo")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_vcc = StreamingPreferences.videoCodecConfig
                        currentIndex = 0
                        for(var i = 0; i < codecListModel.count; i++) {
                            var el_vcc = codecListModel.get(i).val;
                            if (saved_vcc === el_vcc) {
                                currentIndex = i
                                break
                            }
                        }
                        activated(currentIndex)
                    }

                    id: codecComboBox
                    textRole: "text"
                    model: ListModel {
                        id: codecListModel
                        ListElement {
                            text: "Automatique (Recommandé)"
                            val: StreamingPreferences.VCC_AUTO
                        }
                        ListElement {
                            text: "H.264"
                            val: StreamingPreferences.VCC_FORCE_H264
                        }
                        ListElement {
                            text: "HEVC (H.265)"
                            val: StreamingPreferences.VCC_FORCE_HEVC
                        }
                        /*ListElement {
                            text: "HEVC HDR (Experimental)"
                            val: StreamingPreferences.VCC_FORCE_HEVC_HDR
                        }*/
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        StreamingPreferences.videoCodecConfig = codecListModel.get(currentIndex).val
                    }
                }

                CheckBox {
                    id: unlockUnsupportedFps
                    width: parent.width
                    text: "Débloquer les options du FPS non prises en charge"
                    font.pointSize: 12
                    checked: StreamingPreferences.unsupportedFps
                    onCheckedChanged: {
                        // This is called on init, so only do the work if we've
                        // actually changed the value.
                        if (StreamingPreferences.unsupportedFps !== checked) {
                            StreamingPreferences.unsupportedFps = checked

                            // The selectable FPS values depend on whether
                            // this option is enabled or not
                            fpsComboBox.reinitialize()
                        }
                    }
                }

                CheckBox {
                    id: enableMdns
                    width: parent.width
                    text: "Trouver automatiquement les PC sur le réseau local (recommandé)"
                    font.pointSize: 12
                    checked: StreamingPreferences.enableMdns
                    onCheckedChanged: {
                        // This is called on init, so only do the work if we've
                        // actually changed the value.
                        if (StreamingPreferences.enableMdns !== checked) {
                            StreamingPreferences.enableMdns = checked

                            // We must save the updated preference to ensure
                            // ComputerManager can observe the change internally.
                            StreamingPreferences.save()

                            // Restart polling so the mDNS change takes effect
                            if (window.pollingActive) {
                                ComputerManager.stopPollingAsync()
                                ComputerManager.startPolling()
                            }
                        }
                    }
                }

                CheckBox {
                    id: detectNetworkBlocking
                    width: parent.width
                    text: "Détecter automatiquement les connexions bloquées (recommandé)"
                    font.pointSize: 12
                    checked: StreamingPreferences.detectNetworkBlocking
                    onCheckedChanged: {
                        // This is called on init, so only do the work if we've
                        // actually changed the value.
                        if (StreamingPreferences.detectNetworkBlocking !== checked) {
                            StreamingPreferences.detectNetworkBlocking = checked

                            // We must save the updated preference to ensure
                            // ComputerManager can observe the change internally.
                            StreamingPreferences.save()
                        }
                    }
                }
            }
        }
    }
}
