import QtQuick 2.0
import QtQuick.Controls 2.2

ToolButton {
    property string iconSource

    activeFocusOnTab: true

    // FIXME: We're using an Image here rather than icon.source because
    // icons don't work on Qt 5.9 LTS.
    Image {
        id: image
        source: iconSource
        anchors.centerIn: parent.background
        sourceSize {
            width: parent.background.width * 1.10
            height: parent.background.height * 1.10
        }
    }

    // This determines the size of the Material highlight. We increase it
    // from the default because we use larger than normal icons for TV readability.
    background.width: (parent.height - parent.anchors.bottomMargin - parent.anchors.topMargin) * 0.60
    background.height: (parent.height - parent.anchors.bottomMargin - parent.anchors.topMargin) * 0.60

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
