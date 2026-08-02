import QtQuick 2.9
import QtQuick.Controls
import "."

// 方角勾选框。只覆盖 indicator，文字排布/换行继续走基类，
// 免得设置页里那些长说明文字的换行行为被改坏。
CheckBox {
    id: control

    font.family: Theme.fontSans

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2

        radius: 0
        color: control.checked ? Theme.accent : Theme.surface2
        border.width: 1
        border.color: !control.enabled ? Theme.line
                    : control.checked ? Theme.accent
                    : (control.hovered || control.visualFocus ? Theme.accent : Theme.lineStrong)
        opacity: control.enabled ? 1.0 : 0.45

        // Own clicks on the painted box. FluentWinUI3 can ignore a stationary
        // release on a replaced indicator, which makes a normal click appear
        // to work only after several attempts. Keep the base CheckBox behavior
        // for its label, keyboard navigation, and accessibility.
        MouseArea {
            anchors.fill: parent
            anchors.margins: -6
            enabled: control.enabled
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.PointingHandCursor

            onPressed: control.forceActiveFocus(Qt.MouseFocusReason)
            onClicked: control.click()
        }

        Behavior on color {
            ColorAnimation { duration: Theme.durFast }
        }

        // 勾用两条旋转的实心线拼出来，不用字体里的 ✓：字体回退时那个字形的
        // 大小和基线都不可控，而 Canvas 在 visible 翻转时不保证重绘。
        Item {
            anchors.fill: parent
            opacity: control.checked ? 1 : 0

            Behavior on opacity {
                NumberAnimation { duration: Theme.durFast; easing.type: Theme.easing }
            }

            Rectangle {
                x: parent.width * 0.26
                y: parent.height * 0.52
                width: parent.width * 0.30
                height: 2
                radius: 0
                color: Theme.ink
                transformOrigin: Item.Left
                rotation: 45
            }

            Rectangle {
                x: parent.width * 0.35
                y: parent.height * 0.68
                width: parent.width * 0.52
                height: 2
                radius: 0
                color: Theme.ink
                transformOrigin: Item.Left
                rotation: -50
            }
        }
    }
}
