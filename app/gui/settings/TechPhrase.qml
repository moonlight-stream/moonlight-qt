import QtQuick 2.9
import "."
import "../theme"

// 普通叙述与 HUD 芯片在同一个 Flow 中排版。长正文可以占满一行换行，
// 技术关键词保持独立芯片，窄窗口下自然移动到下一行。
Flow {
    id: phrase

    property var segments: []

    spacing: Theme.spaceXs

    Repeater {
        model: phrase.segments

        Item {
            id: segment

            required property int index
            required property var modelData

            readonly property bool highlighted: modelData.highlight === true

            implicitWidth: highlighted ? highlightedTag.implicitWidth :
                                         Math.min(plainText.implicitWidth, phrase.width)
            implicitHeight: highlighted ? 40 : Math.max(40, plainText.implicitHeight)

            TechTag {
                id: highlightedTag
                anchors.centerIn: parent
                visible: segment.highlighted
                text: segment.modelData.text || ""
                showIndicator: false
            }

            Text {
                id: plainText
                anchors.centerIn: parent
                visible: !segment.highlighted
                width: segment.width
                text: segment.modelData.text || ""
                color: Theme.textSettingsSubtitle
                font.family: Theme.fontSans
                font.pointSize: Theme.fontSettingsSubtitle + 1
                font.weight: Font.Medium
                wrapMode: Text.Wrap
            }
        }
    }
}
