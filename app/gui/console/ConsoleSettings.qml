import QtQuick

import StreamingPreferences 1.0

// Écran Paramètres style console (bouton Y), 100 % navigable manette :
// haut/bas pour choisir un réglage, gauche/droite pour modifier la valeur
// (appliquée et sauvegardée immédiatement), B (Échap) pour fermer.
// Réglages exposés : résolution, fréquence, débit + action "Oublier ce PC".
FocusScope {
    id: root

    property string hostName: ""

    signal forgetRequested()
    signal closed()

    visible: false

    property int currentRow: 0
    readonly property int rowCount: 4

    // --- Choix possibles ---
    readonly property var resolutions: [
        { label: "1280 × 720",  w: 1280, h: 720 },
        { label: "1920 × 1080", w: 1920, h: 1080 },
        { label: "2560 × 1440", w: 2560, h: 1440 }
    ]
    readonly property var fpsChoices: [30, 60, 120]

    property int resIndex: 1
    property int fpsIndex: 1
    property int bitrateMbps: 30

    function open() {
        // Recale les sélecteurs sur les préférences actuelles
        resIndex = 1
        for (var i = 0; i < resolutions.length; i++) {
            if (resolutions[i].w === StreamingPreferences.width
                    && resolutions[i].h === StreamingPreferences.height) {
                resIndex = i
                break
            }
        }
        var f = fpsChoices.indexOf(StreamingPreferences.fps)
        fpsIndex = f >= 0 ? f : 1
        bitrateMbps = Math.max(5, Math.round(StreamingPreferences.bitrateKbps / 1000))
        currentRow = 0
        visible = true
        forceActiveFocus()
    }

    function close() {
        visible = false
        closed()
    }

    function apply() {
        StreamingPreferences.width = resolutions[resIndex].w
        StreamingPreferences.height = resolutions[resIndex].h
        StreamingPreferences.fps = fpsChoices[fpsIndex]
        StreamingPreferences.bitrateKbps = bitrateMbps * 1000
        StreamingPreferences.save()
    }

    function adjust(dir) {
        if (currentRow === 0)
            resIndex = Math.max(0, Math.min(resolutions.length - 1, resIndex + dir))
        else if (currentRow === 1)
            fpsIndex = Math.max(0, Math.min(fpsChoices.length - 1, fpsIndex + dir))
        else if (currentRow === 2)
            bitrateMbps = Math.max(5, Math.min(80, bitrateMbps + dir * 5))
        else
            return
        apply()
    }

    Keys.onEscapePressed: close()
    Keys.onUpPressed: currentRow = Math.max(0, currentRow - 1)
    Keys.onDownPressed: currentRow = Math.min(rowCount - 1, currentRow + 1)
    Keys.onLeftPressed: adjust(-1)
    Keys.onRightPressed: adjust(1)
    Keys.onReturnPressed: currentRow === 3 ? forgetRequested() : adjust(1)
    Keys.onEnterPressed: currentRow === 3 ? forgetRequested() : adjust(1)

    // Voile sombre + blocage des clics vers l'arrière-plan
    Rectangle { anchors.fill: parent; color: "#d9000000" }
    MouseArea { anchors.fill: parent }

    Rectangle {
        anchors.centerIn: parent
        width: 620
        height: panelContent.height + 64
        radius: 16
        color: "#12161f"
        border.color: "#2e3542"
        border.width: 1

        Column {
            id: panelContent
            anchors.centerIn: parent
            width: parent.width - 64
            spacing: 18

            Text {
                text: qsTr("Paramètres")
                color: "#f4f6fa"
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }

            Text {
                text: qsTr("STREAM")
                color: "#6b7280"
                font.pixelSize: 11
                font.letterSpacing: 2
                font.weight: Font.DemiBold
            }

            Column {
                width: parent.width
                spacing: 8

                Repeater {
                    model: [
                        { name: qsTr("Résolution") },
                        { name: qsTr("Fréquence") },
                        { name: qsTr("Débit") },
                        { name: qsTr("Oublier ce PC") }
                    ]

                    delegate: Rectangle {
                        readonly property bool isFocused: root.currentRow === index
                        readonly property bool isAction: index === 3

                        width: parent.width
                        height: 54
                        radius: 12
                        color: isFocused ? "#1d2330" : "transparent"
                        border.color: isFocused ? "#F2802A" : "transparent"
                        border.width: isFocused ? 2 : 0

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 18
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.name
                            color: isAction ? "#e35b5b" : "#e8eaf0"
                            font.pixelSize: 15
                            font.weight: Font.Medium
                        }

                        Row {
                            anchors.right: parent.right
                            anchors.rightMargin: 18
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 12

                            Text {
                                text: "‹"
                                visible: isFocused && !isAction
                                color: "#8b909c"; font.pixelSize: 18
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: {
                                    if (index === 0) return root.resolutions[root.resIndex].label
                                    if (index === 1) return root.fpsChoices[root.fpsIndex] + qsTr(" ips")
                                    if (index === 2) return root.bitrateMbps + qsTr(" Mbps")
                                    return root.hostName
                                }
                                color: isAction ? "#8b909c" : "#F2802A"
                                font.pixelSize: 15
                                font.weight: Font.Medium
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "›"
                                visible: isFocused && !isAction
                                color: "#8b909c"; font.pixelSize: 18
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (root.currentRow !== index)
                                    root.currentRow = index
                                else if (isAction)
                                    root.forgetRequested()
                                else
                                    root.adjust(1)
                            }
                        }
                    }
                }
            }

            Text {
                text: qsTr("Les réglages sont appliqués immédiatement.")
                color: "#6b7280"
                font.pixelSize: 12
            }
        }
    }
}
