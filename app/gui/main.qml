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
    property bool pollingActive: false
    property int previousX
    property int previousY

    id: window
    visible: true
    width: 1280
    height: 700
    flags: Qt.FramelessWindowHint | Qt.Window

    visibility: StreamingPreferences.startWindowed ? "Windowed" : "Maximized"

    background: Image {
        id: backgroundImage
        source: "qrc:/res/background.svg"
        sourceSize.width: 1000
        sourceSize.height: 1230
        width: 200
        height: 246
        x: (parent.width - width + 65) / 2
        y: (parent.height - height) / 2

        visible: false
    }

    FontLoader {
        id: iconFont;
        source: "qrc:/data/res/iconfont/iconfont.ttf"
    }

    Component.onCompleted: {
        SdlGamepadKeyNavigation.enable()
    }

    MouseArea {
        id: rightResize
        width: 5
        anchors {
            top: parent.top
            bottom: bottomResize.top
            right: parent.right
        }
        cursorShape:  Qt.SizeHorCursor

        onPressed: {
            previousX = mouseX
        }

        onMouseXChanged: {
            var dx = mouseX - previousX
            if (window.width + dx >= 1100)
               {window.setWidth(window.width + dx)}
        }
    }

    MouseArea {
       id: bottomResize
       height: 5
       anchors {
           bottom: parent.bottom
           left: parent.left
           right: parent.right
       }
       cursorShape: Qt.SizeVerCursor

       onPressed: {
           previousY = mouseY
       }

       onMouseYChanged: {
           var dy = mouseY - previousY
           if (window.height + dy >= 600)
              {window.setHeight(window.height + dy)}
       }
   }

    RowLayout {
        spacing: 0
        anchors.fill: parent

        ToolBar {
            id: sidebar
            Layout.fillHeight: true
            Layout.minimumWidth: 70
            Layout.maximumWidth: 70
            Layout.alignment: Qt.AlignLeft
            Material.background: Material.primary

            ButtonGroup {
                buttons: column_sidebar.children
            }

            ColumnLayout{
                id: column_sidebar
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 20

                NavigableMenuButton {
                    id: gamesBtn
                    Layout.topMargin: 30
                    Layout.minimumHeight: 80
                    Layout.minimumWidth: 80
                    anchors.horizontalCenter: parent.horizontalCenter
                    checkable: true
                    Material.foreground: "#C4B6B6"

                    text: "\ue903"
                    font.family: iconFont.name
                    font.pointSize: 28

                    onClicked: {
                        navigateTo("qrc:/gui/PcView.qml", "Computers");
                    }

                    Keys.onRightPressed: {
                        stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                    }
                }

                NavigableMenuButton {
                    id: settingsBtn
                    Layout.minimumHeight: 80
                    Layout.minimumWidth: 80
                    anchors.horizontalCenter: parent.horizontalCenter
                    checkable: true
                    Material.foreground: "#C4B6B6"

                    text: "\ue908"
                    font.family: iconFont.name
                    font.pointSize: 28

                    onClicked: {
                        navigateTo("qrc:/gui/SettingsView.qml", "Settings");
                    }

                    Keys.onRightPressed: {
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

                NavigableMenuButton {
                    //TODO: Implement a log view then unhide this button
                    id: logsBtn
                    Layout.minimumHeight: 80
                    Layout.minimumWidth: 80
                    anchors.horizontalCenter: parent.horizontalCenter
                    checkable: true
                    Material.foreground: "#C4B6B6"
                    visible: false
                    text: "\ue905"
                    font.family: iconFont.name
                    font.pointSize: 28

                    onClicked: {
                        navigateTo("qrc:/gui/LogView.qml", "Logs");
                    }

                    Keys.onRightPressed: {
                        stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                    }
                }

                NavigableMenuButton {
                    // TODO: Implement gamepad mapping then unhide this button
                    id: gamepadBtn
                    Layout.minimumHeight: 80
                    Layout.minimumWidth: 80
                    anchors.horizontalCenter: parent.horizontalCenter
                    checkable: false
                    visible: false
                    Material.foreground: "#C4B6B6"

                    text: "\ue90b"
                    font.family: iconFont.name
                    font.pointSize: 28

                    onClicked: {

                       navigateTo("qrc:/gui/GamepadMapper.qml", "Gamepad Mapping")

                    }

                    Keys.onRightPressed: {
                        stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                    }
                }


                NavigableMenuButton {
                    id: helpBtn
                    Layout.minimumHeight: 80
                    Layout.minimumWidth: 80
                    anchors.horizontalCenter: parent.horizontalCenter
                    checkable: false
                    visible: SystemProperties.hasBrowser
                    Material.foreground: "#C4B6B6"

                    text: "\ue904"
                    font.family: iconFont.name
                    font.pointSize: 28

                    ToolTip.delay: 1000
                    ToolTip.timeout: 3000
                    ToolTip.visible: hovered
                    ToolTip.text: "Help" + (helpShortcut.nativeText ? (" ("+helpShortcut.nativeText+")") : "")

                    Shortcut {
                        id: helpShortcut
                        sequence: StandardKey.HelpContents
                        onActivated: helpBtn.clicked()
                    }


                    // TODO need to make sure browser is brought to foreground.
                    onClicked: {

                       Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide");

                    }

                    Keys.onRightPressed: {
                        stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                    }
                }
            }

            NavigableMenuButton {
                property string browserUrl: ""

                id: updateBtn
                Layout.minimumHeight: 80
                Layout.minimumWidth: 80
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 10
                checkable: false
                visible: false
                Material.foreground: "#C4B6B6"

                text: "\ue909"
                font.family: iconFont.name
                font.pointSize: 28

                function updateAvailable(version, url)
                {
                    ToolTip.text = "Update available for Moonlight: Version " + version
                    updateButton.browserUrl = url
                    updateButton.visible = true
                }

                Component.onCompleted: {
                    AutoUpdateChecker.onUpdateAvailable.connect(updateAvailable)
                    AutoUpdateChecker.start()
                }

                onClicked: {

                   Qt.openUrlExternally(browserUrl);

                }

                Keys.onRightPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }
            }
        }

        StackView {
            id: stackView
            initialItem: initialView
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.rightMargin: 10
            Layout.bottomMargin: 5
            focus: true

            RoundButton {
                id: addPcButton
                visible: stackView.currentItem.objectName === "Computers"
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 60
                anchors.horizontalCenter: parent.horizontalCenter
                width: 70
                height: 70
                z: 5
                Material.foreground: Material.primary
                Material.background: Material.accent
                Material.elevation: 10

                text: "\ue900"
                font.family: iconFont.name
                font.pointSize: 30

                ToolTip.delay: 1000
                ToolTip.timeout: 3000
                ToolTip.visible: hovered
                ToolTip.text: "Add a new PC manually"

                Shortcut {
                    id: newPcShortcut
                    sequence: StandardKey.New
                    onActivated: addPcButton.clicked()
                    }

                onVisualFocusChanged: {
                    if (visualFocus)
                    {Material.background = "#ABA7A7"}
                    else if (!visualFocus)
                    {Material.background = Material.accent}
                }

                onClicked: {
                    addPcDialog.open();
                    Material.foreground = Material.primary
                    Material.background = Material.accent
                }

                onHoveredChanged: {
                    if (hovered)
                    {Material.background = "#ABA7A7"}
                    else if (!hovered)
                    {Material.background = Material.accent}
                }

                onPressedChanged: {
                    if (pressed)
                    {Material.background = "#E9E9E9"}
                    else if (!pressed)
                    {Material.background = Material.accent}
                    else if (!pressed && hovered)
                    {Material.background = "#ABA7A7"}
                }

                Keys.onUpPressed: {
                    stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                }

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

            pushEnter: Transition {
                    PropertyAnimation {
                        property: "opacity"
                        from: 0
                        to:1
                        duration: 200
                    }
                }
             pushExit: Transition {
                    PropertyAnimation {
                        property: "opacity"
                        from: 1
                        to:0
                        duration: 200
                    }
                }
              popEnter: Transition {
                    PropertyAnimation {
                        property: "opacity"
                        from: 0
                        to:1
                        duration: 200
                    }
                }
               popExit: Transition {
                    PropertyAnimation {
                        property: "opacity"
                        from: 1
                        to:0
                        duration: 200
                    }
                }

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

            if (SystemProperties.isRunningWayland) {
                waylandDialog.open()
            }
            else if (SystemProperties.isWow64) {
                wow64Dialog.open()
            }
            else if (!SystemProperties.hasHardwareAcceleration) {
                noHwDecoderDialog.open()
            }

            if (SystemProperties.unmappedGamepads) {
                unmappedGamepadDialog.unmappedGamepads = SystemProperties.unmappedGamepads
                unmappedGamepadDialog.open()
            }
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
        height: 64
        Material.primary: Material.accent

        MouseArea {
            anchors.fill: parent;


            onPressed: {
                previousX = mouseX
                previousY = mouseY
            }

            onPositionChanged: {
                var dx = mouseX - previousX
                var dy = mouseY - previousY
                var new_x = window.x + dx
                var new_y = window.y + dy
                if (new_y <= 0 && window.visibility !== Window.Maximized)
                    window.visibility = Window.Maximized
                else
                {
                    if (window.visibility === Window.Maximized)
                        window.visibility = Window.Windowed
                    window.x = new_x
                    window.y = new_y
                }
            }
        }

        RowLayout {
            anchors.fill: parent

            Image {
                id: logo_svg
                source: "qrc:/res/logo.svg"
                Layout.alignment: Qt.AlignLeft
                Layout.leftMargin: (sidebar.width - width)/2
                sourceSize.height: 45
                sourceSize.width: 45
                antialiasing: true


            }

            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                color: "transparent"

                Rectangle {
                    border.color: backBtn.activeFocus || backBtn.hovered ? "#F5F5F5" : "#242257"
                    border.width: 1
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    anchors.verticalCenter: parent.verticalCenter
                    color: "transparent"
                    radius: 24


                    height: backBtn.height * 0.8
                    width: backBtn.width * 0.8

                    NavigableToolButton {
                        id: backBtn
                        visible: stackView.depth > 1 && stackView.currentItem.objectName !== "Settings"
                        Material.foreground: "#F5F5F5"
                        anchors.centerIn: parent


                        text: "\ue901"
                        font.family: iconFont.name
                        font.pointSize: 10



                        onClicked: {
                            stackView.pop()

                        }
                        Keys.onDownPressed: {
                            stackView.currentItem.forceActiveFocus(Qt.TabFocus)
                        }
                    }
                }


                Image {
                    id:connectionIndicator
                    source: stackView.currentItem.objectName != "Computers" && stackView.currentItem.objectName != "Settings" ? "qrc:/res/dot-green.svg" : "qrc:/res/dot-red.svg"
                    sourceSize.height: 10
                    sourceSize.width: 10
                    width: 10
                    height: 10
                    smooth: true
                    antialiasing: true


                    anchors.top: connectionStatus.top
                    anchors.right: connectionStatus.left
                    anchors.rightMargin: 10
                    anchors.topMargin: 10
                }

                Label {
                    id: connectionStatus
                    text: stackView.currentItem.objectName != "Computers" && stackView.currentItem.objectName != "Settings" ? stackView.currentItem.objectName : "not connected"
                    font.pointSize: 15
                    color: "#F5F5F5"
                    anchors.verticalCenter: parent.verticalCenter
                    x: (parent.width + controls.width + 50 - width)/2
                }
            }

            Item {
                id: controls
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: 30

                Layout.fillHeight: true
                Layout.minimumWidth: 100
                Layout.maximumWidth: 200

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        border.color: minimizeAppBtn.activeFocus || minimizeAppBtn.hovered ? "#F5F5F5" : "#242257"
                        border.width: 1
                        color: "transparent"
                        radius: 24

                        height: minimizeAppBtn.height * 0.8
                        width: minimizeAppBtn.width * 0.8

                        NavigableToolButton {
                            id: minimizeAppBtn
                            Material.foreground: "#F5F5F5"
                            anchors.centerIn: parent

                            text: "\ue907"
                            font.family: iconFont.name
                            font.pointSize: 10

                            onClicked: {
                                showMinimized();
                            }
                        }
                    }

                    Rectangle {
                        border.color: resizeAppBtn.activeFocus || resizeAppBtn.hovered ? "#F5F5F5" : "#242257"
                        border.width: 1
                        color: "transparent"
                        radius: 24

                        height: resizeAppBtn.height * 0.8
                        width: resizeAppBtn.width * 0.8

                        NavigableToolButton {
                            id: resizeAppBtn
                            Material.foreground: "#F5F5F5"
                            anchors.centerIn: parent

                            text: window.visibility < 3 ? "\ue906" : "\ue90a"
                            font.family: iconFont.name
                            font.pointSize: 10

                            onClicked: {
                                window.visibility < 3 ? showMaximized() : showNormal()
                            }
                        }
                    }

                    Rectangle {
                        border.color: closeAppBtn.activeFocus || closeAppBtn.hovered ? "#F5F5F5" : "#242257"
                        border.width: 1
                        color: "transparent"
                        radius: 24

                        height: closeAppBtn.height * 0.8
                        width: closeAppBtn.width * 0.8

                        NavigableToolButton {
                            id: closeAppBtn
                            Material.foreground: "#F5F5F5"
                            anchors.centerIn: parent

                            text: "\ue902"
                            font.family: iconFont.name
                            font.pointSize: 10

                            onClicked: {
                                close()
                            }
                        }
                    }
                }
            }
        }
    }

    ErrorMessageDialog {
        id: noHwDecoderDialog
        text: "No functioning hardware accelerated H.264 video decoder was detected by Moonlight. " +
              "Your streaming performance may be severely degraded in this configuration."
        helpText: "Click the Help button for more information on solving this problem."
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems"
    }

    ErrorMessageDialog {
        id: waylandDialog
        text: "Moonlight does not support hardware acceleration on Wayland. Continuing on Wayland may result in poor streaming performance. " +
              "Please switch to an X session for optimal performance."
        helpText: "Click the Help button for more information."
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems"
    }

    NavigableMessageDialog {
        id: wow64Dialog
        standardButtons: Dialog.Ok | Dialog.Cancel
        text: "This PC is running a 64-bit version of Windows. Please download the x64 version of Moonlight for the best streaming performance."
        onAccepted: {
            Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-qt/releases");
        }
    }

    ErrorMessageDialog {
        id: unmappedGamepadDialog
        property string unmappedGamepads : ""
        text: "Moonlight detected gamepads without a mapping:\n" + unmappedGamepads
        helpTextSeparator: "\n\n"
        helpText: "Click the Help button for information on how to map your gamepads."
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Gamepad-Mapping"
    }

    // This dialog appears when quitting via keyboard or gamepad button
    NavigableMessageDialog {
        id: quitConfirmationDialog
        standardButtons: Dialog.Yes | Dialog.No
        text: "Are you sure you want to quit?"
        // For keyboard/gamepad navigation
        onAccepted: Qt.quit()
    }

    NavigableDialog {
        id: addPcDialog
        property string label: "Enter the IP address of your GameStream PC:"

        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {
            // Force keyboard focus on the textbox so keyboard navigation works
            editText.forceActiveFocus()
        }

        onAccepted: {
            if (editText.text) {
                ComputerManager.addNewHost(editText.text, false)
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
            }
        }
    }
}
