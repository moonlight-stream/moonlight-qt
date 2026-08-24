import QtQuick 2.9
import QtQuick.Controls
import "."

// 中括号跳转链：等宽 + accent + [ ] 包裹，悬停提亮加下划线。导航入口
// 不占用按钮语言，「下载」等真正动作仍走 HardButton。高度与 HardButton
// 一致(34)，保证相邻混排时垂直对齐。
Button {
    id: control

    readonly property Item __focusFrameTarget: null

    implicitHeight: 34
    padding: Theme.spaceXs
    spacing: 0

    opacity: control.enabled ? 1.0 : 0.45

    contentItem: Text {
        text: "[ " + control.text + " ]"
        color: control.hovered || control.visualFocus ? Theme.accentStrong : Theme.accent
        font.family: Theme.fontMono
        font.pointSize: Theme.fontBody
        font.underline: control.hovered || control.visualFocus
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 0
        color: "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }

    // 只改光标不接事件，点击仍由 Button 处理
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        cursorShape: Qt.PointingHandCursor
    }
}
