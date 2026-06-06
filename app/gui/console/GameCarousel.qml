import QtQuick

// Carrousel horizontal de jaquettes 16:9.
// La jaquette sélectionnée est centrée, agrandie et cerclée d'orange ;
// les voisines débordent et s'estompent. Navigation gauche/droite native.
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

    implicitHeight: focusedH + 30

    ListView {
        id: list
        anchors.fill: parent
        orientation: ListView.Horizontal
        focus: true
        keyNavigationEnabled: true
        spacing: 28
        clip: true

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

            Behavior on width {
                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
            }

            Rectangle {
                id: art
                anchors.centerIn: parent
                width: parent.width
                height: del.ListView.isCurrentItem ? root.focusedH : root.normalH
                radius: 14
                color: model.art
                opacity: del.ListView.isCurrentItem ? 1.0 : 0.5
                border.width: del.ListView.isCurrentItem ? 3 : 0
                border.color: "#F2802A"

                Behavior on height {
                    NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                }
                Behavior on opacity {
                    NumberAnimation { duration: 200 }
                }

                Column {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: del.ListView.isCurrentItem ? 18 : 14
                    spacing: 3
                    Text {
                        text: model.genre
                        color: "#fac775"
                        font.pixelSize: del.ListView.isCurrentItem ? 14 : 12
                    }
                    Text {
                        text: model.title
                        color: "white"
                        font.weight: Font.Medium
                        font.pixelSize: del.ListView.isCurrentItem ? 22 : 15
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (del.ListView.isCurrentItem)
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
