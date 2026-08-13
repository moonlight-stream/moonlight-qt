import QtQuick 2.9
import QtQuick.Controls
import "."

// 方角勾选框。只覆盖 indicator，文字排布/换行继续走基类，
// 免得设置页里那些长说明文字的换行行为被改坏。
CheckBox {
    id: control

    font.family: Theme.fontSans

    // 关掉 FluentWinUI3 那圈白色圆角双环，焦点改用下面的方角 FocusRing。
    readonly property Item __focusFrameTarget: null

    // 自管指针路径要走的切换。不用 AbstractButton.click()：那个方法是 Qt 6.8 才有的
    // （QtQuick.Templates 的 qmltypes 里 click 标着 revision 1544 = 6×256+8），而
    // README 里我们还留着「Qt 5.12 或更新版本仍保留兼容」，Steam Link 那条工具链的
    // Qt 更早，调用它会直接是 TypeError。HardSwitch 早就绕开了，这里一直漏着。
    //
    // toggled() 只在真的换了状态时发，clicked() 照发 —— 让这条自管路径和基类点
    // 标签文字时的那条路径行为一致（目前 25 个使用点全都只接 onCheckedChanged，
    // 但别给以后的调用方留坑）。
    function commitPointerToggle() {
        if (!control.enabled) {
            return
        }

        var previousChecked = control.checked
        control.toggle()
        if (control.checked !== previousChecked) {
            control.toggled()
        }
        control.clicked()
    }

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
                    : (control.hovered ? Theme.accent : Theme.lineStrong)
        opacity: control.enabled ? 1.0 : 0.45

        // 勾选态本身就是 accent 填充 + accent 描边，这圈边框腾不出来表达焦点，
        // 所以焦点走外挂环。只有键盘/手柄带来的焦点才画，鼠标点一下不该冒出个框。
        FocusRing {
            visible: control.visualFocus
        }

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
            onClicked: control.commitPointerToggle()
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
