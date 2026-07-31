import QtQuick 2.9
import "."

// 全应用的主力容器：方角 + 1px 描边 + 零模糊硬偏移投影，可选左侧粗色条。
//
// QML 没有 box-shadow，所以投影就是一块偏移的实心矩形垫在本体后面。
// 「抬起」（hover / focus / 手柄高亮）不是缩放也不是加模糊，而是本体往左上挪
// 3px、投影同时变长 —— 这是 neo-brutalism 的标准交互反馈，读起来像一张实体卡片
// 被从纸面上拈起来。
Item {
    id: root

    property color fill: Theme.surface
    property color borderColor: Theme.line
    property int borderWidth: 1

    // 左侧粗条。宽度给 0 就不画。
    property color accentBarColor: Theme.accent
    property int accentBarWidth: 0

    // 抬起状态：投影变长 + 本体位移
    property bool lifted: false

    property real shadowDepth: lifted ? Theme.shadowOffsetLift : Theme.shadowOffset
    property real liftShift: lifted ? -3 : 0

    // 内容放在粗条右边的区域里，调用方不用自己躲开粗条
    default property alias content: contentArea.data

    Behavior on shadowDepth {
        NumberAnimation { duration: Theme.durFast; easing.type: Theme.easing }
    }
    Behavior on liftShift {
        NumberAnimation { duration: Theme.durFast; easing.type: Theme.easing }
    }

    // 硬投影：零模糊，纯实心偏移块
    Rectangle {
        x: body.x + root.shadowDepth
        y: body.y + root.shadowDepth
        width: body.width
        height: body.height
        color: Theme.shadowColor
    }

    Rectangle {
        id: body

        x: root.liftShift
        y: root.liftShift
        width: root.width
        height: root.height

        radius: 0
        color: root.fill
        border.width: root.borderWidth
        border.color: root.borderColor

        Behavior on color {
            ColorAnimation { duration: Theme.durNormal }
        }
        Behavior on border.color {
            ColorAnimation { duration: Theme.durFast }
        }

        Rectangle {
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            width: root.accentBarWidth
            visible: width > 0
            color: root.accentBarColor
        }

        Item {
            id: contentArea
            anchors {
                fill: parent
                leftMargin: root.accentBarWidth
            }
        }
    }
}
