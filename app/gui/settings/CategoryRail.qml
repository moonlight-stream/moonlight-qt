import QtQuick 2.9
import QtQuick.Controls
import "."
import "../theme"

// 左侧分类导航。窄窗口时 compact 置真，变成横向 tab 条。
Item {
    id: rail

    property var categories: []
    property string currentCategory: ""
    property bool compact: false

    signal categoryPicked(string category)

    function step(delta) {
        var index = indexOf(currentCategory)
        if (index < 0) {
            return
        }
        var next = index + delta
        if (next < 0) {
            next = categories.length - 1
        }
        else if (next >= categories.length) {
            next = 0
        }
        categoryPicked(categories[next].key)
    }

    function indexOf(key) {
        for (var i = 0; i < categories.length; i++) {
            if (categories[i].key === key) {
                return i
            }
        }
        return -1
    }

    implicitHeight: compact ? 46 : 0

    ListView {
        id: list
        anchors.fill: parent
        orientation: rail.compact ? ListView.Horizontal : ListView.Vertical
        spacing: Theme.spaceXs
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: rail.categories

        delegate: ItemDelegate {
            id: item

            readonly property bool current: modelData.key === rail.currentCategory

            width: rail.compact ? Math.max(96, label.implicitWidth + Theme.spaceXl + Theme.spaceLg) : list.width
            height: rail.compact ? list.height : 44

            onClicked: rail.categoryPicked(modelData.key)

            // 方角 + 当前项左侧一条 accent 粗条（横向 tab 条时改成底部一条）。
            // 原来靠一圈描边表示当前项，方角化之后描边和卡片边框太像，粗条读起来更明确。
            background: Rectangle {
                radius: 0
                color: item.current ? Theme.accentSoft
                                    : (item.hovered ? Theme.surface2 : "transparent")

                Rectangle {
                    anchors {
                        left: parent.left
                        top: parent.top
                        bottom: parent.bottom
                    }
                    width: (item.current && !rail.compact) ? Theme.accentBar : 0
                    visible: width > 0
                    color: Theme.accent
                }

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                    }
                    height: (item.current && rail.compact) ? Theme.accentBar : 0
                    visible: height > 0
                    color: Theme.accent
                }

                Behavior on color {
                    ColorAnimation { duration: Theme.durFast }
                }
            }

            contentItem: Row {
                id: label
                spacing: Theme.spaceSm
                leftPadding: Theme.spaceMd

                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    source: modelData.icon
                    // 按 2x 栅格化，Retina 上才不会糊
                    sourceSize.width: 18
                    sourceSize.height: 18
                    width: 18
                    height: 18
                    // 图标本身是白的，靠透明度区分选中与否
                    opacity: item.current ? 1.0 : 0.6

                    Behavior on opacity {
                        NumberAnimation { duration: Theme.durFast }
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.title
                    color: item.current ? Theme.text : Theme.textDim
                    font.family: Theme.fontSans
                    font.pointSize: Theme.fontRowTitle
                    font.weight: item.current ? Font.ExtraBold : Font.Medium
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: Theme.tracking(Theme.fontRowTitle, 0.06)
                    elide: Text.ElideRight
                }
            }
        }
    }
}
