import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import QtQuick.Controls.Material 2.2

import ComputerManager 1.0
import AutoUpdateChecker 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0

ApplicationWindow {
    readonly property bool lightPage: !stackView.currentItem || stackView.currentItem instanceof PcView ||
        stackView.currentItem instanceof SettingsView || stackView.currentItem instanceof StreamSegue ||
        stackView.currentItem instanceof CliStartStreamSegue || stackView.currentItem instanceof QuitSegue
    Material.theme: lightPage ? Material.Light : Material.Dark
    Material.background: lightPage ? "#f7f9fc" : "#303030"
    Material.accent: lightPage ? "#2468d5" : Material.Purple
    property bool pollingActive: false
    readonly property bool customTitleBar: Qt.platform.os === "windows" && isGuiLaunch

    function toggleWindowSize() {
        if (visibility === Window.Maximized || visibility === Window.FullScreen)
            showNormal()
        else
            showMaximized()
    }

    id: window
    title: !isGuiLaunch && stackView.currentItem && stackView.currentItem.streamProgressText
           ? stackView.currentItem.streamProgressText : Qt.application.name
    width: isGuiLaunch ? 960 : 1280
    height: isGuiLaunch ? 665 : 600

    // This function runs prior to creation of the initial StackView item
    function doEarlyInit() {
        SdlGamepadKeyNavigation.enable()
    }

    function showInitialWindow() {
        // Show the window according to the user's preferences
        if (SystemProperties.hasDesktopEnvironment) {
            if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_MAXIMIZED) {
                window.showMaximized()
            }
            else if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_FULLSCREEN) {
                window.showFullScreen()
            }
            else {
                window.show()
            }
        } else {
            window.showFullScreen()
        }
    }

    Component.onCompleted: {
        if (customTitleBar)
            flags |= Qt.FramelessWindowHint

        if (!deferInitialShow)
            showInitialWindow()

        // Display any modal dialogs for configuration warnings
        if (isGuiLaunch) {
            if (SystemProperties.isWow64) {
                wow64Dialog.open()
            }

            // Hardware acceleration and unmapped gamepads are checked asynchronously
            SystemProperties.hasHardwareAccelerationChanged.connect(hasHardwareAccelerationChanged)
            SystemProperties.unmappedGamepadsChanged.connect(hasUnmappedGamepadsChanged)
            SystemProperties.startAsyncLoad()
        }
    }

    function hasHardwareAccelerationChanged() {
        if (!SystemProperties.hasHardwareAcceleration && StreamingPreferences.videoDecoderSelection !== StreamingPreferences.VDS_FORCE_SOFTWARE) {
            if (SystemProperties.isRunningXWayland) {
                xWaylandDialog.open()
            }
            else {
                noHwDecoderDialog.open()
            }
        }
    }

    function hasUnmappedGamepadsChanged() {
        if (SystemProperties.unmappedGamepads) {
            unmappedGamepadDialog.unmappedGamepads = SystemProperties.unmappedGamepads
            unmappedGamepadDialog.open()
        }
    }

    // It would be better to use TextMetrics here, but it always lays out
    // the text slightly more compactly than real Text does in ToolTip,
    // causing unexpected line breaks to be inserted
    Text {
        id: tooltipTextLayoutHelper
        visible: false
        font: ToolTip.toolTip.font
        text: ToolTip.toolTip.text
    }

    // This configures the maximum width of the singleton attached QML ToolTip. If left unconstrained,
    // it will never insert a line break and just extend on forever.
    ToolTip.toolTip.contentWidth: Math.min(tooltipTextLayoutHelper.width, 400)

    function launchGuiSession(session) {
        if (Qt.platform.os === "windows" && isGuiLaunch) {
            if (!session.launchInWindow()) {
                streamSegueErrorDialog.text = qsTr("Unable to open the streaming window.")
                streamSegueErrorDialog.open()
            }
        } else {
            stackView.push(Qt.resolvedUrl("StreamSegue.qml"), {
                "session": session, "appName": session.appName
            })
        }
    }

    onClosing: function(close) {
        if (trayAvailable && StreamingPreferences.closeToTray) {
            close.accepted = false
            hide()
        }
    }

    function goBack() {
        if (toolBar.devicePage && stackView.currentItem.showingSettings)
            stackView.currentItem.showingSettings = false
        else
            stackView.pop()
    }

    StackView {
        id: stackView
        anchors.fill: parent
        focus: true

        Component.onCompleted: {
            // Perform our early initialization before constructing
            // the initial view and pushing it to the StackView
            doEarlyInit()
            push(initialView)
        }

        onCurrentItemChanged: {
            // Ensure focus travels to the next view when going back
            if (currentItem) {
                currentItem.forceActiveFocus()
            }
        }

        Keys.onEscapePressed: {
            if (depth > 1 || toolBar.settingsPage) {
                goBack()
            }
            else {
                quitConfirmationDialog.open()
            }
        }

        Keys.onBackPressed: {
            if (depth > 1 || toolBar.settingsPage) {
                goBack()
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
        // Polling is only managed from the window for a normal GUI launch;
        // CLI commands (stream/quit/pair) manage it via the ComputerSeeker.
        if (isGuiLaunch) {
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

        // Poll for gamepad input only when the window is in focus
        SdlGamepadKeyNavigation.notifyWindowFocus(visible && active)
    }

    onActiveChanged: {
        // Polling is only managed from the window for a normal GUI launch;
        // CLI commands (stream/quit/pair) manage it via the ComputerSeeker.
        if (isGuiLaunch) {
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

        // Poll for gamepad input only when the window is in focus
        SdlGamepadKeyNavigation.notifyWindowFocus(visible && active)
    }

    function navigateTo(url, objectType)
    {
        var existingItem = stackView.find(function(item, index) {
            return item instanceof objectType
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

    Shortcut {
        id: newPcShortcut
        sequence: StandardKey.New
        enabled: isGuiLaunch
        onActivated: addPcDialog.open()
    }

    Shortcut {
        sequence: StandardKey.HelpContents
        enabled: SystemProperties.hasBrowser && isGuiLaunch
        onActivated: Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide")
    }

    header: ToolBar {
        id: toolBar
        readonly property bool settingsPage: devicePage && stackView.currentItem.showingSettings
        readonly property bool devicePage: stackView.currentItem instanceof PcView
        Material.theme: window.Material.theme
        Material.background: window.lightPage ? "#eff4f7" : Material.primary
        MouseArea {
            anchors.fill: parent
            enabled: window.customTitleBar
            property point pressPoint
            onPressed: function(mouse) { pressPoint = Qt.point(mouse.x, mouse.y) }
            onPositionChanged: function(mouse) {
                if (pressed && window.visibility !== Window.FullScreen &&
                        Math.abs(mouse.x - pressPoint.x) + Math.abs(mouse.y - pressPoint.y) >= Qt.styleHints.startDragDistance)
                    window.startSystemMove()
            }
            onDoubleClicked: window.toggleWindowSize()
        }
        height: window.customTitleBar ? 46 : (devicePage ? 38 : 60)
        anchors.topMargin: 5
        anchors.bottomMargin: 5

        Label {
            id: titleLabel
            visible: toolBar.width > 700 && !toolBar.devicePage && !toolBar.settingsPage
            anchors.fill: parent
            text: stackView.currentItem.objectName
            font.pointSize: 20
            elide: Label.ElideRight
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
        }

        RowLayout {
            spacing: 8
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.fill: parent

            NavigableToolButton {
                // Only make the button visible if the user has navigated somewhere.
                visible: stackView.depth > 1

                icon.width: 18
                icon.height: 18
                iconSource: "qrc:/res/arrow_left.svg"

                onClicked: goBack()

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
            }

            Image {
                visible: toolBar.devicePage
                Layout.leftMargin: 6
                Layout.preferredWidth: 19; Layout.preferredHeight: 19
                source: "qrc:/res/moonlight.svg"
            }
            Label {
                visible: toolBar.devicePage && toolBar.width >= 410
                text: "Moonlight"
                font.pixelSize: 15; font.bold: true; color: "#203047"
            }
            Label {
                visible: toolBar.devicePage
                text: qsTr("Version %1").arg(SystemProperties.versionString)
                font.pixelSize: 12; color: "#52647a"
            }
            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                font: titleLabel.font
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                text: !toolBar.devicePage && !titleLabel.visible && stackView.currentItem ?
                          stackView.currentItem.objectName : ""
            }

            NavigableToolButton {
                property string browserUrl: ""

                id: updateButton

                iconSource: "qrc:/res/update.svg"

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered || visible

                // Invisible until we get a callback notifying us that
                // an update is available
                visible: false

                onClicked: {
                    if (SystemProperties.hasBrowser) {
                        Qt.openUrlExternally(browserUrl);
                    }
                }

                function updateAvailable(version, url)
                {
                    ToolTip.text = qsTr("Update available for Moonlight: Version %1").arg(version)
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
                // TODO: Implement gamepad mapping then unhide this button
                visible: false

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Gamepad Mapper")

                iconSource: "qrc:/res/ic_videogame_asset_white_48px.svg"

                onClicked: navigateTo("qrc:/gui/GamepadMapper.qml", GamepadMapper)

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
            }

            NavigableToolButton {
                id: settingsButton
                visible: isGuiLaunch && !toolBar.devicePage
                Accessible.name: qsTr("Settings")

                iconSource:  "qrc:/res/settings.svg"

                onClicked: {
                    var devices = stackView.get(0)
                    if (!(devices instanceof PcView)) return
                    if (stackView.currentItem !== devices)
                        stackView.pop(devices, StackView.Immediate)
                    devices.openSettings()
                }

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
                ToolTip.text: qsTr("Settings") + (settingsShortcut.nativeText ? (" ("+settingsShortcut.nativeText+")") : "")
            }

            Row {
                visible: window.customTitleBar
                Layout.fillHeight: true
                Repeater {
                    model: 2
                    ToolButton {
                        id: captionButton
                        readonly property bool isClose: index === 1
                        readonly property bool showHighlight: hovered || visualFocus || down
                        width: 46
                        height: parent.height
                        padding: 0
                        hoverEnabled: true
                        Accessible.name: isClose ? qsTr("Close") : qsTr("Minimize")
                        ToolbarToolTip { text: captionButton.Accessible.name }
                        font.family: "Segoe MDL2 Assets"
                        text: isClose ? "\ue8bb" : "\ue921"
                        font.pixelSize: 10
                        Material.foreground: isClose && showHighlight ? "white" : toolBar.Material.foreground
                        background: Item {
                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width - 6
                                height: 30
                                radius: 6
                                visible: captionButton.showHighlight
                                color: {
                                    if (captionButton.isClose)
                                        return captionButton.down ? "#a62318" : "#c42b1c"
                                    if (window.lightPage)
                                        return captionButton.down ? "#ccd9e0" : "#dde7ec"
                                    return captionButton.down ? "#56616b" : "#454e57"
                                }
                                border.width: captionButton.visualFocus ? 1 : 0
                                border.color: captionButton.isClose ? "white" : toolBar.Material.foreground
                            }
                        }
                        onClicked: {
                            if (isClose) window.close()
                            else window.showMinimized()
                        }
                    }
                }
            }
        }
    }

    // The frame handles share the header's parent so they cover the full window,
    // including the header. Only the narrow perimeter accepts mouse events.
    Repeater {
        parent: toolBar.parent
        model: window.customTitleBar && window.visibility === Window.Windowed ?
                   [Qt.LeftEdge, Qt.RightEdge, Qt.TopEdge, Qt.BottomEdge,
                    Qt.TopEdge | Qt.LeftEdge, Qt.TopEdge | Qt.RightEdge,
                    Qt.BottomEdge | Qt.LeftEdge, Qt.BottomEdge | Qt.RightEdge] : []
        MouseArea {
            z: 20
            readonly property int edges: modelData
            readonly property bool horizontalEdge: (edges & (Qt.LeftEdge | Qt.RightEdge)) !== 0
            readonly property bool verticalEdge: (edges & (Qt.TopEdge | Qt.BottomEdge)) !== 0
            width: horizontalEdge ? 5 : parent.width - 10
            height: verticalEdge ? 5 : parent.height - 10
            x: (edges & Qt.RightEdge) ? parent.width - width : horizontalEdge ? 0 : 5
            y: (edges & Qt.BottomEdge) ? parent.height - height : verticalEdge ? 0 : 5
            cursorShape: horizontalEdge && verticalEdge ?
                (edges === (Qt.TopEdge | Qt.LeftEdge) || edges === (Qt.BottomEdge | Qt.RightEdge) ?
                     Qt.SizeFDiagCursor : Qt.SizeBDiagCursor) : horizontalEdge ? Qt.SizeHorCursor : Qt.SizeVerCursor
            onPressed: window.startSystemResize(edges)
        }
    }

    ErrorMessageDialog {
        id: noHwDecoderDialog
        text: qsTr("No functioning hardware accelerated video decoder was detected by Moonlight. " +
                   "Your streaming performance may be severely degraded in this configuration.")
        helpText: qsTr("Click the Help button for more information on solving this problem.")
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems"
    }

    ErrorMessageDialog {
        id: xWaylandDialog
        text: qsTr("Hardware acceleration doesn't work on XWayland. Continuing on XWayland may result in poor streaming performance. " +
                   "Try running with QT_QPA_PLATFORM=wayland or switch to X11.")
        helpText: qsTr("Click the Help button for more information.")
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems"
    }

    NavigableMessageDialog {
        id: wow64Dialog
        standardButtons: Dialog.Ok | Dialog.Cancel
        text: qsTr("This version of Moonlight isn't optimized for your PC. Please download the '%1' version of Moonlight for the best streaming performance.").arg(SystemProperties.friendlyNativeArchName)
        onAccepted: {
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-qt/releases");
        }
    }

    ErrorMessageDialog {
        id: unmappedGamepadDialog
        property string unmappedGamepads : ""
        text: qsTr("Moonlight detected gamepads without a mapping:") + "\n" + unmappedGamepads
        helpTextSeparator: "\n\n"
        helpText: qsTr("Click the Help button for information on how to map your gamepads.")
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Gamepad-Mapping"
    }

    // This dialog appears when quitting via keyboard or gamepad button
    NavigableMessageDialog {
        id: quitConfirmationDialog
        standardButtons: Dialog.Yes | Dialog.No
        text: qsTr("Are you sure you want to quit?")
        // For keyboard/gamepad navigation
        onAccepted: Qt.quit()
    }

    // HACK: This belongs in StreamSegue but keeping a dialog around after the parent
    // dies can trigger bugs in Qt 5.12 that cause the app to crash. For now, we will
    // host this dialog in a QML component that is never destroyed.
    //
    // To repro: Start a stream, cut the network connection to trigger the "Connection
    // terminated" dialog, wait until the app grid times out back to the PC grid, then
    // try to dismiss the dialog.
    ErrorMessageDialog {
        id: streamSegueErrorDialog

        property bool quitAfter: false

        onClosed: {
            if (quitAfter) {
                Qt.quit()
            }

            // StreamSegue assumes its dialog will be re-created each time we
            // start streaming, so fake it by wiping out the text each time.
            text = ""
        }
    }

    NavigableDialog {
        id: addPcDialog
        property string label: qsTr("Enter the IP address of your host PC:")

        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {
            // Force keyboard focus on the textbox so keyboard navigation works
            editText.forceActiveFocus()
        }

        onClosed: {
            editText.clear()
        }

        onAccepted: {
            if (editText.text) {
                ComputerManager.addNewHostManually(editText.text.trim())
            }
        }

        ColumnLayout {
            Label {
                text: addPcDialog.label
                font.bold: true
            }

            TextField {
                id: editText
                Layout.fillWidth: true
                focus: true

                Keys.onReturnPressed: {
                    addPcDialog.accept()
                }

                Keys.onEnterPressed: {
                    addPcDialog.accept()
                }
            }
        }
    }
}
