import QtQuick 2.9
import QtQuick.Controls 2.2

GridView {
    // Detect Qt 5.11 or earlier using presence of synchronousDrag.
    // Prior to 5.12, the leftMargin and rightMargin values did not work.
    property bool hasBrokenMargins: this.synchronousDrag === undefined

    property int minMargin: 10
    property real availableWidth
    property int itemsPerRow 
    property real horizontalMargin

    function updateMargins() {
        leftMargin = horizontalMargin
        rightMargin = horizontalMargin

        if (hasBrokenMargins) {
            anchors.leftMargin = leftMargin
            anchors.rightMargin = rightMargin
        }
    }

    onHorizontalMarginChanged: {
        updateMargins()
    }

    Component.onCompleted: {
        availableWidth = (parent.width - 2 * minMargin)
        itemsPerRow = availableWidth / cellWidth
        horizontalMargin = itemsPerRow < count && availableWidth >= cellWidth ?
                                        (availableWidth % cellWidth) / 2 : minMargin

        if (hasBrokenMargins) {
            // This will cause an anchor conflict with the parent StackView
            // which breaks animation, but otherwise the grid will not be
            // centered in the window.
            anchors.fill = parent
            anchors.topMargin = topMargin
            anchors.bottomMargin = bottomMargin
        }

        updateMargins()
    }

    boundsBehavior: Flickable.OvershootBounds
}
