import QtQuick 2.0
import QtQuick.Controls

import "theme"

// 全应用的对话框外壳：方角 Panel + 硬投影 + 左侧强调粗条，标题走宽字距大写。
// 内容和按钮由各个调用点自己填，这里只管壳。
Dialog {
    id: control

    modal: true
    anchors.centerIn: Overlay.overlay

    topPadding: Theme.spaceXl
    bottomPadding: Theme.spaceXl
    // 左边多让出粗条的宽度，否则内容会压在粗条上
    leftPadding: Theme.spaceXl + Theme.accentBar
    rightPadding: Theme.spaceXl

    background: Panel {
        fill: Theme.surfaceLayer
        accentBarColor: Theme.accent
        accentBarWidth: Theme.accentBar
    }

    // 遮罩用 ink 而不是默认的半透明黑，和各页的壁纸遮罩同一个底色
    Overlay.modal: Rectangle {
        color: Qt.rgba(Theme.ink.r, Theme.ink.g, Theme.ink.b, 0.66)
    }

    header: Item {
        // 没设 title 的对话框（绝大多数消息框）不占高度
        visible: control.title !== ""
        implicitHeight: visible ? titleText.implicitHeight + Theme.spaceXl + Theme.spaceMd : 0

        Text {
            id: titleText

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                leftMargin: control.leftPadding
                rightMargin: control.rightPadding
                topMargin: Theme.spaceLg
            }
            text: control.title
            color: Theme.text
            font.family: Theme.fontSans
            font.pointSize: Theme.fontCardTitle
            font.weight: Font.ExtraBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: Theme.tracking(Theme.fontCardTitle, 0.08)
            elide: Text.ElideRight
        }

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                leftMargin: control.leftPadding
                rightMargin: control.rightPadding
            }
            height: 1
            color: Theme.line
            visible: parent.visible
        }
    }

    // 按钮区。不覆盖的话 FluentWinUI3 会自己垫一块圆角底板（那块比 Panel 亮一档的
    // 深蓝），按钮也还是圆角胶囊 —— 添加主机那个框看起来「很奇怪」就是这个原因。
    // NavigableMessageDialog 有自己的 footer（它要 helpRequested），会覆盖这一份。
    footer: DialogButtonBox {
        visible: count > 0
        standardButtons: control.standardButtons

        padding: Theme.spaceXl
        topPadding: 0
        spacing: Theme.spaceSm
        alignment: Qt.AlignRight

        background: Item {}

        delegate: HardButton {
            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
            Keys.onRightPressed: nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocus)
            Keys.onLeftPressed: nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
        }
    }

    onClosed: {
        // We must force focus back to the last item. If we don't,
        // gamepad and keyboard navigation will break after a
        // dialog appears.
        stackView.forceActiveFocus()
    }
}
