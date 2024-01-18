import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import QtQuick.Controls.Material 2.2
import QtQuick.Particles 2.0

import ComputerManager 1.0
import AutoUpdateChecker 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0

import ThemeManager 1.0

ApplicationWindow {
    property bool pollingActive: false

    // Set by SettingsView to force the back operation to pop all
    // pages except the initial view. This is required when doing
    // a retranslate() because AppView breaks for some reason.
    property bool clearOnBack: false

    id: window
    visible: true
    width: 1280
    height: 600

    Rectangle {
        anchors.fill: parent

        Loader {
            id: backgroundLoader
            anchors.fill: parent
            sourceComponent: ThemeManager.gameModeEnabled ? gameModeBackground : solidBackground
        }

        Component {
            id: gameModeBackground
            Rectangle {
                id: gameModeRectangle
                anchors.fill: window
                property string primaryColor: ThemeManager.primaryColor
                property string secondaryColor: ThemeManager.secondaryColor

                // compatibily to Qt 5.15 and Qt 6
                Component.onCompleted: {
                    var importByQtVersion = qtVersion.substring(0, 1) === "6" ? "Qt5Compat.GraphicalEffects" : "QtGraphicalEffects";
                    console.log(importByQtVersion)
                    var shadowEffect = Qt.createQmlObject(
                        'import '+importByQtVersion+';
                        ParticleSystem {
                            id: particleSystem
                            running: true
                        }

                        ImageParticle {
                            source: "qrc:/res/bubble.png"
                            system: particleSystem
                            alpha: 0.7
                        }

                        Emitter {
                            size:40
                            system: particleSystem
                            width: parent.width
                            height: parent.height
                            emitRate: 8
                            lifeSpan: 3000
                            velocity: AngleDirection { angle: 360; angleVariation: 360; magnitude: 50; magnitudeVariation: 50 }
                        }',
                        gameModeRectangle
                    );
                }

                ParticleSystem {
                    id: particleSystem
                    running: true
                }

                ImageParticle {
                    source: "qrc:/res/bubble.png"
                    system: particleSystem
                    alpha: 0.7
                }

                Emitter {
                    size:40
                    system: particleSystem
                    width: parent.width
                    height: parent.height
                    emitRate: 8
                    lifeSpan: 3000
                    velocity: AngleDirection { angle: 360; angleVariation: 360; magnitude: 50; magnitudeVariation: 50 }
                }

                gradient: Gradient {
                    GradientStop { position: 0; color: gameModeRectangle.primaryColor }
                    GradientStop { position: 1; color: gameModeRectangle.secondaryColor }
                }

                Connections {
                    target: ThemeManager
                    onPrimaryColorChanged: {
                        gameModeRectangle.primaryColor = ThemeManager.primaryColor;
                        gameModeRectangle.gradient = gameModeRectangle.gradient;
                        toolbarBackground.color = ThemeManager.primaryColor;
                    }
                    onSecondaryColorChanged: {
                        gameModeRectangle.secondaryColor = ThemeManager.secondaryColor;
                        gameModeRectangle.gradient = gameModeRectangle.gradient;
                    }
                }
            }
        }

        Component {
            id: solidBackground
            Rectangle {
                anchors.fill: parent
                color: "#303030"
            }
        }
    }

    Popup {
        id: settingsPopup
        modal: true
        padding: 50
        anchors.centerIn: parent

        contentItem: Column {
            anchors.fill: parent
            spacing: 20
            anchors.margins: 50

            RowLayout {
                Layout.alignment: Qt.AlignCenter
                spacing: 10
                Label {
                    text: qsTr("Theme settings")
                    font.bold: true
                    font.pointSize: 22
                }
            }

            RowLayout {
                spacing: 10
                Label {
                    text: qsTr("Choose application cover size")
                }

                Slider {
                    id: sizeSlider
                    from: 0.5
                    to: 2.0
                    stepSize: 0.1
                    value: ThemeManager.appImageSize
                }

                Label {
                    text: sizeSlider.value.toFixed(1)
                }
            }

            RowLayout {
                spacing: 10
                Label {
                    text: qsTr("Select primary color")
                }

                TextField {
                    id: primaryColorField
                    text: ThemeManager.getPrimaryColor()
                }
            }

            RowLayout {
                spacing: 10
                Label {
                    text: qsTr("Select secondary color")
                }

                TextField {
                    id: secondaryColorField
                    text: ThemeManager.getSecondaryColor()
                }
            }

            RowLayout {
                spacing: 10
                Label {
                    text: qsTr("Choose spacing")
                }

                Slider {
                    id: spacingSlider
                    from: 0
                    to: 120
                    stepSize: 2
                    value: ThemeManager.spacing
                }

                Label {
                    text: spacingSlider.value
                }
            }

            RowLayout {
                spacing: 10
                Label {
                    text: qsTr("Application name font size")
                }

                Slider {
                    id: fontSizeSlider
                    from: 15
                    to: 60
                    stepSize: 1
                    value: ThemeManager.appNameFontSize
                }

                Label {
                    text: fontSizeSlider.value
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10

                Button {
                    text: qsTr("Cancel")
                    onClicked: settingsPopup.close()
                }

                Button {
                    text: qsTr("Save")
                    onClicked: {
                        var primaryColorText = primaryColorField.text
                        var secondaryColorText = secondaryColorField.text

                        ThemeManager.setSpacing(spacingSlider.value)
                        ThemeManager.setPrimaryColor(primaryColorText)
                        ThemeManager.setSecondaryColor(secondaryColorText)
                        ThemeManager.setAppImageSize(sizeSlider.value)
                        ThemeManager.setAppNameFontSize(fontSizeSlider.value)
                        settingsPopup.close()
                    }
                }
            }
        }
    }


    // Override the background color to Material 2 colors for Qt 6.5+
    // in order to improve contrast between GFE's placeholder box art
    // and the background of the app grid.
    Component.onCompleted: {
        if (SystemProperties.usesMaterial3Theme) {
            Material.background = "#303030"
        }
    }

    visibility: {
        if (SystemProperties.hasDesktopEnvironment) {
            if(ThemeManager.gameModeEnabled){
                return "FullScreen"
            }else{
                if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_WINDOWED) return "Windowed"
                else if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_MAXIMIZED) return "Maximized"
                else if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_FULLSCREEN) return "FullScreen"
            }
        } else {
            return "FullScreen"
        }
    }
  
    // This configures the maximum width of the singleton attached QML ToolTip. If left unconstrained,
    // it will never insert a line break and just extend on forever.
    ToolTip.toolTip.contentWidth: ToolTip.toolTip.implicitContentWidth < 400 ? ToolTip.toolTip.implicitContentWidth : 400
    GridView {
        id: myStackView
    }

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
        height: ThemeManager.gameModeEnabled ? 220 : 60
        anchors.topMargin: 5
        anchors.bottomMargin: 5
        topPadding: ThemeManager.gameModeEnabled ? 60 : 0
        bottomPadding: ThemeManager.gameModeEnabled ? 60 : 0
        background: Rectangle {
            id: toolbarBackground
            color: ThemeManager.gameModeEnabled ? ThemeManager.getPrimaryColor() : "#3F51B5"
        }

        Label {
            id: titleLabel
            visible: toolBar.width > 700
            anchors.fill: parent
            text: stackView.currentItem.objectName
            font.pointSize: ThemeManager.gameModeEnabled ? 28 : 20
            elide: Label.ElideRight
            anchors.leftMargin: ThemeManager.gameModeEnabled ? 170 : 0
            horizontalAlignment: ThemeManager.gameModeEnabled ? Qt.AlignHLeft : Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
        }

        RowLayout {
            spacing: ThemeManager.gameModeEnabled ? 50 : 10
            anchors.leftMargin: ThemeManager.gameModeEnabled ? 100 : 0
            anchors.rightMargin: ThemeManager.gameModeEnabled ? 100 : 0
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

            NavigableToolButton {
                id: themeSettings
                icon.source: "qrc:/res/theme_edit.svg"
                icon.height: 48
                icon.width: 48

                onClicked: {
                    settingsPopup.open()
                }
                visible: ThemeManager.gameModeEnabled

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Edit theme settings")

                Keys.onDownPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
            }

            Button {
                id: gamepadToggleButtonn
                flat: true
                icon.source: "qrc:/res/game_mode.svg"
                icon.height: ThemeManager.gameModeEnabled ? 48 : 24
                icon.width: ThemeManager.gameModeEnabled ? 74 : 37
                icon.color: ThemeManager.gameModeEnabled ? "#040687" : "#FFF"

                onClicked: {
                    ThemeManager.setGameModeEnabled(!ThemeManager.gameModeEnabled)
                }

                ToolTip.delay: 1000
                ToolTip.timeout: 5000
                ToolTip.visible: hovered
                ToolTip.text: ThemeManager.gameModeEnabled ? qsTr("Disable game mode") : qsTr("Enable game mode")
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
}
