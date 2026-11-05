import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

ToolButton {
    property string iconSource

    activeFocusOnTab: true

    icon.source: iconSource
    // Size the icon from the button's height rather than its background.
    // Material's background spans the full button width when the content
    // isn't square, so binding the icon to it feeds the button's width back
    // into its own implicit width and the toolbar layout grows without bound.
    icon.width: height / 2
    icon.height: height / 2

    // This determines the size of the Material highlight. We increase it
    // from the default because we use larger than normal icons for TV readability.
    Layout.preferredHeight: parent.height

    Keys.onReturnPressed: {
        clicked()
    }

    Keys.onEnterPressed: {
        clicked()
    }

    Keys.onRightPressed: {
        nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocus)
    }

    Keys.onLeftPressed: {
        nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
    }
}
