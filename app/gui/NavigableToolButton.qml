import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

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
