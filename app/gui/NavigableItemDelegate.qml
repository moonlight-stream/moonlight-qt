import QtQuick 2.0
import QtQuick.Controls 2.2

ItemDelegate {
    property GridView grid
    property string accessibleName: ""
    property string accessibleDescription: ""

    highlighted: grid.activeFocus && grid.currentItem === this

    // Accessibility support for screen readers
    Accessible.role: Accessible.ListItem
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.onPressAction: clicked()

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
            nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
        }
    }
    Keys.onReturnPressed: {
        clicked()
    }
    Keys.onEnterPressed: {
        clicked()
    }
}
