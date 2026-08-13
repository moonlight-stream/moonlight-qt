import QtQuick 2.0
import QtQuick.Controls

ItemDelegate {
    property GridView grid

    // 关掉 FluentWinUI3 那圈白色圆角双环 —— PcView / AppView 的格子项自己画
    // 焦点表达（月球高亮、卡片描边），再套一圈圆角白环只会糊在一起。
    // 详见 theme/FocusRing.qml 的注释。
    readonly property Item __focusFrameTarget: null

    highlighted: grid.activeFocus && grid.currentItem === this

    Keys.onLeftPressed: {
        grid.moveCurrentIndexLeft()
    }
    Keys.onRightPressed: {
        grid.moveCurrentIndexRight()
    }
    Keys.onDownPressed: {
        grid.moveCurrentIndexDown()
    }
    Keys.onUpPressed: {
        grid.moveCurrentIndexUp()

        // If we've reached the top of the grid, move focus to the toolbar
        if (grid.currentItem === this) {
            nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocusReason)
        }
    }
    Keys.onReturnPressed: {
        clicked()
    }
    Keys.onEnterPressed: {
        clicked()
    }
}
