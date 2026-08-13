import QtQuick 2.0
import QtQuick.Controls
import QtQuick.Layouts 1.3

import "theme"

ToolButton {
    id: control

    property string iconSource

    // 工具栏图标现在是 Fluent 24px Regular 的线性字形（和设置页分类栏同一套），
    // 不再是以前那批 Material 48px filled。线性笔画在 40px 下会显得又细又飘，
    // 所以按它的设计尺寸走 24 —— 顶部 bar 只有 56 高，40px 的图标本来也太满了。
    property int iconSize: 24

    activeFocusOnTab: true

    // 关掉 FluentWinUI3 那圈白色圆角双环，焦点由下面的 2px accent 边框表达。
    // 详见 theme/FocusRing.qml 的注释。
    readonly property Item __focusFrameTarget: null

    icon.source: iconSource
    icon.width: iconSize
    icon.height: iconSize

    // 统一染色：图标自己的 fill 是白的，这里压到 textDim，hover / 焦点时才提亮到
    // text。这样一排图标的视觉重量是一致的，不会因为各自笔画粗细不同而深浅不齐。
    icon.color: (control.hovered || control.visualFocus || control.down)
                ? Theme.text : Theme.textDim

    // 方角化。FluentWinUI3 的 ToolButton 背景是圆角高亮块，换成方角 + 描边；
    // 按下时反过来用 accentSoft 填充，不做缩放也不做模糊。
    // 静止无描边，hover 1px accent，focus 2px accent —— 和其他控件一个规矩。
    background: Rectangle {
        radius: 0
        color: control.down ? Theme.accentSoft
                            : (control.hovered ? Theme.surface2 : "transparent")
        border.width: control.visualFocus ? 2 : (control.hovered ? 1 : 0)
        border.color: Theme.accent

        Behavior on color {
            ColorAnimation { duration: Theme.durFast }
        }
    }

    // This determines the size of the focus/hover highlight. We increase it
    // from the default because we use larger than normal icons for TV readability.
    Layout.preferredHeight: parent.height

    Keys.onReturnPressed: {
        clicked()
    }

    Keys.onEnterPressed: {
        clicked()
    }

    Keys.onRightPressed: {
        nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocusReason)
    }

    Keys.onLeftPressed: {
        nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocusReason)
    }
}
