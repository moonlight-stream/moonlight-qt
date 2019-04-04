import QtQuick 2.0
import QtQuick.Controls 2.2

ToolButton {
    activeFocusOnTab: true

    Keys.onReturnPressed: {
        clicked()
    }

    Keys.onDownPressed: {
        nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocus)
    }

    Keys.onUpPressed: {
        nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
    }
}
