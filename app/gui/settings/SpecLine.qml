import QtQuick 2.9
import "../theme"

// 规格行：能力标签退化为等宽大写的一行丝印，青色斜杠分隔。
// 无容器无投影 —— 粗野感来自字距与大写，不来自装饰。
Flow {
    id: line

    property var tags: []

    spacing: Theme.spaceSm

    Repeater {
        model: line.tags

        Row {
            id: entry

            required property int index
            required property string modelData

            spacing: Theme.spaceSm

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: entry.modelData
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                font.capitalization: Font.AllUppercase
                font.letterSpacing: Theme.trackingCaption
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "/"
                visible: entry.index < line.tags.length - 1
                color: Theme.accentDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                font.letterSpacing: Theme.trackingCaption
            }
        }
    }
}
