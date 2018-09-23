import QtQuick 2.0
import QtQuick.Controls 2.2

ToolButton {
    activeFocusOnTab: true

    Keys.onReturnPressed: {
        clicked()
    }

    Keys.onRightPressed: {
        nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocus)
    }

    Keys.onLeftPressed: {
        nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
    }
}
