import QtQuick 2.9
import QtQuick.Controls
import QtQuick.Layouts 1.3
import "."
import "../theme"

// 一行设置：左边标题 + 说明，右边控件。
// 用 FocusScope 是为了让 activeFocus 在内部控件获得焦点时为真，
// 这样手柄/键盘 Tab 过来时整行会亮起来，而不是只有控件本身有个小框。
FocusScope {
    id: row

    property string title: ""
    property string description: ""
    // 业务上是否该显示这一行（例如某功能在当前平台不可用）
    property bool applicable: true

    default property alias controlContent: controlSlot.data

    width: parent ? parent.width : 0
    visible: applicable
    height: visible ? implicitHeight : 0
    implicitHeight: Math.max(textColumn.implicitHeight, controlSlot.implicitHeight) + Theme.spaceMd * 2

    // 行背景：方角，hover 时填 surface2，focus 时描 1px accent。
    // 底部那条 1px 细线是行与行之间的分隔（参考站靠这种细线分栏，不靠间距），
    // 最后一行不画，避免和卡片下沿贴出双线。
    Rectangle {
        anchors.fill: parent
        radius: 0
        color: hoverArea.containsMouse ? Theme.surface2 : "transparent"
        border.width: 1
        border.color: row.activeFocus ? Theme.accent : "transparent"

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 1
            color: Theme.line
            // 同一个 Column 里最后一行不画分隔线
            visible: row.parent && row.parent.children
                     && row.parent.children[row.parent.children.length - 1] !== row
        }

        Behavior on color {
            ColorAnimation { duration: Theme.durFast }
        }
        Behavior on border.color {
            ColorAnimation { duration: Theme.durFast }
        }
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    RowLayout {
        anchors {
            fill: parent
            leftMargin: Theme.spaceMd
            rightMargin: Theme.spaceMd
            topMargin: Theme.spaceMd
            bottomMargin: Theme.spaceMd
        }
        spacing: Theme.spaceLg

        Column {
            id: textColumn
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.spaceXs

            Text {
                width: parent.width
                text: row.title
                visible: text !== ""
                color: Theme.text
                font.family: Theme.fontSans
                font.pointSize: Theme.fontRowTitle
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: row.description
                visible: text !== ""
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
            }
        }

        Item {
            id: controlSlot
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            Layout.preferredWidth: childrenRect.width
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
