import QtQuick 2.9
import QtQuick.Controls 2.2

import AppModel 1.0
import ComputerManager 1.0

GridView {
    property int computerIndex
    property AppModel appModel : createModel()
    property bool activated

    id: appGrid
    focus: true
    activeFocusOnTab: true
    topMargin: 20
    bottomMargin: 5
    leftMargin: contentHeight > cellHeight && parent.width > cellWidth ? (parent.width % cellWidth) / 2 : 10
    rightMargin: leftMargin
    cellWidth: 230; cellHeight: 297;

    function computerLost()
    {
        // Go back to the PC view on PC loss
        stackView.pop()
    }

    Component.onCompleted: {
        // Don't show any highlighted item until interacting with them.
        // We do this here instead of onActivated to avoid losing the user's
        // selection when backing out of a different page of the app.
        currentIndex = -1

        // HACK: If this is not Qt 5.12 (which has synchronousDrag),
        // set anchors on creation. This will cause an anchor conflict
        // with the parent StackView which breaks animation, but otherwise
        // the grid will not be centered in the window.
        if (this.synchronousDrag === undefined) {
            anchors.fill = parent
            anchors.leftMargin = leftMargin
            anchors.rightMargin = rightMargin
            anchors.topMargin = topMargin
            anchors.bottomMargin = bottomMargin
        }
    }

    StackView.onActivated: {
        appModel.computerLost.connect(computerLost)
        activated = true
    }

    StackView.onDeactivating: {
        appModel.computerLost.disconnect(computerLost)
        activated = false
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import AppModel 1.0; AppModel {}', parent, '')
        model.initialize(ComputerManager, computerIndex)
        return model
    }

    model: appModel

    delegate: NavigableItemDelegate {
        width: 220; height: 287;
        grid: appGrid

        Image {
            property bool isPlaceholder: false

            id: appIcon
            anchors.horizontalCenter: parent.horizontalCenter
            y: 10
            source: model.boxart

            onSourceSizeChanged: {
                if ((width == 130 && height == 180) || // GFE 2.0 placeholder image
                    (width == 628 && height == 888) || // GFE 3.0 placeholder image
                    (width == 200 && height == 266))   // Our no_app_image.png
                {
                    isPlaceholder = true
                }
                else
                {
                    isPlaceholder = false
                }

                width = 200
                height = 267
            }

            // Display a tooltip with the full name if it's truncated
            ToolTip.text: model.name
            ToolTip.delay: 1000
            ToolTip.timeout: 5000
            ToolTip.visible: (parent.hovered || parent.highlighted) && (!appNameText.visible || appNameText.truncated)
        }

        ToolButton {
            id: resumeButton
            anchors.horizontalCenterOffset: appIcon.isPlaceholder ? -47 : 0
            anchors.verticalCenterOffset: appIcon.isPlaceholder ? -75 : -60
            anchors.centerIn: appIcon
            visible: model.running
            implicitWidth: 125
            implicitHeight: 125

            Image {
                source: "qrc:/res/baseline-play_circle_filled_white-48px.svg"
                anchors.centerIn: parent
                sourceSize {
                    width: 75
                    height: 75
                }
            }

            onClicked: {
                launchOrResumeSelectedApp()
            }

            ToolTip.text: "Resume Game"
            ToolTip.delay: 1000
            ToolTip.timeout: 3000
            ToolTip.visible: hovered
        }

        ToolButton {
            id: quitButton
            anchors.horizontalCenterOffset: appIcon.isPlaceholder ? 47 : 0
            anchors.verticalCenterOffset: appIcon.isPlaceholder ? -75 : 60
            anchors.centerIn: appIcon
            visible: model.running
            implicitWidth: 125
            implicitHeight: 125

            Image {
                source: "qrc:/res/baseline-cancel-24px.svg"
                anchors.centerIn: parent
                sourceSize {
                    width: 75
                    height: 75
                }
            }

            onClicked: {
                doQuitGame()
            }

            ToolTip.text: "Quit Game"
            ToolTip.delay: 1000
            ToolTip.timeout: 3000
            ToolTip.visible: hovered
        }

        Label {
            id: appNameText
            visible: appIcon.isPlaceholder
            text: model.name
            width: appIcon.width
            height: model.running ? 150 : appIcon.height
            anchors.left: appIcon.left
            anchors.right: appIcon.right
            anchors.bottom: appIcon.bottom
            font.pointSize: 22
            verticalAlignment: model.running ? Text.AlignTop : Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }

        function launchOrResumeSelectedApp()
        {
            var runningIndex = appModel.getRunningAppIndex()
            if (runningIndex >= 0 && runningIndex !== index) {
                quitAppDialog.appName = appModel.getRunningAppName()
                quitAppDialog.segueToStream = true
                quitAppDialog.nextAppName = model.name
                quitAppDialog.nextAppIndex = index
                quitAppDialog.open()
                return
            }

            var component = Qt.createComponent("StreamSegue.qml")
            var segue = component.createObject(stackView, {"appName": model.name, "session": appModel.createSessionForApp(index)})
            stackView.push(segue)
        }

        onClicked: {
            if (model.running) {
                // This will primarily be keyboard/gamepad driven so use
                // open() instead of popup()
                appContextMenu.open()
            }
            else {
                launchOrResumeSelectedApp()
            }
        }

        Keys.onMenuPressed: {
            if (model.running) {
                // This will primarily be keyboard/gamepad driven so use
                // open() instead of popup()
                appContextMenu.open()
            }
        }

        function doQuitGame() {
            quitAppDialog.appName = appModel.getRunningAppName()
            quitAppDialog.segueToStream = false
            quitAppDialog.open()
        }

        NavigableMenu {
            id: appContextMenu
            NavigableMenuItem {
                parentMenu: appContextMenu
                text: model.running ? "Resume Game" : "Launch Game"
                onTriggered: launchOrResumeSelectedApp()
            }
            NavigableMenuItem {
                parentMenu: appContextMenu
                text: "Quit Game"
                onTriggered: doQuitGame()
                visible: model.running
            }
        }
    }

    NavigableMessageDialog {
        id: quitAppDialog
        property string appName : ""
        property bool segueToStream : false
        property string nextAppName: ""
        property int nextAppIndex: 0
        text:"Are you sure you want to quit " + appName +"? Any unsaved progress will be lost."
        standardButtons: Dialog.Yes | Dialog.No

        function quitApp() {
            var component = Qt.createComponent("QuitSegue.qml")
            var params = {"appName": appName}
            if (segueToStream) {
                // Store the session and app name if we're going to stream after
                // successfully quitting the old app.
                params.nextAppName = nextAppName
                params.nextSession = appModel.createSessionForApp(nextAppIndex)
            }
            else {
                params.nextAppName = null
                params.nextSession = null
            }

            stackView.push(component.createObject(stackView, params))

            // Trigger the quit after pushing the quit segue on screen
            appModel.quitRunningApp()
        }

        onAccepted: quitApp()
    }

    ScrollBar.vertical: ScrollBar {
        // Manually hide the scrollbar to prevent it from being drawn on top
        // of the StreamSegue during the transition. It can sometimes get stuck
        // there since we're not consistently pumping the event loop while
        // starting the stream.
        visible: activated

        parent: appGrid.parent
        anchors {
            top: parent.top
            right: parent.right
            bottom: parent.bottom
        }
    }
}
