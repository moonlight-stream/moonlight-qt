import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2

import QtQuick.Controls.Material 2.1

import ComputerManager 1.0
import AutoUpdateChecker 1.0
import StreamingPreferences 1.0

ApplicationWindow {
    property bool pollingActive: false

    id: window
    visible: true
    width: 1280
    height: 600

    // Maximize the window by default when the stream is configured
    // for full-screen or borderless windowed. This is ideal for TV
    // setups where the user doesn't want a tiny window in the middle
    // of their screen when starting Moonlight.
    visibility: prefs.windowMode != StreamingPreferences.WM_WINDOWED ? "Maximized" : "Windowed"

    Material.theme: Material.Dark
    Material.accent: Material.Purple

    StreamingPreferences {
        id: prefs
    }

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
            else {
                quitConfirmationDialog.open()
            }
        }

        Keys.onBackPressed: {
            if (depth > 1) {
                stackView.pop()
            }
            else {
                quitConfirmationDialog.open()
            }
        }

        Keys.onMenuPressed: {
            settingsButton.clicked()
        }

        // This is a keypress we've reserved for letting the
        // SdlGamepadKeyNavigation object tell us to show settings
        // when Menu is consumed by a focused control.
        Keys.onHangupPressed: {
            settingsButton.clicked()
        }
    }

    // This timer keeps us polling for 5 minutes of inactivity
    // to allow the user to work with Moonlight on a second display
    // while dealing with configuration issues. This will ensure
    // machines come online even if the input focus isn't on Moonlight.
    Timer {
        id: inactivityTimer
        interval: 5 * 60000
        onTriggered: {
            if (!active && pollingActive) {
                ComputerManager.stopPollingAsync()
                pollingActive = false
            }
        }
    }

    onVisibleChanged: {
        // When we become invisible while streaming is going on,
        // stop polling immediately.
        if (!visible) {
            inactivityTimer.stop()

            if (pollingActive) {
                ComputerManager.stopPollingAsync()
                pollingActive = false
            }
        }
        else if (active) {
            // When we become visible and active again, start polling
            inactivityTimer.stop()

            // Restart polling if it was stopped
            if (!pollingActive) {
                ComputerManager.startPolling()
                pollingActive = true
            }
        }
    }

    onActiveChanged: {
        if (active) {
            // Stop the inactivity timer
            inactivityTimer.stop()

            // Restart polling if it was stopped
            if (!pollingActive) {
                ComputerManager.startPolling()
                pollingActive = true
            }
        }
        else {
            // Start the inactivity timer to stop polling
            // if focus does not return within a few minutes.
            inactivityTimer.restart()
        }
    }

    property bool initialized: false

    // BUG: Using onAfterSynchronizing: here causes very strange
    // failures on Linux. Many shaders fail to compile and we
    // eventually segfault deep inside the Qt OpenGL code.
    onAfterRendering: {
        // We use this callback to trigger dialog display because
        // it only happens once the window is fully constructed.
        // Doing it earlier can lead to the dialog appearing behind
        // the window or otherwise without input focus.
        if (!initialized) {
            if (prefs.isRunningWayland()) {
                waylandDialog.open()
            }
            else if (prefs.isWow64()) {
                wow64Dialog.open()
            }
            else if (!prefs.hasAnyHardwareAcceleration()) {
                noHwDecoderDialog.open()
            }

            var unmappedGamepads = prefs.getUnmappedGamepads()
            if (unmappedGamepads) {
                unmappedGamepadDialog.unmappedGamepads = unmappedGamepads
                unmappedGamepadDialog.open()
            }

            initialized = true;
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

    MessageDialog {
        id: noHwDecoderDialog
        modality:Qt.WindowModal
        icon: StandardIcon.Warning
        standardButtons: StandardButton.Ok | StandardButton.Help
        text: "No functioning hardware accelerated H.264 video decoder was detected by Moonlight. " +
              "Your streaming performance may be severely degraded in this configuration. " +
              "Click the Help button for more information on solving this problem."
        onHelp: {
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems");
        }
    }

    MessageDialog {
        id: waylandDialog
        modality:Qt.WindowModal
        icon: StandardIcon.Warning
        standardButtons: StandardButton.Ok | StandardButton.Help
        text: "Moonlight does not support hardware acceleration on Wayland. Continuing on Wayland may result in poor streaming performance. " +
              "Please switch to an X session for optimal performance."
        onHelp: {
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems");
        }
    }

    MessageDialog {
        id: wow64Dialog
        modality:Qt.WindowModal
        icon: StandardIcon.Warning
        standardButtons: StandardButton.Ok | StandardButton.Cancel
        text: "This PC is running a 64-bit version of Windows. Please download the x64 version of Moonlight for the best streaming performance."
        onAccepted: {
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-qt/releases");
        }
    }

    MessageDialog {
        id: unmappedGamepadDialog
        property string unmappedGamepads : ""
        modality:Qt.WindowModal
        icon: StandardIcon.Warning
        standardButtons: StandardButton.Ok | StandardButton.Help
        text: "Moonlight detected gamepads without a proper mapping. " +
              "The following gamepads will not function until this is resolved: " + unmappedGamepads + "\n\n" +
              "Click the Help button for information on how to map your gamepads."
        onHelp: {
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Gamepad-Mapping");
        }
    }

    // This dialog appears when quitting via keyboard or gamepad button
    MessageDialog {
        id: quitConfirmationDialog
        modality:Qt.WindowModal
        icon: StandardIcon.Warning
        standardButtons: StandardButton.Yes | StandardButton.No
        text: "Are you sure you want to quit?"

        onYes: Qt.quit()

        // For keyboard/gamepad navigation
        onAccepted: Qt.quit()
    }
}
