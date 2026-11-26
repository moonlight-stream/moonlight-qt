import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Controls.Material 2.2
import QtQuick.Window 2.2

import AppModel 1.0
import ComputerModel 1.0
import LocalHostSessions 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0
import SdlGamepadKeyNavigation 1.0

FocusScope {
    id: devices
    property ComputerModel computerModel: createModel()
    readonly property var device: pcList.currentItem
    readonly property string wallpaperHost: device && device.ready && !showingSettings &&
                                           StackView.status === StackView.Active && window.active ? device.hostUuid : ""
    readonly property bool compact: width < 620
    property bool showingSettings: false
    property bool pickerOpen: false
    property var pendingQuits: ({})
    readonly property var currentQuit: device ? pendingQuits[device.hostUuid] : null
    readonly property var cardTools: ["files", "view", "restart", "shutdown"]
    readonly property var allTools: ["files", "view", "wake", "restart", "shutdown"]
    property string toolResultHost: ""
    property string toolResultMessage: ""
    onCompactChanged: pickerOpen = false
    onWallpaperHostChanged: if (wallpaperHost) computerModel.refreshDesktopWallpaper(wallpaperHost)
    objectName: qsTr("My devices")
    Material.theme: Material.Light
    readonly property color actionBlue: "#2468d5"
    readonly property color actionTint: "#e3edfc"
    Material.accent: actionBlue
    Material.background: "#f7f9fc"
    focus: true

    Connections {
        target: ComputerManager
        function onQuitAppCompleted(error, hostUuid) {
            var request = devices.pendingQuits[hostUuid]
            if (!request) return
            delete devices.pendingQuits[hostUuid]
            devices.pendingQuitsChanged()
            if (error !== undefined && error !== null) {
                errorDialog.text = error
                errorDialog.helpText = ""
                errorDialog.open()
            } else if (request.targetId && device && device.hostUuid === hostUuid &&
                       !devices.showingSettings && quickGrid.model && !quickGrid.model.getRunningAppId()) {
                var session = quickGrid.model.createSessionForAppId(request.targetId)
                if (session) window.launchGuiSession(session)
            }
        }
    }

    Connections {
        target: computerModel
        function onToolActionResult(uuid, message, success) {
            if (!device || uuid !== device.hostUuid || !message) return
            if (success) {
                devices.toolResultHost = uuid
                devices.toolResultMessage = message
                clearResult.restart()
            } else {
                toolError.text = message
                toolError.open()
            }
        }
    }
    Timer {
        id: clearResult
        interval: 6000
        onTriggered: devices.toolResultMessage = ""
    }

    function toolLabel(tool) {
        switch (tool) {
        case "files": return qsTranslate("FileTransferWindow", "File transfer")
        case "view": return qsTr("View only")
        case "more": return qsTr("More tools")
        case "wake": return qsTr("Wake PC")
        case "restart": return qsTr("Restart")
        case "shutdown": return qsTr("Shut down")
        }
        return ""
    }

    function toolAvailable(tool) {
        if (!device) return false
        if (tool === "more") return true
        if (tool === "wake") return !device.online && !device.unknown && device.wakeable
        if (tool === "files")
            return Qt.platform.os === "windows" && device.ready && !device.powerPending
        if (!device.ready || !device.serverSupported || device.powerPending || currentQuit) return false
        if (tool === "view")
            return !device.opening && (!device.hasStreamWindow || device.viewOnly)
        return device.powerAvailable
    }

    function useTool(tool) {
        if (!toolAvailable(tool)) return
        if (tool === "more") {
            toolsDialog.hostUuid = device.hostUuid
            toolsDialog.open()
            return
        }
        var uuid = device.hostUuid
        var row = computerModel.indexOfUuid(uuid)
        if (row < 0) return
        toolsDialog.close()
        if (tool === "files") {
            computerModel.openFileTransfer(uuid)
        } else if (tool === "view") {
            var session = computerModel.createViewOnlySession(row)
            if (session) {
                window.launchGuiSession(session)
            } else {
                toolError.text = qsTr("The desktop is unavailable for viewing on %1.").arg(device.hostName)
                toolError.open()
            }
        } else if (tool === "wake") {
            computerModel.wakeComputer(row)
        } else {
            powerDialog.hostUuid = uuid
            powerDialog.hostName = device.hostName
            powerDialog.restart = tool === "restart"
            powerDialog.open()
        }
    }

    // Note: Any initialization done here that is critical for streaming must
    // also be done in CliStartStreamSegue.qml, since this code does not run
    // for command-line initiated streams.
    StackView.onActivated: {
        // Setup signals on CM
        ComputerManager.computerAddCompleted.connect(addComplete)
    }

    StackView.onDeactivating: {
        ComputerManager.computerAddCompleted.disconnect(addComplete)
    }

    function pairingComplete(error)
    {
        // Close the PIN dialog
        pairDialog.close()

        // Display a failed dialog if we got an error
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.helpText = ""
            errorDialog.open()
        }
    }

    function addComplete(success, detectedPortBlocking)
    {
        if (!success) {
            errorDialog.text = qsTr("Unable to connect to the specified PC.")

            if (detectedPortBlocking) {
                errorDialog.text += "\n\n" + qsTr("This PC's Internet connection is blocking Moonlight. Streaming over the Internet may not work while connected to this network.")
            }
            else {
                errorDialog.helpText = qsTr("Click the Help button for possible solutions.")
            }

            errorDialog.open()
        }
    }

    function selectDevice(index) {
        showingSettings = false
        pcList.currentIndex = index
        pickerOpen = false
    }

    function openSettings() {
        pickerOpen = false
        showingSettings = true
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', parent, '')
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(pairingComplete)
        model.connectionTestCompleted.connect(testConnectionDialog.connectionTestComplete)
        return model
    }

    Rectangle { anchors.fill: parent; color: "#f7f9fc" }

    LocalHostSessions {
        id: hostSessions
        onDisconnectFailed: {
            toolError.text = qsTr("Sunshine could not confirm the disconnect. Check the connected devices and try again.")
            toolError.open()
        }
    }
    Timer {
        running: Qt.platform.os === "windows" && window.visible && window.visibility !== Window.Minimized &&
                 (devices.visible || stackView.currentItem instanceof HostSessionsView)
        interval: 1000
        repeat: true
        triggeredOnStart: true
        onTriggered: hostSessions.refresh()
    }
    HostSessionBanner {
        id: incomingCard
        monitor: hostSessions
        z: 3
        visible: hostSessions.devices.length > 0
        width: devices.compact ? devices.width : rail.width
        height: visible ? implicitHeight : 0
        anchors { left: parent.left; top: parent.top }
        onToggled: {
            devices.pickerOpen = false
            stackView.push(Qt.resolvedUrl("HostSessionsView.qml"), {monitor: hostSessions})
        }
    }
    ItemDelegate {
        id: picker
        visible: devices.compact
        anchors { left: parent.left; top: incomingCard.bottom; margins: 6 }
        height: 36
        text: qsTr("Devices")
        onClicked: devices.pickerOpen = !devices.pickerOpen
    }
    MouseArea {
        anchors.fill: parent
        anchors.topMargin: picker.y + picker.height
        visible: devices.compact && devices.pickerOpen
        z: 1
        onClicked: devices.pickerOpen = false
    }
    Rectangle {
        id: rail
        visible: !devices.compact || devices.pickerOpen
        z: 2
        width: Math.min(220, devices.width)
        anchors.top: parent.top
        anchors.topMargin: devices.compact ? picker.y + picker.height + 4 : 0
        anchors.bottom: parent.bottom
        color: "#eff4f7"
        Keys.onEscapePressed: function(event) {
            if (devices.compact) {
                devices.pickerOpen = false
                picker.forceActiveFocus()
            } else event.accepted = false
        }

        Item {
            id: listTitle
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 6 }
            anchors.topMargin: 6 + (devices.compact ? 0 : incomingCard.height)
            height: 42
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 2
                spacing: 8
                Label {
                    id: myDevicesTitle
                    Layout.fillWidth: true
                    text: qsTr("My devices")
                    color: "#536278"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                ToolButton {
                    id: addDeviceButton
                    Layout.preferredWidth: 32; Layout.preferredHeight: 32
                    text: "+"
                    font.pixelSize: 21
                    Accessible.name: qsTranslate("main", "Add PC manually")
                    ToolbarToolTip {
                        text: addDeviceButton.Accessible.name +
                              (newPcShortcut.nativeText ? " (" + newPcShortcut.nativeText + ")" : "")
                    }
                    onClicked: {
                        devices.pickerOpen = false
                        addPcDialog.open()
                    }
                }
            }
        }
        ItemDelegate {
            id: railSettings
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 6 }
            height: 40
            text: qsTranslate("StreamControlCenter", "Control center")
            icon.source: "qrc:/res/settings.svg"
            icon.width: 18; icon.height: 18
            highlighted: devices.showingSettings
            background: Rectangle {
                radius: 6
                color: devices.showingSettings ? "#dce8fa" : railSettings.hovered || railSettings.visualFocus ? "#e5ecf5" : "transparent"
            }
            onClicked: devices.openSettings()
        }
        ListView {
            id: pcList
            readonly property int rowHeight: 42
            readonly property real rowsHeight: count * rowHeight + Math.max(0, count - 1) * spacing
            anchors { left: parent.left; right: parent.right; top: listTitle.bottom; bottom: railSettings.top; margins: 8 }
            topMargin: 4
            spacing: 3
            clip: true
            focus: true
            model: computerModel
            section.property: "paired"
            section.delegate: Item {
                width: pcList.width
                // Put spare space above the lower section. Long lists scroll as one.
                height: section === "false" ? Math.max(unpairedTitle.implicitHeight,
                            pcList.height - pcList.topMargin - pcList.rowsHeight) : 0
                Label {
                    id: unpairedTitle
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    visible: parent.height > 0
                    text: qsTr("Unpaired devices")
                    leftPadding: 10
                    topPadding: 12
                    bottomPadding: 6
                    color: myDevicesTitle.color
                    font: myDevicesTitle.font
                    elide: Text.ElideRight
                }
            }
            onCountChanged: if (count && currentIndex < 0) currentIndex = 0
            ScrollBar.vertical: AppScrollBar {}
            delegate: ItemDelegate {
                id: pcRow
                hoverEnabled: true
                width: pcList.width
                height: pcList.rowHeight
                readonly property bool selected: !devices.showingSettings && ListView.isCurrentItem
                property string hostName: model.name
                readonly property string hostUuid: model.uuid
                readonly property bool hasStreamWindow: model.streamWindowState !== ComputerModel.NoStreamWindow
                readonly property bool opening: model.streamWindowState === ComputerModel.OpeningStreamWindow
                readonly property bool viewOnly: model.viewOnlyStreamWindow
                readonly property bool wakeable: model.wakeable
                readonly property bool powerAvailable: model.hostPowerAvailable
                readonly property bool powerPending: model.hostPowerPending
                readonly property bool serverSupported: model.serverSupported
                readonly property string windowActionLabel: viewOnly ? qsTr("Show viewing window") : qsTr("Show remote control window")
                readonly property string streamDetail: model.streamProgressText || streamStatus
                readonly property string streamStatus: model.streamWindowState === ComputerModel.ConnectedStreamWindow
                                                       ? (viewOnly ? qsTr("Viewing") : qsTr("Remote control active")) : qsTr("Connecting…")
                property bool online: model.online
                property bool paired: model.paired
                property bool unknown: model.statusUnknown
                property url wallpaper: model.desktopWallpaper
                property alias quickApps: quickAppsLoader.item
                Loader {
                    id: quickAppsLoader
                    active: pcRow.ready && pcRow.ListView.isCurrentItem
                    sourceComponent: AppModel {
                        id: apps
                        Component.onCompleted: computerModel.initializeQuickApps(apps, index)
                    }
                }
                property alias pcContextMenu: pcContextMenuLoader.item
                readonly property bool ready: online && paired && !unknown
                readonly property bool needsPairing: online && !paired && !unknown
                readonly property color statusColor: ready ? "#21a66a" :
                                                     needsPairing || unknown ? "#b97923" : "#99a5b5"
                readonly property string statusText: unknown ? qsTr("Searching") : !online ? qsTr("Offline") :
                                                     paired ? qsTr("Online") : qsTr("Unpaired")
                leftPadding: 28
                rightPadding: rowStatus.visible ? rowStatus.implicitWidth + 20 : 12
                spacing: 10
                text: hostName
                font.weight: selected ? Font.DemiBold : Font.Normal
                icon.source: "qrc:/res/desktop_windows-48px.svg"
                icon.width: 18
                icon.height: 18
                icon.color: "#596b80"
                Material.foreground: selected ? "#182b46" : "#415168"
                Accessible.name: hostName
                Accessible.description: hasStreamWindow ? streamDetail : statusText
                TextMetrics {
                    id: nameMetrics
                    text: pcRow.hostName
                    font: pcRow.font
                }
                ToolbarToolTip {
                    id: nameTip
                    visible: pcRow.visible && (pcRow.hovered || pcRow.visualFocus) &&
                             !(pcRow.pcContextMenu && pcRow.pcContextMenu.visible) &&
                             nameMetrics.advanceWidth > pcRow.availableWidth - pcRow.icon.width - pcRow.spacing
                    text: pcRow.hostName
                    width: Math.min(implicitWidth, 300, devices.width - 16)
                    x: pcRow.width + 8
                    y: (pcRow.height - height) / 2
                    contentItem: Label {
                        text: nameTip.text
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.pixelSize: 13
                        color: "#20364e"
                    }
                }
                background: Rectangle {
                    radius: 7
                    color: pcRow.selected ? "#dce8fa" : pcRow.hovered ? "#e5ecf5" : "transparent"
                    border.width: pcRow.visualFocus ? 1 : 0
                    border.color: devices.actionBlue
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7
                    height: 7
                    radius: width / 2
                    color: pcRow.hasStreamWindow ? devices.actionBlue : pcRow.statusColor
                }
                Label {
                    id: rowStatus
                    visible: pcRow.hasStreamWindow || pcRow.needsPairing || pcRow.hovered || pcRow.visualFocus
                    anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                    text: pcRow.hasStreamWindow ? pcRow.streamStatus : pcRow.statusText
                    font.pixelSize: 11
                    color: pcRow.hasStreamWindow ? devices.actionBlue : pcRow.ready ? "#23694e" : pcRow.needsPairing ? "#825516" : "#526478"
                }
                onClicked: devices.selectDevice(index)
                onDoubleClicked: activateDevice()
                Keys.onReturnPressed: activateDevice()
                Keys.onEnterPressed: activateDevice()
                Loader {
                    id: pcContextMenuLoader
                    sourceComponent: NavigableMenu {
                        id: pcContextMenu
                        initiator: pcRow
                        Material.theme: Material.Light
                        Material.accent: devices.actionBlue
                        Material.foreground: "#20364e"
                        Material.background: "#ffffff"
                        width: Math.min(window.width, Math.max(200, contentWidth + 12))
                        padding: 5
                        onAboutToShow: {
                            requestPrivacy.checked = StreamingPreferences.privacyScreenForHost(pcRow.hostUuid)
                            createPrivacy.checked = StreamingPreferences.createPrivacyDisplayForHost(pcRow.hostUuid)
                        }
                        background: Rectangle {
                            radius: 8
                            color: "#ffffff"
                            border.color: "#c5d4e6"
                        }
                        MenuItem {
                            text: qsTr("PC Status: %1").arg(pcRow.statusText)
                            font.bold: true
                            contentItem: Label {
                                text: parent.text
                                color: "#526780"
                                verticalAlignment: Text.AlignVCenter
                            }
                            enabled: false
                        }
                        NavigableMenuItem {
                            text: qsTr("More tools")
                            onTriggered: {
                                pcList.currentIndex = index
                                devices.useTool("more")
                            }
                        }
                        NavigableMenuItem {
                            text: qsTr("Wake PC")
                            onTriggered: computerModel.wakeComputer(index)
                            visible: !model.online && model.wakeable
                        }
                        NavigableMenuItem {
                            id: requestPrivacy
                            visible: pcRow.paired
                            checkable: true
                            text: qsTr("Connect using privacy screen")
                            onTriggered: StreamingPreferences.setPrivacyScreenForHost(pcRow.hostUuid, !StreamingPreferences.privacyScreenForHost(pcRow.hostUuid))
                        }
                        NavigableMenuItem {
                            visible: pcRow.paired
                            text: qsTr("Privacy screen resolution…")
                            onTriggered: {
                                privacySizeDialog.hostUuid = pcRow.hostUuid
                                privacySizeField.text = StreamingPreferences.privacyDesktopSizeForHost(pcRow.hostUuid)
                                privacySizeDialog.open()
                            }
                        }
                        NavigableMenuItem {
                            id: createPrivacy
                            visible: pcRow.paired
                            checkable: true
                            text: qsTr("Allow virtual display creation while locked")
                            onTriggered: StreamingPreferences.setCreatePrivacyDisplayForHost(pcRow.hostUuid, !StreamingPreferences.createPrivacyDisplayForHost(pcRow.hostUuid))
                        }
                        NavigableMenuItem {
                            text: qsTr("Test Network")
                            onTriggered: {
                                computerModel.testConnectionForComputer(index)
                                testConnectionDialog.open()
                            }
                        }

                        NavigableMenuItem {
                            text: qsTr("Rename PC")
                            onTriggered: {
                                renamePcDialog.pcUuid = pcRow.hostUuid
                                renamePcDialog.originalName = model.name
                                renamePcDialog.open()
                            }
                        }
                        NavigableMenuItem {
                            text: qsTr("Remove device")
                            onTriggered: deletePcDialog.request(model.uuid, model.name)
                        }
                        NavigableMenuItem {
                            text: qsTr("View Details")
                            onTriggered: {
                                showPcDetailsDialog.pcDetails = model.details
                                showPcDetailsDialog.open()
                            }
                        }
                    }
                }

                function activateDevice() {
                    if (devices.pendingQuits[hostUuid]) return
                    if (ComputerManager.activateStreamWindow(hostUuid)) return
                    if (model.online) {
                        if (!model.serverSupported) {
                            errorDialog.text = qsTr("The version of GeForce Experience on %1 is not supported by this build of Moonlight. You must update Moonlight to stream from %1.").arg(model.name)
                            errorDialog.helpText = ""
                            errorDialog.open()
                        }
                        else if (model.paired) {
                            var session = computerModel.createQuickSession(index)
                            if (session) {
                                window.launchGuiSession(session)
                            } else {
                                errorDialog.text = qsTr("No desktop or direct-launch app is available on this device.")
                                errorDialog.helpText = ""
                                errorDialog.open()
                            }
                        }
                        else {
                            var pin = computerModel.generatePinString()

                            // Kick off pairing in the background
                            computerModel.pairComputer(index, pin)

                            // Display the pairing dialog
                            pairDialog.pin = pin
                            pairDialog.open()
                        }
                    } else if (!model.online) {
                        // Using open() here because it may be activated by keyboard
                        if (pcContextMenu) pcContextMenu.open()
                    }
                }

                onPressAndHold: {
                    // popup() ensures the menu appears under the mouse cursor
                    pcList.currentIndex = index
                    if (!pcContextMenu) return
                    if (pcContextMenu.popup) {
                        pcContextMenu.popup()
                    }
                    else {
                        // Qt 5.9 doesn't have popup()
                        if (pcContextMenu) pcContextMenu.open()
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton;
                    onClicked: {
                        parent.pressAndHold()
                    }
                }

                Keys.onMenuPressed: {
                    // We must use open() here so the menu is positioned on
                    // the ItemDelegate and not where the mouse cursor is
                    if (pcContextMenu) pcContextMenu.open()
                }

                Keys.onDeletePressed: deletePcDialog.request(model.uuid, model.name)
            }

        }
    }

    Rectangle {
        id: detailsPanel
        anchors.fill: parent
        anchors.leftMargin: devices.compact ? 0 : rail.width
        anchors.topMargin: devices.compact ? picker.y + picker.height + 4 : 0
        color: "#f8fafc"
        radius: 9
        border.color: "#e1e6eb"
    }

    Loader {
        anchors.fill: detailsPanel
        active: devices.showingSettings && devices.StackView.status !== StackView.Inactive
        focus: active
        sourceComponent: SettingsView {}
    }

    ColumnLayout {
        id: deviceSummary
        visible: !devices.showingSettings
        anchors {
            left: detailsPanel.left; right: detailsPanel.right; top: detailsPanel.top
            margins: devices.compact ? 14 : Math.min(40, devices.width * 0.04)
            topMargin: devices.compact ? 14 : 22
        }
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: devices.compact ? 10 : 16
            spacing: 8
            Rectangle {
                visible: !!device
                implicitWidth: status.width + 16
                implicitHeight: 23
                radius: height / 2
                color: "#20252b"
                Row {
                    id: status
                    anchors.centerIn: parent
                    spacing: 5
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 7; height: 7; radius: 4
                        color: device ? device.statusColor : "#99a5b5"
                    }
                    Label {
                        text: device ? device.statusText : ""
                        font.pixelSize: 12; color: "white"
                    }
                }
            }
            Label {
                Layout.maximumWidth: Math.max(0, deviceSummary.width - status.width - 68)
                text: device ? device.hostName : qsTr("My devices")
                font.pixelSize: devices.compact ? 20 : 24
                font.bold: true
                elide: Text.ElideRight
                color: "#182334"
            }
            OverflowButton {
                id: optionsButton
                visible: !!device
                Accessible.name: qsTr("Device options")
                onClicked: if (device.pcContextMenu) {
                    if (device.pcContextMenu.popup)
                        device.pcContextMenu.popup(optionsButton, 0, optionsButton.height)
                    else
                        device.pcContextMenu.open()
                }
            }

            Item { Layout.fillWidth: true }
        }
        Item {
            id: card
            readonly property int cornerRadius: 8
            readonly property bool hasStreamWindow: !!device && device.hasStreamWindow
            property real emphasis: connectButton.down ? 0.3 :
                                    connectButton.hovered || connectButton.visualFocus ? 1 : 0
            Behavior on emphasis { NumberAnimation { duration: 175; easing.type: Easing.OutCubic } }
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(100, Math.min(320, width * 0.4, detailsPanel.height * 0.48))
            clip: true

            Item {
                anchors.fill: parent
                scale: 1 + 0.035 * card.emphasis
                Image {
                    anchors.fill: parent
                    visible: !wallpaper.visible
                    source: "qrc:/res/remote-horizon.svg"
                    fillMode: Image.PreserveAspectCrop
                }
                Image {
                    id: wallpaper
                    anchors.fill: parent
                    visible: wallpaper.status === Image.Ready
                    source: device && device.wallpaper.toString() && preparedWallpaper.lastReady &&
                            preparedWallpaper.lastReady.host === device.hostUuid ? preparedWallpaper.lastReady.url : ""
                    sourceSize.width: 1600
                    fillMode: Image.PreserveAspectCrop
                }
                Image {
                    id: preparedWallpaper
                    property var lastReady: null
                    visible: false
                    asynchronous: true
                    sourceSize.width: 1600
                    fillMode: Image.PreserveAspectCrop
                    source: device ? device.wallpaper : ""
                    onStatusChanged: {
                        if (preparedWallpaper.status === Image.Ready && device)
                            lastReady = {host: device.hostUuid, url: preparedWallpaper.source}
                        else if (!preparedWallpaper.source.toString())
                            lastReady = null
                    }
                }
            }
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0; color: card.hasStreamWindow ? "#94071728" : "transparent" }
                    GradientStop { position: 0.45; color: card.hasStreamWindow ? "#aa071728" : "#12071728" }
                    GradientStop { position: 1; color: "#c0071728" }
                }
            }
            Row {
                visible: !card.hasStreamWindow
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Math.max(14, card.height * 0.14)
                spacing: 9
                Label {
                    text: connectButton.text
                    color: "white"; font.pixelSize: 18
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label {
                    text: "\u2192"
                    color: "white"; font.pixelSize: 21
                    transform: Translate { x: 5 * card.emphasis }
                    Accessible.ignored: true
                }
            }

            Column {
                visible: card.hasStreamWindow
                anchors.centerIn: parent
                width: Math.max(1, parent.width - 16)
                spacing: card.height >= 160 ? 12 : 6
                Image {
                    visible: card.height >= 160
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 42; height: 42
                    source: "qrc:/res/desktop_windows-48px.svg"
                }
                Label {
                    width: parent.width
                    text: device ? device.streamDetail : ""
                    textFormat: Text.PlainText
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    font.pixelSize: 18; font.weight: Font.DemiBold
                    color: "white"
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, showWindowLabel.implicitWidth + 32)
                    height: 36
                    radius: 6
                    color: connectButton.down ? "#194eaf" : connectButton.hovered ? "#3375e1" : devices.actionBlue
                    Label {
                        id: showWindowLabel
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: connectButton.text
                        color: "white"; font.pixelSize: 15; font.weight: Font.Medium
                    }
                }
            }

            AbstractButton {
                id: connectButton
                enabled: !devices.currentQuit
                anchors.fill: parent
                hoverEnabled: true
                text: card.hasStreamWindow ? device.windowActionLabel :
                      !device ? qsTr("Add device") : !device.online ? qsTr("Device options") :
                      !device.paired ? qsTr("Pair device") : qsTr("Enter desktop")
                Accessible.description: card.hasStreamWindow ? device.streamDetail : ""
                contentItem: Item {}
                background: Rectangle {
                    radius: card.cornerRadius
                    color: "transparent"
                    border.width: connectButton.visualFocus ? 2 : 0
                    border.color: "#9ac7ff"
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: Qt.PointingHandCursor
                }
                Keys.onReturnPressed: function(event) { if (!event.isAutoRepeat) clicked() }
                Keys.onEnterPressed: function(event) { if (!event.isAutoRepeat) clicked() }
                onClicked: device ? device.activateDevice() : addPcDialog.open()
            }

            // Mask just the corners to keep the scalable artwork inexpensive to resize.
            Repeater {
                model: device ? 2 : 4
                Canvas {
                    width: card.cornerRadius; height: width
                    x: index % 2 ? card.width - width : 0
                    y: index >= 2 ? card.height - height : 0
                    rotation: [0, 90, 270, 180][index]
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.fillStyle = detailsPanel.color
                        ctx.beginPath()
                        ctx.moveTo(0, 0)
                        ctx.lineTo(width, 0)
                        ctx.arc(width, height, width, -Math.PI / 2, -Math.PI, true)
                        ctx.closePath()
                        ctx.fill()
                    }
                }
            }
        }
        Item {
            id: toolsBar
            readonly property int toolColumns: width < 480 ? 2 : 4
            visible: !!device
            Layout.fillWidth: true
            Layout.preferredHeight: toolColumns === 2 ? 84 : 47
            Item {
                anchors.fill: parent
                clip: true
                // Crop the upper corners at the seam with the artwork.
                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: -radius
                    radius: card.cornerRadius
                    color: "#ffffff"
                    border.color: "#d7e0eb"
                }
            }
            GridLayout {
                anchors.fill: parent
                anchors.margins: 4
                columns: toolsBar.toolColumns
                columnSpacing: 2
                rowSpacing: 2
                Repeater {
                    model: devices.cardTools
                    DeviceToolButton {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 120
                        Layout.minimumWidth: 30
                        Layout.fillHeight: true
                        text: devices.toolLabel(modelData)
                        iconSource: "qrc:/res/device-" + modelData + ".svg"
                        enabled: devices.toolAvailable(modelData)
                        onClicked: devices.useTool(modelData)
                        Rectangle {
                            visible: index % toolsBar.toolColumns !== 0
                            anchors { left: parent.left; leftMargin: -1; verticalCenter: parent.verticalCenter }
                            width: 1; height: 18
                            color: "#d7e0eb"
                        }
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 6
            visible: !!device && devices.toolResultHost === device.hostUuid && devices.toolResultMessage !== ""
            text: devices.toolResultMessage
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: "#226c50"
        }
        Label {
            visible: !device
            Layout.fillWidth: true
            text: StreamingPreferences.enableMdns ? qsTr("Searching for compatible hosts on your local network...")
                                                  : qsTr("Automatic PC discovery is disabled. Add your PC manually.")
            wrapMode: Text.Wrap
            color: "#52647a"
        }
    }

    ColumnLayout {
        anchors {
            left: deviceSummary.left; right: deviceSummary.right
            top: deviceSummary.bottom; bottom: detailsPanel.bottom
            topMargin: devices.compact ? 12 : 20; bottomMargin: 8
        }
        visible: !devices.showingSettings && !!device && device.ready
        spacing: 10
        Label {
            Layout.fillWidth: true
            visible: quickGrid.count > 0 || (!!quickGrid.model && quickGrid.model.hasHiddenApps)
            text: qsTr("Quick launch")
            color: "#1b2c42"
            font.pixelSize: 17
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        GridView {
            id: quickGrid
            readonly property int tileSpacing: 10
            readonly property int columnCount: Math.max(1, Math.min(count, Math.floor(width / 280)))
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.rightMargin: -tileSpacing
            clip: true
            cellWidth: Math.max(1, Math.min(400, width / columnCount))
            cellHeight: 84
            model: device ? device.quickApps : null
            ScrollBar.vertical: AppScrollBar {}
            footer: CheckBox {
                visible: !!quickGrid.model && quickGrid.model.hasHiddenApps
                height: visible ? implicitHeight : 0
                text: qsTr("Show hidden apps")
                checked: !!quickGrid.model && quickGrid.model.showHiddenApps
                onToggled: quickGrid.model.showHiddenApps = checked
            }
            delegate: AbstractButton {
                id: shortcut
                readonly property bool stopping: !!devices.currentQuit && devices.currentQuit.runningId === model.appid
                opacity: model.hidden ? 0.55 : 1
                width: Math.max(1, quickGrid.cellWidth - quickGrid.tileSpacing)
                height: quickGrid.cellHeight - quickGrid.tileSpacing
                hoverEnabled: true
                text: model.name
                padding: 0
                background: Rectangle {
                    radius: 11
                    color: shortcut.down ? devices.actionTint : shortcut.hovered ? "#f3f7ff" : "white"
                    border.width: shortcut.visualFocus ? 2 : 1
                    border.color: shortcut.visualFocus ? devices.actionBlue : shortcut.hovered ? "#a9c4ed" : "#dce4ec"
                }
                contentItem: RowLayout {
                    spacing: 10
                    Image {
                        visible: !model.running || shortcut.width >= 235
                        Layout.leftMargin: 14
                        Layout.preferredWidth: 43; Layout.preferredHeight: 54
                        source: model.boxart; asynchronous: true
                        fillMode: Image.PreserveAspectFit
                    }
                    Label {
                        id: shortcutName
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        Layout.leftMargin: model.running && shortcut.width < 235 ? 10 : 0
                        text: shortcut.text
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        color: "#24364b"
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        spacing: 0
                        Layout.rightMargin: 6
                        SessionActionButton {
                            id: resumeButton
                            visible: model.running
                            enabled: !shortcut.stopping
                            text: device && device.hasStreamWindow ? device.windowActionLabel :
                                  model.isDesktop ? qsTranslate("AppView", "Reconnect to Desktop") : qsTranslate("AppView", "Resume Game")
                            icon.source: "qrc:/res/play_arrow_FILL1_wght700_GRAD200_opsz48.svg"
                            icon.color: "#2468d5"
                            onClicked: shortcut.launch()
                        }
                        SessionActionButton {
                            id: stopButton
                            visible: model.running && !shortcut.stopping
                            text: model.isDesktop ? qsTranslate("AppView", "End Desktop Session") : qsTranslate("AppView", "Quit Game")
                            icon.source: "qrc:/res/stop_FILL1_wght700_GRAD200_opsz48.svg"
                            icon.color: "#b84240"
                            onClicked: switchApp.request(quickGrid.model, model.appid, 0)
                        }
                        BusyIndicator {
                            visible: shortcut.stopping
                            running: visible
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            Accessible.name: qsTranslate("QuitSegue", "Quitting %1...").arg(shortcut.text)
                        }
                        OverflowButton {
                            id: appOptions
                            Accessible.name: qsTr("Options for %1").arg(shortcut.text)
                            onClicked: {
                                if (appMenu.popup)
                                    appMenu.popup(appOptions, appOptions.width - appMenu.width, appOptions.height)
                                else
                                    appMenu.open()
                            }
                        }
                    }
                }
                ToolbarToolTip {
                    visible: shortcut.visible && shortcutName.truncated && !appMenu.visible &&
                             !appOptions.hovered && !resumeButton.hovered && !stopButton.hovered &&
                             (shortcut.hovered || shortcut.visualFocus)
                    text: shortcut.text
                    width: Math.min(implicitWidth, shortcut.width)
                }

                function launch() {
                    if (devices.currentQuit) return
                    if (ComputerManager.activateStreamWindow(device.hostUuid)) return
                    var apps = quickGrid.model
                    var running = apps.getRunningAppId()
                    if (running && running !== model.appid) {
                        switchApp.request(apps, running, model.appid)
                    } else {
                        window.launchGuiSession(apps.createSessionForApp(index))
                    }
                }
                onClicked: launch()
                onPressAndHold: appMenu.popup ? appMenu.popup() : appMenu.open()
                Keys.onMenuPressed: appMenu.open()
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier)) {
                        appMenu.open()
                        event.accepted = true
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: shortcut.pressAndHold()
                }
                NavigableMenu {
                    id: appMenu
                    initiator: appOptions.activeFocus ? appOptions : shortcut
                    margins: 8
                    width: Math.min(window.width - 2 * margins, Math.max(200, contentWidth + leftPadding + rightPadding))
                    padding: 5
                    background: Rectangle {
                        radius: 8
                        color: "white"
                        border.color: "#c5d4e6"
                    }
                    NavigableMenuItem {
                        visible: !model.running
                        text: device && device.hasStreamWindow ? device.windowActionLabel : model.isDesktop
                              ? qsTranslate("AppView", "Connect to Desktop") : qsTranslate("AppView", "Launch Game")
                        onTriggered: shortcut.launch()
                    }
                    NavigableMenuItem {
                        checkable: true
                        checked: model.directLaunch
                        enabled: !model.hidden
                        text: qsTranslate("AppView", "Direct Launch")
                        onTriggered: quickGrid.model.setAppDirectLaunch(index, !model.directLaunch)
                    }
                    NavigableMenuItem {
                        checkable: true
                        checked: model.hidden
                        enabled: model.hidden || (!model.running && !model.directLaunch)
                        text: qsTranslate("AppView", "Hide Game")
                        onTriggered: quickGrid.model.setAppHidden(index, !model.hidden)
                    }
                }
            }
        }
    }

    NavigableDialog {
        id: toolsDialog
        property string hostUuid
        readonly property bool currentHost: !!device && device.hostUuid === hostUuid
        onCurrentHostChanged: if (visible && !currentHost) close()
        title: qsTr("More tools")
        Material.theme: Material.Light
        Material.accent: devices.actionBlue
        width: Math.min(420, devices.width - 16)
        height: Math.min(390, devices.height - 12)
        padding: 16
        standardButtons: Dialog.Close
        background: Rectangle { color: "#ffffff"; radius: 10; border.color: "#d7e0eb" }
        header: Label {
            text: toolsDialog.title
            leftPadding: 18; topPadding: 16; bottomPadding: 8
            color: "#172437"; font.pixelSize: 20; font.weight: Font.DemiBold
        }
        contentItem: Flickable {
            id: toolScroller
            clip: true
            contentWidth: width
            contentHeight: toolsList.height
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: AppScrollBar {}
            Column {
                id: toolsList
                width: toolScroller.width
                spacing: 5
                Repeater {
                    id: toolItems
                    model: devices.allTools
                    DeviceToolButton {
                        width: toolsList.width
                        height: 44
                        listItem: true
                        text: devices.toolLabel(modelData)
                        iconSource: "qrc:/res/device-" + modelData + ".svg"
                        enabled: toolsDialog.currentHost && devices.toolAvailable(modelData)
                        onClicked: devices.useTool(modelData)
                        onActiveFocusChanged: {
                            if (!activeFocus) return
                            if (y < toolScroller.contentY)
                                toolScroller.contentY = y
                            else if (y + height > toolScroller.contentY + toolScroller.height)
                                toolScroller.contentY = y + height - toolScroller.height
                        }
                    }
                }
            }
        }
        onOpened: {
            toolScroller.contentY = 0
            for (var i = 0; i < toolItems.count; i++) {
                if (toolItems.itemAt(i).enabled) {
                    toolItems.itemAt(i).forceActiveFocus(Qt.TabFocusReason)
                    return
                }
            }
            standardButton(Dialog.Close).forceActiveFocus(Qt.TabFocusReason)
        }
    }

    NavigableDialog {
        id: powerDialog
        property string hostUuid
        property string hostName
        property bool restart
        Material.theme: Material.Light
        Material.accent: devices.actionBlue
        width: Math.min(395, devices.width - 16)
        padding: 20
        title: restart ? qsTr("Restart") : qsTr("Shut down")
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { color: "#ffffff"; radius: 10; border.color: "#d7e0eb" }
        contentItem: ColumnLayout {
            spacing: 16
            Label {
                Layout.fillWidth: true
                text: powerDialog.restart
                      ? qsTr("Restart %1? Anyone using this device could lose unsaved work.").arg(powerDialog.hostName)
                      : qsTr("Shut down %1? Anyone using this device could lose unsaved work.").arg(powerDialog.hostName)
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: "#26394f"
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button {
                    id: cancelPower
                    text: qsTr("Cancel")
                    flat: true
                    onClicked: powerDialog.reject()
                }
                Button {
                    text: powerDialog.title
                    highlighted: true
                    Material.accent: "#a33539"
                    enabled: !!device && device.hostUuid === powerDialog.hostUuid &&
                             devices.toolAvailable(powerDialog.restart ? "restart" : "shutdown")
                    onClicked: powerDialog.accept()
                }
            }
        }
        onOpened: cancelPower.forceActiveFocus(Qt.TabFocusReason)
        onAccepted: {
            if (device && device.hostUuid === hostUuid && devices.toolAvailable(restart ? "restart" : "shutdown"))
                computerModel.requestHostPower(hostUuid, restart)
        }
    }

    NavigableMessageDialog {
        id: toolError
        standardButtons: Dialog.Ok
    }

    NavigableMessageDialog {
        id: switchApp
        property var apps
        property int runningId
        property int targetId
        standardButtons: Dialog.Yes | Dialog.No
        function request(list, running, target) {
            if (devices.currentQuit) return
            apps = list
            runningId = running
            targetId = target
            text = qsTranslate("AppView", "Are you sure you want to quit %1? Any unsaved progress will be lost.").arg(apps.getRunningAppName())
            open()
        }
        onAccepted: {
            if (devices.currentQuit || !device || !apps || apps !== quickGrid.model || apps.getRunningAppId() !== runningId) return
            if (targetId && ComputerManager.activateStreamWindow(device.hostUuid)) return
            devices.pendingQuits[device.hostUuid] = { runningId: runningId, targetId: targetId }
            devices.pendingQuitsChanged()
            apps.quitRunningApp()
        }
    }

    ErrorMessageDialog {
        id: errorDialog

        // Using Setup-Guide here instead of Troubleshooting because it's likely that users
        // will arrive here by forgetting to enable GameStream or not forwarding ports.
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide"
    }

    NavigableMessageDialog {
        id: pairDialog
        closePolicy: Popup.CloseOnEscape

        // don't allow edits to the rest of the window while open
        property string pin : "0000"
        text:qsTr("Please enter %1 on your host PC. This dialog will close when pairing is completed.").arg(pin)+"\n\n"+
             qsTr("If your host PC is running Sunshine, navigate to the Sunshine web UI to enter the PIN.")
        standardButtons: Dialog.Cancel
        onRejected: {
            // FIXME: We should interrupt pairing here
        }
    }

    NavigableDialog {
        id: deletePcDialog
        property string pcUuid
        property string pcName
        title: qsTr("Remove device")
        width: Math.min(392, devices.width - 24)
        padding: 22
        closePolicy: Popup.CloseOnEscape
        Material.theme: Material.Light
        function request(uuid, name) {
            pcUuid = uuid
            pcName = name
            open()
        }
        onOpened: cancelRemoval.forceActiveFocus(Qt.TabFocusReason)
        onAccepted: {
            var targetRow = computerModel.indexOfUuid(pcUuid)
            if (targetRow >= 0)
                computerModel.deleteComputer(targetRow)
        }
        background: Rectangle {
            radius: 14
            color: "#ffffff"
            border.color: "#dce3ec"
        }
        header: Label {
            text: deletePcDialog.title
            leftPadding: 22; rightPadding: 22; topPadding: 20
            color: "#172437"; font.pixelSize: 20; font.weight: Font.DemiBold
        }
        contentItem: ColumnLayout {
            spacing: 18
            Label {
                Layout.fillWidth: true
                text: qsTr("Remove “%1” from your device list?").arg(deletePcDialog.pcName)
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: "#43536a"; font.pixelSize: 15
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8
                Button {
                    id: cancelRemoval
                    text: qsTr("Cancel")
                    flat: true
                    Keys.onReturnPressed: deletePcDialog.reject()
                    Keys.onEnterPressed: deletePcDialog.reject()
                    KeyNavigation.right: confirmRemoval
                    onClicked: deletePcDialog.reject()
                }
                Button {
                    id: confirmRemoval
                    text: qsTr("Remove device")
                    highlighted: true
                    Material.accent: "#aa3540"
                    Keys.onReturnPressed: deletePcDialog.accept()
                    Keys.onEnterPressed: deletePcDialog.accept()
                    KeyNavigation.left: cancelRemoval
                    onClicked: deletePcDialog.accept()
                }
            }
        }
    }

    NavigableMessageDialog {
        id: testConnectionDialog
        closePolicy: Popup.CloseOnEscape
        standardButtons: Dialog.Ok

        onAboutToShow: {
            testConnectionDialog.text = qsTr("Moonlight is testing your network connection to determine if any required ports are blocked.") + "\n\n" + qsTr("This may take a few seconds…")
            showSpinner = true
        }

        function connectionTestComplete(result, blockedPorts)
        {
            if (result === -1) {
                text = qsTr("The network test could not be performed because none of Moonlight's connection testing servers were reachable from this PC. Check your Internet connection or try again later.")
                imageSrc = "qrc:/res/baseline-warning-24px.svg"
            }
            else if (result === 0) {
                text = qsTr("This network does not appear to be blocking Moonlight. If you still have trouble connecting, check your PC's firewall settings.") + "\n\n" + qsTr("If you are trying to stream over the Internet, install the Moonlight Internet Hosting Tool on your gaming PC and run the included Internet Streaming Tester to check your gaming PC's Internet connection.")
                imageSrc = "qrc:/res/baseline-check_circle_outline-24px.svg"
            }
            else {
                text = qsTr("Your PC's current network connection seems to be blocking Moonlight. Streaming over the Internet may not work while connected to this network.") + "\n\n" + qsTr("The following network ports were blocked:") + "\n"
                text += blockedPorts
                imageSrc = "qrc:/res/baseline-error_outline-24px.svg"
            }

            // Stop showing the spinner and show the image instead
            showSpinner = false
        }
    }

    NavigableDialog {
        id: privacySizeDialog
        property string hostUuid
        title: qsTr("Privacy screen resolution")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            standardButton(Dialog.Ok).enabled = privacySizeField.valid
            privacySizeField.forceActiveFocus()
            privacySizeField.selectAll()
        }
        onAccepted: StreamingPreferences.setPrivacyDesktopSizeForHost(hostUuid, privacySizeField.text.trim())
        ColumnLayout {
            Label { text: qsTr("Size for the next private connection. Leave blank to use Sunshine settings.") }
            TextField {
                id: privacySizeField
                placeholderText: "2560x1440"
                Layout.fillWidth: true
                readonly property var dimensions: text.trim().split("x")
                readonly property bool valid: !text.trim() ||
                    (/^[0-9]{3,4}x[0-9]{3,4}$/.test(text.trim()) &&
                     Number(dimensions[0]) >= 320 && Number(dimensions[0]) <= 8192 &&
                     Number(dimensions[1]) >= 200 && Number(dimensions[1]) <= 8192)
                onValidChanged: if (privacySizeDialog.opened) privacySizeDialog.standardButton(Dialog.Ok).enabled = valid
                onAccepted: if (valid) privacySizeDialog.accept()
            }
        }
    }

    NavigableDialog {
        id: renamePcDialog
        property string label: qsTr("Enter the new name for this PC:")
        property string originalName
        property string pcUuid

        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {
            // Force keyboard focus on the textbox so keyboard navigation works
            editText.forceActiveFocus()
        }

        onClosed: {
            editText.clear()
        }

        onAccepted: {
            var row = computerModel.indexOfUuid(pcUuid)
            if (editText.text && row >= 0) {
                computerModel.renameComputer(row, editText.text)
            }
        }

        ColumnLayout {
            Label {
                text: renamePcDialog.label
                font.bold: true
            }

            TextField {
                id: editText
                placeholderText: renamePcDialog.originalName
                Layout.fillWidth: true
                focus: true

                Keys.onReturnPressed: {
                    renamePcDialog.accept()
                }

                Keys.onEnterPressed: {
                    renamePcDialog.accept()
                }
            }
        }
    }

    NavigableMessageDialog {
        id: showPcDetailsDialog
        property string pcDetails : "";
        text: showPcDetailsDialog.pcDetails
        imageSrc: "qrc:/res/baseline-help_outline-24px.svg"
        standardButtons: Dialog.Ok
    }

}
