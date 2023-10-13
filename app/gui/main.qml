import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import QtQuick.Controls.Material 2.2

import ComputerManager 1.0
import AutoUpdateChecker 1.0
import StreamingPreferences 1.0
import HotkeyModel 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0
import HotkeyManager 1.0

ApplicationWindow {
    property string tag: "main"

    property bool pollingActive: false

    // Set by SettingsView to force the back operation to pop all
    // pages except the initial view. This is required when doing
    // a retranslate() because AppView breaks for some reason.
    property bool clearOnBack: false

    property HotkeyModel hotkeyModel : createHotkeyModel()

    function createComputerModel()
    {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', window, '')
        model.initialize(ComputerManager)
        return model
    }

    function createAppModel(computerIndex, showHiddenGames) {
        var appModel = Qt.createQmlObject('import AppModel 1.0; AppModel {}', window, '')
        appModel.initialize(ComputerManager, computerIndex, showHiddenGames)
        return appModel
    }

    function createHotkeyModel() {
        var model = Qt.createQmlObject('import HotkeyModel 1.0; HotkeyModel {}', window, '')
        model.initialize(ComputerManager, HotkeyManager)
        return model
    }

    id: window
    visible: true
    width: 1280
    height: 600

    // Override the background color to Material 2 colors for Qt 6.5+
    // in order to improve contrast between GFE's placeholder box art
    // and the background of the app grid.
    Component.onCompleted: {
        if (SystemProperties.usesMaterial3Theme) {
            Material.background = "#303030"
        }
        HotkeyManager.hotkeysChanged.connect(hotkeysChanged)
        shortcutsRefresh()
    }

    Component.onDestruction: {
        HotkeyManager.hotkeysChanged.disconnect(hotkeysChanged)
    }

    property var shortcuts : []

    function hotkeysChanged() {
        log(tag, "hotkeysChanged()")
        shortcutsRefresh()
    }

    function shortcutsPrint() {
        var text = "[ "
        shortcuts.forEach(function (shortcut, index) {
            if (index > 0) {
                text += ", "
            }
            text += `shortcut.sequence="${shortcut.sequence}"`
        })
        text += " ]"
        log(tag, `shortcuts=${text}`)
    }

    function shortcutsRefresh() {
        while (shortcuts.length > 0) {
            var shortcut = shortcuts.pop()
            // We must unset existing sequence before we can
            // successfully create a usable replacement below
            shortcut.sequence = null;
        }
        HotkeyManager.hotkeyNumbers().forEach(function (hotkeyNumber) {
            var hotkeyInfo = HotkeyManager.get(hotkeyNumber)
            var shortcut = Qt.createQmlObject('import QtQuick 2.9; Shortcut {}', window, '')
            shortcut.sequence = `Ctrl+Alt+Shift+${hotkeyNumber}`
            shortcut.activated.connect(function () {
                launchApp(hotkeyInfo.computerName, hotkeyInfo.appName)
            })
            shortcuts.push(shortcut)
        })
        shortcutsPrint()
    }

    function launchApp(computerName, appName, quitExistingApp) {
        log(tag, `launchApp("${computerName}", "${appName}")`)

        // Temporarily very similar to launchOrResumeSelectedApp...

        var computerIndex = ComputerManager.getComputerIndex(computerName)
        log(tag, "computerIndex=" + computerIndex)
        if (computerIndex < 0) {
            return
        }

        var appModel = createAppModel(computerIndex, true)

        var appIndex = appModel.getAppIndex(appName)
        log(tag, "appIndex=" + appIndex)
        if (appIndex < 0) {
            return
        }

        var runningAppName = appModel.getRunningAppName()
        log(tag, "runningAppName=" + runningAppName)
        if (runningAppName.length > 0 && runningAppName !== appName) {
            if (quitExistingApp) {
                quitAppDialog.appName = runningAppName
                quitAppDialog.segueToStream = true
                quitAppDialog.nextAppName = appName
                quitAppDialog.nextAppIndex = appIndex
                quitAppDialog.open()
            }
            return
        }

        var session = appModel.createSessionForApp(appIndex)
        var isResume = runningAppName === appName
        startStream(appModel, appName, session, isResume)
    }

    function startStream(appModel, appName, session, isResume) {
        var component = Qt.createComponent("StreamSegue.qml")
        var segue = component.createObject(stackView, {
                                               "appName": appName,
                                               "session": session,
                                               "isResume": isResume
                                           })
        stackView.push(segue)
    }

    visibility: {
        if (SystemProperties.hasDesktopEnvironment) {
            if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_WINDOWED) return "Windowed"
            else if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_MAXIMIZED) return "Maximized"
            else if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_FULLSCREEN) return "FullScreen"
        } else {
            return "FullScreen"
        }
    }
  
    // This configures the maximum width of the singleton attached QML ToolTip. If left unconstrained,
    // it will never insert a line break and just extend on forever.
    ToolTip.toolTip.contentWidth: ToolTip.toolTip.implicitContentWidth < 400 ? ToolTip.toolTip.implicitContentWidth : 400

    function goBack() {
        if (clearOnBack) {
            // Pop all items except the first one
            stackView.pop(null)
            clearOnBack = false
        }
        else {
            stackView.pop()
        }
    }

    StackView {
        id: stackView
        initialItem: initialView
        anchors.fill: parent
        focus: true

        onCurrentItemChanged: {
            log(tag, "onCurrentItemChanged: stackView.currentItem=" + currentItem)

            // Ensure focus travels to the next view when going back
            if (currentItem) {
                currentItem.forceActiveFocus()
            }
        }

        Keys.onEscapePressed: {
            if (depth > 1) {
                goBack()
            }
            else {
                quitConfirmationDialog.open()
            }
        }

        Keys.onBackPressed: {
            if (depth > 1) {
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
        log(tag, "onActiveChanged: active=" + active)
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
            // Set initialized before calling anything else, because
            // pumping the event loop can cause us to get another
            // onAfterRendering call and potentially reenter this code.
            initialized = true;

            if (SystemProperties.isWow64) {
                wow64Dialog.open()
            }
            else if (!SystemProperties.hasHardwareAcceleration) {
                if (SystemProperties.isRunningXWayland) {
                    xWaylandDialog.open()
                }
                else {
                    noHwDecoderDialog.open()
                }
            }

            if (SystemProperties.unmappedGamepads) {
                unmappedGamepadDialog.unmappedGamepads = SystemProperties.unmappedGamepads
                unmappedGamepadDialog.open()
            }
        }
    }

    // Workaround for lack of instanceof in Qt 5.9.
    //
    // Based on https://stackoverflow.com/questions/13923794/how-to-do-a-is-a-typeof-or-instanceof-in-qml
    function qmltypeof(obj, className) { // QtObject, string -> bool
        // className plus "(" is the class instance without modification
        // className plus "_QML" is the class instance with user-defined properties
        var str = obj.toString();
        return str.startsWith(className + "(") || str.startsWith(className + "_QML");
    }

    function navigateTo(url, objectType)
    {
        var existingItem = stackView.find(function(item, index) {
            return qmltypeof(item, objectType)
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
        height: 60
        anchors.topMargin: 5
        anchors.bottomMargin: 5

        Label {
            id: titleLabel
            visible: toolBar.width > 700
            anchors.fill: parent
            text: stackView.currentItem.objectName
            font.pointSize: 20
            elide: Label.ElideRight
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
        }

        RowLayout {
            spacing: 10
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.fill: parent

            NavigableToolButton {
                // Only make the button visible if the user has navigated somewhere.
                visible: stackView.depth > 1

                iconSource: "qrc:/res/arrow_left.svg"

                onClicked: goBack()

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
            }

            // This label will appear when the window gets too small and
            // we need to ensure the toolbar controls don't collide
            Label {
                id: titleRowLabel
                font.pointSize: titleLabel.font.pointSize
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                Layout.fillWidth: true

                // We need this label to always be visible so it can occupy
                // the remaining space in the RowLayout. To "hide" it, we
                // just set the text to empty string.
                text: !titleLabel.visible ? stackView.currentItem.objectName : ""
            }

            Label {
                id: versionLabel
                visible: qmltypeof(stackView.currentItem, "SettingsView")
                text: qsTr("Version %1").arg(SystemProperties.versionString)
                font.pointSize: 12
                horizontalAlignment: Qt.AlignRight
                verticalAlignment: Qt.AlignVCenter
            }

            NavigableToolButton {
                id: discordButton
                visible: SystemProperties.hasBrowser &&
                         qmltypeof(stackView.currentItem, "SettingsView")

                iconSource: "qrc:/res/Discord-Logo-White.svg"

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Join our community on Discord")

                // TODO need to make sure browser is brought to foreground.
                onClicked: Qt.openUrlExternally("https://moonlight-stream.org/discord");

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
            }

            NavigableToolButton {
                id: addPcButton
                visible: qmltypeof(stackView.currentItem, "PcView")

                iconSource:  "qrc:/res/ic_add_to_queue_white_48px.svg"

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Add PC manually") + (newPcShortcut.nativeText ? (" ("+newPcShortcut.nativeText+")") : "")

                Shortcut {
                    id: newPcShortcut
                    sequence: StandardKey.New
                    onActivated: addPcButton.clicked()
                }

                onClicked: {
                    addPcDialog.open()
                }

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
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
                id: helpButton
                visible: SystemProperties.hasBrowser

                iconSource: "qrc:/res/question_mark.svg"

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Help") + (helpShortcut.nativeText ? (" ("+helpShortcut.nativeText+")") : "")

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
                id: hotkeysButton

                visible: qmltypeof(stackView.currentItem, "PcView") || qmltypeof(stackView.currentItem, "AppView")

                iconSource: "qrc:/res/keyboard.svg"

                onClicked: {
                    StreamingPreferences.initialView = "HotkeysView"
                    StreamingPreferences.save()

                    var component = Qt.createComponent("HotkeysView.qml")
                    var hotkeysView = component.createObject(stackView)
                    stackView.replace(null, hotkeysView)
                }

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }

                Shortcut {
                    id: hotkeysShortcut
                    sequence: "Ctrl+Alt+Shift+H"
                    onActivated: hotkeysButton.clicked()
                }

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Hotkeys") + (hotkeysShortcut.nativeText ? (" ("+hotkeysShortcut.nativeText+")") : "")
            }

            NavigableToolButton {
                id: pcsButton

                visible: qmltypeof(stackView.currentItem, "HotkeysView")

                iconSource: "qrc:/res/desktop_windows-48px.svg"

                onClicked: {
                    StreamingPreferences.initialView = "PcView"
                    StreamingPreferences.save()

                    var component = Qt.createComponent("PcView.qml")
                    var pcView = component.createObject(stackView)
                    stackView.replace(null, pcView)
                }

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }

                Shortcut {
                    id: pcsShortcut
                    sequence: "Ctrl+Alt+Shift+P"
                    onActivated: pcsButton.clicked()
                }

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: qsTr("PCs") + (pcsShortcut.nativeText ? (" ("+pcsShortcut.nativeText+")") : "")
            }

            NavigableToolButton {
                // TODO: Implement gamepad mapping then unhide this button
                visible: false

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Gamepad Mapper")

                iconSource: "qrc:/res/ic_videogame_asset_white_48px.svg"

                onClicked: navigateTo("qrc:/gui/GamepadMapper.qml", "GamepadMapper")

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
            }

            NavigableToolButton {
                id: settingsButton

                iconSource:  "qrc:/res/settings.svg"

                onClicked: navigateTo("qrc:/gui/SettingsView.qml", "SettingsView")

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

    function displayToast(text) {
        toastText.text = text
        toastText.visible = true
        toastTimer.restart()
        log(tag, text)
    }

    Label {
        id: toastText
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        font.pointSize: 18
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap

        Timer {
            id: toastTimer
            // This toast appears for 3 seconds, just shorter than how long
            // Session will wait for it to be displayed. This gives it time
            // to transition to invisible before continuing.
            interval: 3000
            onTriggered: {
                toastText.visible = false
            }
        }
    }

    function log() {
        var heavy = true
        if (heavy) {
            var dateString = new Date().toISOString()
            console.log(`${dateString}: ${Array.prototype.slice.call(arguments).join(" ")}`)
        } else {
            console.log(Array.prototype.slice.call(arguments).join(" "))
        }
    }
}
