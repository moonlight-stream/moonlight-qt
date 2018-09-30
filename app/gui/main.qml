import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2

import QtQuick.Controls.Material 2.1

import ComputerManager 1.0
import AutoUpdateChecker 1.0

ApplicationWindow {
    property bool pollingActive: false

    id: window
    visible: true
    width: 1280
    height: 600

    Material.theme: Material.Dark
    Material.accent: Material.Purple

    StackView {
        id: stackView
        initialItem: initialView
        anchors.fill: parent
        focus: true

        onCurrentItemChanged: {
            // Ensure focus travels to the next view when going back
            if (currentItem) {
                currentItem.forceActiveFocus()
            }
        }

        Keys.onEscapePressed: {
            if (depth > 1) {
                stackView.pop()
            }
        }

        Keys.onBackPressed: {
            if (depth > 1) {
                stackView.pop()
            }
        }

        Keys.onMenuPressed: {
            settingsButton.clicked()
        }
    }

    onVisibilityChanged: {
        // We don't want to just use 'active' here because that will stop polling if
        // we lose focus, which might be overzealous for users with multiple screens
        // where we may be clearly visible on the other display. Ideally we'll poll
        // only if the window is visible to the user (not if obscured by other windows),
        // but it seems difficult to do this portably.
        var shouldPoll = visibility !== Window.Minimized && visibility !== Window.Hidden

        if (shouldPoll && !pollingActive) {
            ComputerManager.startPolling()
            pollingActive = true
        }
        else if (!shouldPoll && pollingActive) {
            ComputerManager.stopPollingAsync()
            pollingActive = false
        }
    }

    function navigateTo(url, objectName)
    {
        var existingItem = stackView.find(function(item, index) {
            return item.objectName === objectName
        })

        if (existingItem !== null) {
            // Pop to the existing item
            stackView.pop(existingItem)
        }
        else {
            // Create a new item
            stackView.push(url)
        }
    }

    header: ToolBar {
        id: toolBar
        anchors.topMargin: 5
        anchors.bottomMargin: 5

        RowLayout {
            spacing: 20
            anchors.fill: parent

            NavigableToolButton {
                // Only make the button visible if the user has navigated somewhere.
                visible: stackView.depth > 1

                // FIXME: We're using an Image here rather than icon.source because
                // icons don't work on Qt 5.9 LTS.
                Image {
                    source: "qrc:/res/arrow_left.svg"
                    anchors.centerIn: parent
                    sourceSize {
                        // The icon should be square so use the height as the width too
                        width: toolBar.height - toolBar.anchors.bottomMargin - toolBar.anchors.topMargin
                        height: toolBar.height - toolBar.anchors.bottomMargin - toolBar.anchors.topMargin
                    }
                }

                onClicked: stackView.pop()

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
            }

            Label {
                id: titleLabel
                text: stackView.currentItem.objectName
                font.pointSize: 15
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }

            NavigableToolButton {
                property string browserUrl: ""

                id: updateButton

                Image {
                    source: "qrc:/res/update.svg"
                    anchors.centerIn: parent
                    sourceSize {
                        width: toolBar.height - toolBar.anchors.bottomMargin - toolBar.anchors.topMargin
                        height: toolBar.height - toolBar.anchors.bottomMargin - toolBar.anchors.topMargin
                    }
                }

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: "Update available for Moonlight"

                // Invisible until we get a callback notifying us that
                // an update is available
                visible: false

                onClicked: Qt.openUrlExternally(browserUrl);

                function updateAvailable(url)
                {
                    updateButton.browserUrl = url
                    updateButton.visible = true
                }

                Component.onCompleted: {
                    AutoUpdateChecker.onUpdateAvailable.connect(updateAvailable)
                    AutoUpdateChecker.start()
                }

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
            }

            NavigableToolButton {
                id: helpButton

                Image {
                    source: "qrc:/res/question_mark.svg"
                    anchors.centerIn: parent
                    sourceSize {
                        width: toolBar.height - toolBar.anchors.bottomMargin - toolBar.anchors.topMargin
                        height: toolBar.height - toolBar.anchors.bottomMargin - toolBar.anchors.topMargin
                    }
                }

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: "Help" + (helpShortcut.nativeText ? (" ("+helpShortcut.nativeText+")") : "")

                Shortcut {
                    id: helpShortcut
                    sequence: StandardKey.HelpContents
                    onActivated: helpButton.clicked()
                }

                // TODO need to make sure browser is brought to foreground.
                onClicked: Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide");

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
            }

            NavigableToolButton {
                // TODO: Implement gamepad mapping then unhide this button
                visible: false

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: "Gamepad Mapper"

                Image {
                    source: "qrc:/res/ic_videogame_asset_white_48px.svg"
                    anchors.centerIn: parent
                    sourceSize {
                        width: toolBar.height - toolBar.anchors.bottomMargin - toolBar.anchors.topMargin
                        height: toolBar.height - toolBar.anchors.bottomMargin - toolBar.anchors.topMargin
                    }
                }

                onClicked: navigateTo("qrc:/gui/GamepadMapper.qml", "Gamepad Mapping")

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
            }

            NavigableToolButton {
                id: settingsButton

                Image {
                    source: "qrc:/res/settings.svg"
                    anchors.centerIn: parent
                    sourceSize {
                        width: toolBar.height - toolBar.anchors.bottomMargin - toolBar.anchors.topMargin
                        height: toolBar.height - toolBar.anchors.bottomMargin - toolBar.anchors.topMargin
                    }
                }

                onClicked: navigateTo("qrc:/gui/SettingsView.qml", "Settings")

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }

                Shortcut {
                    id: settingsShortcut
                    sequence: StandardKey.Preferences
                    onActivated: settingsButton.clicked()
                }

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: "Settings" + (settingsShortcut.nativeText ? (" ("+settingsShortcut.nativeText+")") : "")
            }
        }
    }
}
