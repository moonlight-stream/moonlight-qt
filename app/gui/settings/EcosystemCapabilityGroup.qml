pragma ComponentBehavior: Bound
import QtQuick 2.9
import "."
import "../theme"

// “关于”页中的只读能力分组。只负责排版，不探测硬件，也不代表当前设备一定可用。
Column {
    id: group

    property string title: ""
    property string description: ""
    property var tags: []
    property bool showDivider: true

    width: parent ? parent.width : 0
    spacing: Theme.spaceSm

    Text {
        width: parent.width
        text: group.title
        color: Theme.text
        font.family: Theme.fontSans
        font.pointSize: Theme.fontRowTitle
        font.weight: Font.Medium
        wrapMode: Text.Wrap
    }

    Text {
        width: parent.width
        text: group.description
        color: Theme.textDim
        font.family: Theme.fontMono
        font.pointSize: Theme.fontCaption
        wrapMode: Text.Wrap
    }

    SpecLine {
        width: parent.width
        tags: group.tags
    }

    Rectangle {
        width: parent.width
        height: 1
        visible: group.showDivider
        color: Theme.line
    }
}
