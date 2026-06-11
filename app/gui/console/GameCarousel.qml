import QtQuick
import QtQuick.Effects

// Carrousel horizontal de jaquettes 16:9.
// La jaquette sélectionnée est centrée, agrandie, cerclée d'orange avec un
// halo lumineux et une ombre portée ; les voisines débordent et s'estompent.
// Navigation gauche/droite native (clavier + manette via SdlGamepadKeyNavigation).
// Consomme un modèle exposant les rôles `name`, `boxart` (url) et `running` (bool).
FocusScope {
    id: root

    property alias model: list.model
    property alias currentIndex: list.currentIndex
    signal launchRequested(int index)

    // Tailles des jaquettes (ratio 16:9)
    property int focusedW: 460
    property int focusedH: 259
    property int normalW: 360
    property int normalH: 203

    implicitHeight: focusedH + 46

    ListView {
        id: list
        anchors.fill: parent
        orientation: ListView.Horizontal
        focus: true
        keyNavigationEnabled: true
        spacing: 28
        clip: true
        cacheBuffer: root.focusedW * 2

        // Garde l'élément courant centré
        highlightRangeMode: ListView.StrictlyEnforceRange
        preferredHighlightBegin: width / 2 - root.focusedW / 2
        preferredHighlightEnd: width / 2 + root.focusedW / 2
        highlightMoveDuration: 200
        highlight: Item {}

        delegate: Item {
            id: del
            width: del.ListView.isCurrentItem ? root.focusedW : root.normalW
            height: list.height

            readonly property bool isCurrent: del.ListView.isCurrentItem
            readonly property real coverH: isCurrent ? root.focusedH : root.normalH

            Behavior on width {
                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
            }

            // Ombre portée douce sous la jaquette
            Canvas {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: del.coverH / 2 + 8
                width: parent.width * 0.85
                height: 30
                opacity: del.isCurrent ? 0.8 : 0.35
                Behavior on opacity { NumberAnimation { duration: 200 } }
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    var g = ctx.createRadialGradient(width / 2, height / 2, 0,
                                                     width / 2, height / 2, width / 2)
                    g.addColorStop(0, "rgba(0, 0, 0, 0.55)")
                    g.addColorStop(1, "rgba(0, 0, 0, 0)")
                    ctx.fillStyle = g
                    ctx.save()
                    ctx.scale(1, height / width)
                    ctx.fillRect(0, 0, width, width)
                    ctx.restore()
                }
            }

            // Halo orange diffus autour de la jaquette active
            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 12
                height: del.coverH + 12
                radius: 20
                color: "transparent"
                border.color: "#30F2802A"
                border.width: 5
                visible: del.isCurrent
            }
            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 6
                height: del.coverH + 6
                radius: 17
                color: "transparent"
                border.color: "#60F2802A"
                border.width: 3
                visible: del.isCurrent
            }

            Rectangle {
                id: frame
                anchors.centerIn: parent
                width: parent.width
                height: del.coverH
                radius: 14
                color: "#1b1e26"  // remplit derrière la jaquette si elle ne charge pas
                opacity: del.isCurrent ? 1.0 : 0.5
                border.width: del.isCurrent ? 3 : 1
                border.color: del.isCurrent ? "#F2802A" : "#2a2f3a"

                Behavior on height {
                    NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                }
                Behavior on opacity {
                    NumberAnimation { duration: 200 }
                }

                // Contenu de la jaquette, masqué par un rectangle arrondi pour
                // que l'image suive réellement les coins du cadre.
                Item {
                    id: coverContent
                    anchors.fill: parent
                    anchors.margins: 1
                    visible: false

                    Image {
                        anchors.fill: parent
                        source: model.boxart
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        smooth: true
                    }

                    // Dégradé sombre en bas pour la lisibilité du texte
                    Rectangle {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                        height: 80
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#00000000" }
                            GradientStop { position: 1.0; color: "#d8000000" }
                        }
                    }
                }

                Rectangle {
                    id: coverMask
                    anchors.fill: coverContent
                    radius: 13
                    visible: false
                    layer.enabled: true
                    layer.smooth: true
                }

                MultiEffect {
                    anchors.fill: coverContent
                    source: coverContent
                    maskEnabled: true
                    maskSource: coverMask
                }

                // Badge "EN COURS" (jeu actuellement diffusé)
                Rectangle {
                    anchors { top: parent.top; left: parent.left; margins: 12 }
                    visible: model.running
                    width: runningRow.width + 20
                    height: 24
                    radius: 12
                    color: "#cc10151d"
                    border.color: "#2e3542"
                    border.width: 1

                    Row {
                        id: runningRow
                        anchors.centerIn: parent
                        spacing: 6
                        Rectangle {
                            width: 7; height: 7; radius: 3.5
                            color: "#34d399"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: qsTr("EN COURS")
                            color: "#cdd2da"
                            font.pixelSize: 10
                            font.letterSpacing: 1
                            font.weight: Font.DemiBold
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                // Titre du jeu, par-dessus le dégradé
                Text {
                    anchors {
                        left: parent.left; right: parent.right; bottom: parent.bottom
                        margins: del.isCurrent ? 18 : 14
                    }
                    text: model.name
                    color: "white"
                    font.weight: Font.Medium
                    font.pixelSize: del.isCurrent ? 22 : 15
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (del.isCurrent)
                        root.launchRequested(index)
                    else
                        list.currentIndex = index
                }
            }
        }

        Keys.onReturnPressed: root.launchRequested(list.currentIndex)
        Keys.onEnterPressed: root.launchRequested(list.currentIndex)
    }
}
