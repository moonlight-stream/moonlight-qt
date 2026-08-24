import QtQuick 2.9
import QtQuick.Controls
import "../theme"

// 客户端生态中的平台选择项。选中态表示当前详情，焦点态只表示键盘/手柄位置。
Button {
    id: control

    property bool selected: false

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    leftPadding: Theme.spaceLg
    rightPadding: Theme.spaceLg

    readonly property Item __focusFrameTarget: null

    contentItem: Text {
        text: control.text
        color: control.selected ? Theme.accentStrong : Theme.text
        font.family: Theme.fontSans
        font.pointSize: Theme.fontBody
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 164
        implicitHeight: 44
        radius: 0
        color: control.down || control.selected
               ? Theme.accentSoft
               : (control.hovered || control.visualFocus ? Theme.surface2 : "transparent")
        border.width: control.visualFocus ? 2 : 1
        border.color: control.selected || control.visualFocus || control.hovered
                      ? Theme.accent : Theme.line

        Rectangle {
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            width: control.selected ? Theme.accentBar : 0
            visible: width > 0
            color: Theme.accent
        }
    }

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
}
