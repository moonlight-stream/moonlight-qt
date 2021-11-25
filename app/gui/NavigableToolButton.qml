import QtQuick 2.0
import QtQuick.Controls 2.2

// We use a RoundButton instead of a ToolButton because ToolButton
// doesn't seem to like to be sized larger than normal on Qt 6.2.
RoundButton {
    property string iconSource

    // Style like a ToolButton
    leftInset: 0
    rightInset: 0
    flat: true

    activeFocusOnTab: true

    // FIXME: We're using an Image here rather than icon.source because
    // icons don't work on Qt 5.9 LTS.
    Image {
        id: image
        source: iconSource
        anchors.centerIn: parent.background
        sourceSize {
            width: parent.background.width * 0.80
            height: parent.background.height * 0.80
        }
    }

    // This determines the size of the Material highlight. We increase it
    // from the default because we use larger than normal icons for TV readability.
    background.implicitWidth: (parent.height - parent.anchors.bottomMargin - parent.anchors.topMargin) * 0.80
    background.implicitHeight: (parent.height - parent.anchors.bottomMargin - parent.anchors.topMargin) * 0.80

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
