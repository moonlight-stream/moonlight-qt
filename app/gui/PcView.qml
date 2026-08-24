import QtQuick 2.9
import QtQuick.Controls
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import Qt.labs.platform 1.1
import QtCore

import ComputerModel 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0
import ImageUtils 1.0

import "theme"
import "Brand.js" as Brand

CenteredGridView {
    // 这一页自带壁纸，main.qml 不用再垫一层
    readonly property bool usesOwnBackground: true
    property ComputerModel computerModel : createModel()
    readonly property string currentBgUrl: backgroundImage.currentImageUrl

    function reloadBackgroundFromPreferences(forceRefresh) {
        backgroundImage.reloadFromPreferences(forceRefresh === true)
    }

    function applyLocalBackgroundImage(fileUrl) {
        var validationError = imageUtils.validateLocalBackgroundImage(fileUrl)
        if (validationError !== "") {
            errorDialog.text = validationError
            errorDialog.open()
            return false
        }

        StreamingPreferences.backgroundImageLocalPath = fileUrl
        StreamingPreferences.save()
        return true
    }

    // 壁纸由这一页负责抓取和刷新，但整个窗口都要用，所以每次变化都同步给 ApplicationWindow。
    // 这样离开这一页之后（连接进度页、退出页、设置页）背景不会突然变成一块纯色。
    onCurrentBgUrlChanged: {
        if (Window.window) {
            Window.window.backgroundImageUrl = currentBgUrl
        }
    }

    id: pcGrid
    focus: true
    activeFocusOnTab: true
    topMargin: 72   // 工具栏 56 + 一格间距
    bottomMargin: 5
    cellWidth: 240; cellHeight: 280;
    objectName: qsTr("Computers")

    Component.onCompleted: {
        // Don't show any highlighted item until interacting with them.
        // We do this here instead of onActivated to avoid losing the user's
        // selection when backing out of a different page of the app.
        currentIndex = -1
    }

    // Note: Any initialization done here that is critical for streaming must
    // also be done in CliStartStreamSegue.qml, since this code does not run
    // for command-line initiated streams.
    StackView.onActivated: {
        // Setup signals on CM
        ComputerManager.computerAddCompleted.connect(addComplete)

        // Highlight the first item if a gamepad is connected
        if (currentIndex === -1 && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            currentIndex = 0
        }

        backgroundImage.reloadFromPreferences(false)
    }

    Connections {
        target: StreamingPreferences
        function onBackgroundConfigurationChanged() {
            pcGrid.reloadBackgroundFromPreferences(true)
        }
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
                errorDialog.text += "\n\n" + Brand.text(qsTr("This PC's Internet connection is blocking Moonlight. Streaming over the Internet may not work while connected to this network."))
            }
            else {
                errorDialog.helpText = qsTr("Click the Help button for possible solutions.")
            }

            errorDialog.open()
        }
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', parent, '')
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(pairingComplete)
        model.connectionTestCompleted.connect(testConnectionDialog.connectionTestComplete)
        return model
    }

    function openAppView(computerIndex, computerName, showHiddenGames)
    {
        // 造不出来时 createObject 返回 null，push(null) 只会往日志里丢一句
        // 「nothing to push」就完了 —— 界面上表现为「点了没反应」，非常难查。
        // status 和 createObject 的返回值都要看：status 只说明文件加载成功了，
        // 实例化本身还可能失败（属性赋值出错等）。
        function fail(reason) {
            console.error("Failed to open AppView.qml: " + reason)
            errorDialog.text = qsTr("Unable to open the app list for %1.").arg(computerName)
            errorDialog.helpText = reason
            errorDialog.open()
        }

        var component = Qt.createComponent("AppView.qml")
        if (component.status !== Component.Ready) {
            fail(component.errorString())
            return
        }

        var properties = {"computerIndex": computerIndex, "objectName": computerName}
        if (showHiddenGames === true) {
            properties.showHiddenGames = true
        }

        var appView = component.createObject(stackView, properties)
        if (!appView) {
            fail(component.errorString())
            return
        }

        stackView.push(appView)
    }

    function showAddressSelectionForComputer(computerIndex, computerName, openAppAfterSelection)
    {
        var addresses = computerModel.getConnectionAddressesForComputer(computerIndex)

        // 列表里第一项是「自动」这个伪条目，判断有没有可选地址得数真地址。
        var realAddressCount = 0
        for (var i = 0; i < addresses.length; i++) {
            if (!addresses[i].isAuto) {
                realAddressCount++
            }
        }

        if (realAddressCount === 0) {
            errorDialog.text = qsTr("No connection IP addresses are available for %1.").arg(computerName)
            errorDialog.helpText = ""
            errorDialog.open()
            return
        }

        if (realAddressCount === 1) {
            if (openAppAfterSelection === true) {
                openAppView(computerIndex, computerName, false)
            }
            return
        }

        // 预选交给 SelectAddressDialog 自己按 isActive 算
        selectAddressDialog.pcIndex = computerIndex
        selectAddressDialog.pcName = computerName
        selectAddressDialog.openAppAfterSelection = openAppAfterSelection === true
        selectAddressDialog.addresses = addresses
        selectAddressDialog.promptText = qsTr("Choose the IP address to connect to %1:").arg(computerName)
        selectAddressDialog.open()
    }

    // 搜索状态：Manrope 800 大标题 + DM Mono 说明行 + 斜条纹读条，
    // 都咬着同一条左基线。
    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spaceXl * 2, 560)
        spacing: Theme.spaceMd
        visible: pcGrid.count === 0

        Text {
            width: parent.width
            text: StreamingPreferences.enableMdns ? qsTr("Searching") : qsTr("No Computers")
            color: Theme.text
            font.family: Theme.fontSans
            font.pointSize: 26
            font.weight: Font.ExtraBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: Theme.trackingTight(26)
            // 标题和说明都咬着读条的左基线，不居中 —— 和加载页、退出页一致
            horizontalAlignment: Text.AlignLeft
        }

        // 关掉 mDNS 时读条停在暗态：这里本来就没有在扫描，一台没通电的仪表比
        // 一条空轨道更说明问题。
        HardProgress {
            width: parent.width
            running: StreamingPreferences.enableMdns
        }

        Text {
            width: parent.width
            text: StreamingPreferences.enableMdns ? qsTr("Searching for compatible hosts on your local network...")
                                                  : qsTr("Automatic PC discovery is disabled. Add your PC manually.")
            color: Theme.textDim
            font.family: Theme.fontMono
            font.pointSize: Theme.fontBody
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.Wrap
        }
    }

    model: computerModel

    // 这一项刻意不跟着换成 Panel 硬卡片：月球头像是这一页的创意主体，
    // 一旦套上方角卡片和硬投影，月球就从「浮在壁纸上的天体」变成「贴纸」，
    // 而且卡片自带的 hover 高亮块会和头像抢注意力。样式和交互都按改造前保留。
    delegate: NavigableItemDelegate {
        width: 240; height: 240;
        grid: pcGrid

        property alias pcContextMenu : pcContextMenuLoader.item

        // 右键菜单是异步 Loader 造的，刚进视野的条目上 item 还是 null。四个调用点
        // 以前都直接用，读 null 的成员会抛 TypeError，这一次点击就被静默吃掉 ——
        // 表现正是「点了没反应，再点一次才出来」。这里记下意图，等造好再开。
        // 0 = 没有待处理，1 = open()，2 = popup()（跟着鼠标位置）
        property int pendingMenuRequest: 0

        function openContextMenu(atCursor) {
            if (!pcContextMenuLoader.item) {
                pendingMenuRequest = atCursor ? 2 : 1
                return
            }

            pendingMenuRequest = 0
            if (atCursor && pcContextMenuLoader.item.popup) {
                pcContextMenuLoader.item.popup()
            }
            else {
                // Qt 5.9 没有 popup()；键盘触发时也走这条，菜单落在条目上而不是光标处
                pcContextMenuLoader.item.open()
            }
        }

        Rectangle {
            id: pcIcon
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 10
            width: 160
            height: 160
            radius: width / 2
            color: {
                // 根据名称生成固定颜色，确保同一台PC总是相同颜色
                var hash = 0;
                for (var i = 0; i < model.name.length; i++) {
                    hash = model.name.charCodeAt(i) + ((hash << 5) - hash);
                }
                var color = '#';
                for (var j = 0; j < 3; j++) {
                    var value = (hash >> (j * 8)) & 0xFF;
                    color += ('00' + value.toString(16)).substr(-2);
                }
                return color;
            }

            Image {
                id: moonMask
                anchors.fill: parent
                source: "qrc:/res/moon-mask.png"
                opacity: 0.7
                fillMode: Image.PreserveAspectFit

                // 根据PC名称生成旋转角度
                property real rotationAngle: {
                    var hash = 0;
                    for (var i = 0; i < model.name.length; i++) {
                        hash = model.name.charCodeAt(i) + ((hash << 5) - hash);
                    }
                    return (hash % 180);
                }

                rotation: rotationAngle
            }

            Text {
                anchors.centerIn: parent
                text: model.name ? model.name.charAt(0).toUpperCase() : "?"
                font.pixelSize: parent.width * 0.6
                font.bold: true
                color: parent.color
            }
        }

        Image {
            // TODO: Tooltip
            id: stateIcon
            anchors {
                right: pcIcon.right
                bottom: pcIcon.bottom
                rightMargin: 5
                bottomMargin: 5
            }
            visible: !model.statusUnknown && (!model.online || !model.paired)
            source: !model.online ? "qrc:/res/warning_FILL1_wght300_GRAD200_opsz24.svg" : "qrc:/res/baseline-lock-24px.svg"
            sourceSize {
                width: !model.online ? 32 : 28
                height: !model.online ? 32 : 28
            }
            opacity: 0.8
        }

        Rectangle {
            id: statusUnknownSpinner
            anchors.horizontalCenter: pcIcon.horizontalCenter
            anchors.verticalCenter: pcIcon.verticalCenter
            anchors.verticalCenterOffset: 0
            width: 160
            height: 160
            color: "transparent"
            visible: model.statusUnknown

            Image {
                id: spinnerImage
                anchors.centerIn: parent
                width: 160
                height: 160
                source: "qrc:/res/loading.svg"

                RotationAnimation {
                    target: spinnerImage
                    property: "rotation"
                    from: 0
                    to: 360
                    duration: 1500
                    loops: Animation.Infinite
                    running: statusUnknownSpinner.visible
                }
            }
        }

        Label {
            id: pcNameText
            text: model.name

            width: parent.width
            anchors.top: pcIcon.bottom
            anchors.topMargin: 20
            anchors.bottom: parent.bottom
            font.pointSize: 16
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }

        Loader {
            id: pcContextMenuLoader
            asynchronous: true
            onLoaded: {
                // 造好之前有人点过，把那次点击补上。但要确认这一页还在最前面 ——
                // 点完立刻返回或进入某台主机的话，菜单会弹在新页面上。
                if (pcContextMenuLoader.parent.pendingMenuRequest !== 0) {
                    if (pcGrid.StackView.status === StackView.Active) {
                        pcContextMenuLoader.parent.openContextMenu(
                            pcContextMenuLoader.parent.pendingMenuRequest === 2)
                    }
                    else {
                        pcContextMenuLoader.parent.pendingMenuRequest = 0
                    }
                }
            }
            sourceComponent: NavigableMenu {
                id: pcContextMenu
                initiator: pcContextMenuLoader.parent
                MenuItem {
                    text: qsTr("PC Status: %1").arg(model.online ? qsTr("Online") : qsTr("Offline"))
                    font.bold: true
                    enabled: false
                }
                NavigableMenuItem {
                    text: qsTr("View All Apps")
                    onTriggered: {
                        openAppView(index, model.name, true)
                    }
                    visible: model.online && model.paired
                }
                NavigableMenuItem {
                    text: qsTr("Select Connection IP")
                    onTriggered: showAddressSelectionForComputer(index, model.name, false)
                    visible: model.online && model.paired && computerModel.hasMultipleConnectionAddresses(index)
                }
                NavigableMenuItem {
                    text: qsTr("Wake PC")
                    onTriggered: computerModel.wakeComputer(index)
                    visible: !model.online && model.wakeable
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
                        renamePcDialog.pcIndex = index
                        renamePcDialog.originalName = model.name
                        renamePcDialog.open()
                    }
                }
                NavigableMenuItem {
                    text: qsTr("Delete PC")
                    onTriggered: {
                        deletePcDialog.pcIndex = index
                        deletePcDialog.pcName = model.name
                        deletePcDialog.open()
                    }
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

        onClicked: {
            if (model.online) {
                if (!model.serverSupported) {
                    errorDialog.text = Brand.text(qsTr("The version of GeForce Experience on %1 is not supported by this build of Moonlight. You must update Moonlight to stream from %1.")).arg(model.name)
                    errorDialog.helpText = ""
                    errorDialog.open()
                }
                else if (model.paired) {
                    // Go directly to app view; IP can be changed from there
                    openAppView(index, model.name, false)
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
                openContextMenu(false)
            }
        }

        onPressAndHold: {
            // popup() ensures the menu appears under the mouse cursor
            openContextMenu(true)
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
            openContextMenu(false)
        }

        Keys.onDeletePressed: {
            deletePcDialog.pcIndex = index
            deletePcDialog.pcName = model.name
            deletePcDialog.open()
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
        standardButtons: DialogButtonBox.Cancel
        onRejected: {
            // FIXME: We should interrupt pairing here
        }
    }

    NavigableMessageDialog {
        id: deletePcDialog
        // don't allow edits to the rest of the window while open
        property int pcIndex : -1
        property string pcName : ""
        text: qsTr("Are you sure you want to remove '%1'?").arg(pcName)
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel

        onAccepted: {
            computerModel.deleteComputer(pcIndex)
        }
    }

    NavigableMessageDialog {
        id: testConnectionDialog
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        standardButtons: DialogButtonBox.Ok

        onAboutToShow: {
            testConnectionDialog.text = Brand.text(qsTr("Moonlight is testing your network connection to determine if any required ports are blocked.")) + "\n\n" + qsTr("This may take a few seconds…")
            showSpinner = true
        }

        function connectionTestComplete(result, blockedPorts)
        {
            if (result === -1) {
                text = Brand.text(qsTr("The network test could not be performed because none of Moonlight's connection testing servers were reachable from this PC. Check your Internet connection or try again later."))
                imageSrc = "qrc:/res/baseline-warning-24px.svg"
            }
            else if (result === 0) {
                text = Brand.text(qsTr("This network does not appear to be blocking Moonlight. If you still have trouble connecting, check your PC's firewall settings.") + "\n\n" + qsTr("If you are trying to stream over the Internet, install the Moonlight Internet Hosting Tool on your gaming PC and run the included Internet Streaming Tester to check your gaming PC's Internet connection."))
                imageSrc = "qrc:/res/baseline-check_circle_outline-24px.svg"
            }
            else {
                text = Brand.text(qsTr("Your PC's current network connection seems to be blocking Moonlight. Streaming over the Internet may not work while connected to this network.")) + "\n\n" + qsTr("The following network ports were blocked:") + "\n"
                text += blockedPorts
                imageSrc = "qrc:/res/baseline-error_outline-24px.svg"
            }

            // Stop showing the spinner and show the image instead
            showSpinner = false
        }
    }

    NavigableDialog {
        id: renamePcDialog
        property string label: qsTr("Enter the new name for this PC:")
        property string originalName
        property int pcIndex : -1;

        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel

        onOpened: {
            // Force keyboard focus on the textbox so keyboard navigation works
            editText.forceActiveFocus()
        }

        onClosed: {
            editText.clear()
        }

        onAccepted: {
            if (editText.text) {
                computerModel.renameComputer(pcIndex, editText.text)
            }
        }

        ColumnLayout {
            Text {
                text: renamePcDialog.label
                color: Theme.text
                font.family: Theme.fontSans
                font.pointSize: Theme.fontRowTitle
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            HardTextField {
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

    // 和 AppView 的地址选择框是同一个组件，只有提示语和落地方式不同
    SelectAddressDialog {
        id: selectAddressDialog
        property int pcIndex: -1
        property string pcName: ""
        property bool openAppAfterSelection: false

        onAddressSelected: function(address) {
            var ok = address.isAuto
                    ? computerModel.resetToAutomaticAddressForComputer(pcIndex)
                    : computerModel.setActiveAddressForComputer(pcIndex, address.address, address.port)
            if (!ok) {
                errorDialog.text = qsTr("Unable to switch the connection IP for %1.").arg(pcName)
                errorDialog.helpText = ""
                errorDialog.open()
                return
            }

            if (openAppAfterSelection) {
                openAppView(pcIndex, pcName, false)
            }
        }

        onClosed: {
            addresses = []
            openAppAfterSelection = false
            pcIndex = -1
            pcName = ""
        }
    }

    NavigableMessageDialog {
        id: showPcDetailsDialog
        property string pcDetails : "";
        text: showPcDetailsDialog.pcDetails
        imageSrc: "qrc:/res/baseline-help_outline-24px.svg"
        standardButtons: DialogButtonBox.Ok
    }

    ScrollBar.vertical: ScrollBar {}

    Image {
        id: backgroundImage
        anchors.fill: parent
        source: ""
        fillMode: Image.PreserveAspectCrop
        z: -2
        property string currentImageUrl: ""
        property string activeRequestKey: ""
        property bool lastRequestWasBusy: false

        Settings {
            id: settings
            property string cachedImagePath: ""
            property string cachedSourceKey: ""
            property real lastRefreshTime: Date.now()
        }

        onStatusChanged: {
            if (status === Image.Loading) {
                loadingIndicator.visible = true
            } else if (status === Image.Ready) {
                loadingIndicator.visible = false
            } else if (status === Image.Error) {
                loadingIndicator.visible = false
                if (StreamingPreferences.backgroundSource === StreamingPreferences.BGS_LOCAL) {
                    errorDialog.text = qsTr("The local background could not be loaded. Photography has been restored.")
                    errorDialog.open()
                    restorePhotographyFromInvalidLocalImage()
                }
                else if (usesNetworkSource()) {
                    getBackgroundImage()
                }
            }
        }

        function usesNetworkSource() {
            return StreamingPreferences.backgroundSource !== StreamingPreferences.BGS_LOCAL &&
                   StreamingPreferences.backgroundSource !== StreamingPreferences.BGS_NONE
        }

        function configuredCacheKey() {
            switch (StreamingPreferences.backgroundSource) {
            case StreamingPreferences.BGS_PHOTOGRAPHY:
                return "photography:picsum"
            case StreamingPreferences.BGS_ANIME:
                return "anime:pipw"
            case StreamingPreferences.BGS_API:
                var apiUrl = StreamingPreferences.backgroundImageApi.trim()
                return apiUrl === "" ? "photography:picsum" : "api:" + apiUrl
            case StreamingPreferences.BGS_LOCAL:
                return "local:" + StreamingPreferences.backgroundImageLocalPath
            case StreamingPreferences.BGS_NONE:
                return "none"
            default:
                return "photography:picsum"
            }
        }

        function configuredNetworkUrl() {
            switch (StreamingPreferences.backgroundSource) {
            case StreamingPreferences.BGS_PHOTOGRAPHY:
                return "https://picsum.photos/1920/1080?random=" + Date.now()
            case StreamingPreferences.BGS_API:
                var apiUrl = StreamingPreferences.backgroundImageApi.trim()
                return apiUrl === ""
                       ? "https://picsum.photos/1920/1080?random=" + Date.now()
                       : apiUrl
            case StreamingPreferences.BGS_ANIME:
                return "https://img-api.pipw.top"
            default:
                return ""
            }
        }

        function cacheFileUrl(cachePath) {
            return "file:///" + cachePath.replace(/\\/g, "/").replace(/^\/+/, "")
        }

        function showBackground(imageUrl) {
            source = imageUrl
            currentImageUrl = imageUrl
        }

        function clearBackground() {
            loadNewImageTimer.stop()
            loadingIndicator.visible = false
            source = ""
            currentImageUrl = ""
        }

        function restorePhotographyFromInvalidLocalImage() {
            clearBackground()
            // Clearing the local path also restores BGS_PHOTOGRAPHY in StreamingPreferences.
            StreamingPreferences.backgroundImageLocalPath = ""
            StreamingPreferences.save()
        }

        function reloadFromPreferences(forceRefresh) {
            loadNewImageTimer.stop()

            if (!StreamingPreferences.backgroundSetupCompleted) {
                clearBackground()
                return
            }

            if (StreamingPreferences.backgroundSource === StreamingPreferences.BGS_NONE) {
                clearBackground()
                return
            }

            if (StreamingPreferences.backgroundSource === StreamingPreferences.BGS_LOCAL) {
                var localUrl = StreamingPreferences.backgroundImageLocalPath
                var validationError = imageUtils.validateLocalBackgroundImage(localUrl)
                if (localUrl !== "" && validationError === "") {
                    loadingIndicator.visible = false
                    showBackground(localUrl)
                    return
                }

                if (validationError !== "") {
                    errorDialog.text = validationError + "\n\n" + qsTr("Photography has been restored.")
                    errorDialog.open()
                }
                restorePhotographyFromInvalidLocalImage()
                return
            }

            var cacheKey = configuredCacheKey()
            var canMigrateLegacyCache = settings.cachedSourceKey === "" &&
                                        StreamingPreferences.backgroundSource === StreamingPreferences.BGS_ANIME
            if (!forceRefresh && settings.cachedImagePath &&
                    imageUtils.fileExists(settings.cachedImagePath) &&
                    (settings.cachedSourceKey === cacheKey || canMigrateLegacyCache)) {
                settings.cachedSourceKey = cacheKey
                showBackground(cacheFileUrl(settings.cachedImagePath))

                var oneWeek = 60 * 60 * 1000 * 24 * 7
                if (Date.now() - settings.lastRefreshTime > oneWeek) {
                    loadNewImageTimer.start()
                }
                return
            }

            getBackgroundImage()
        }

        function getBackgroundImage() {
            var requestUrl = configuredNetworkUrl()
            if (requestUrl === "") {
                reloadFromPreferences(false)
                return
            }

            loadingIndicator.visible = true
            var requestKey = configuredCacheKey()
            lastRequestWasBusy = false
            var requestStarted = imageUtils.fetchAndSaveRandomBackground(requestUrl)
            if (requestStarted || !lastRequestWasBusy) {
                activeRequestKey = requestKey
            }
        }

        function handleImageResponse(cachePath) {
            if (activeRequestKey !== configuredCacheKey()) {
                reloadFromPreferences(true)
                return
            }

            settings.cachedImagePath = cachePath
            settings.cachedSourceKey = activeRequestKey
            showBackground(cacheFileUrl(cachePath))
            settings.lastRefreshTime = Date.now()
        }

        function handleImageError(errorMessage) {
            console.error("Background image load failed:", errorMessage)
            if (activeRequestKey !== configuredCacheKey()) {
                reloadFromPreferences(true)
                return
            }

            var displayingActiveCache = settings.cachedImagePath !== "" &&
                    settings.cachedSourceKey === activeRequestKey &&
                    source.toString() === cacheFileUrl(settings.cachedImagePath)
            if (!displayingActiveCache) {
                source = "qrc:/res/gura.png"
                currentImageUrl = ""
            }
        }

        Timer {
            id: loadNewImageTimer
            interval: 1000 // 延迟1秒加载新图片
            repeat: false
            onTriggered: {
                backgroundImage.getBackgroundImage();
            }
        }
    }

    DropArea {
        anchors.fill: parent
        onEntered: function(drag) {
            if (drag.hasUrls) {
                drag.accept(Qt.LinkAction)
                dragBorder.visible = true
            }
        }
        onExited: dragBorder.visible = false
        onDropped: function(drop) {
            dragBorder.visible = false
            if (!drop.hasUrls || drop.urls.length !== 1) {
                errorDialog.text = qsTr("Drop one local image at a time.")
                errorDialog.open()
                return
            }

            pcGrid.applyLocalBackgroundImage(drop.urls[0].toString())
        }
    }

    // 拖放时的边框效果
    Rectangle {
        id: dragBorder
        anchors.fill: parent
        color: "transparent"
        border.color: Theme.acid
        border.width: 4
        visible: false
        z: 1
    }

    // 拖放提示文字
    Column {
        anchors.centerIn: parent
        spacing: Theme.spaceSm
        visible: dragBorder.visible
        z: 1

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Drop To Set Wallpaper")
            color: Theme.acid
            font.family: Theme.fontSans
            font.pointSize: 22
            font.weight: Font.ExtraBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: Theme.tracking(22, 0.08)
        }

        MicroLabel {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "JPG / PNG / WEBP / BMP"
        }
    }

    // 添加右键菜单功能
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        propagateComposedEvents: true
        z: -1  // 确保这个MouseArea位于PC条目之下

        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                if (backgroundImage.currentImageUrl) {
                    console.log("右键菜单被触发")
                    backgroundContextMenu.popup()
                }
            }
        }
    }

    // 添加上下文菜单
    NavigableMenu {
        id: backgroundContextMenu
        property real lastRefreshTime: 0  // Date.now() is a 13-digit millisecond timestamp

        NavigableMenuItem {
            parentMenu: backgroundContextMenu
            text: qsTr("Save wallpaper")
            onTriggered: {
                console.log("触发下载背景图片")
                saveFileDialog.open()
            }
        }

        NavigableMenuItem {
            parentMenu: backgroundContextMenu
            text: qsTr("Refresh wallpaper")
            onTriggered: {
                var currentTime = Date.now();
                if (currentTime - backgroundContextMenu.lastRefreshTime < 10000) {
                    saveNotification.text = qsTr("Please wait at least 10 seconds between refreshes.")
                    saveNotification.open()
                    return;
                }
                backgroundContextMenu.lastRefreshTime = currentTime;

                loadingIndicator.visible = true
                refreshTimer.start()
            }
        }
    }

    // 添加刷新定时器，避免可能的调用冲突
    Timer {
        id: refreshTimer
        interval: 200  // 延迟200毫秒
        repeat: false
        onTriggered: {
            backgroundImage.reloadFromPreferences(true)
        }
    }

    // 文件保存对话框
    FileDialog {
        id: saveFileDialog
        title: qsTr("Choose where to save")
        nameFilters: [qsTr("Image files (*.jpg *.jpeg *.png *.webp)")]
        fileMode: FileDialog.SaveFile

        currentFile: {
            var timestamp = new Date().getTime()
            // 从URL中提取文件扩展名
            var extension = ".jpg"
            if (backgroundImage.currentImageUrl) {
                var urlPath = backgroundImage.currentImageUrl.toString()
                var extMatch = urlPath.match(/\.(jpg|jpeg|png|webp)($|\?)/i)
                if (extMatch) {
                    extension = "." + extMatch[1].toLowerCase()
                }
            }
            return "file:///setu_" + timestamp + extension
        }

        onAccepted: {
            var finalPath = saveFileDialog.fileUrl || saveFileDialog.currentFile || saveFileDialog.file

            console.log("原始路径: " + finalPath)

            if (finalPath) {
                var ext = finalPath.toString().split('.').pop().toLowerCase()
                if (["jpg", "jpeg", "png", "webp"].indexOf(ext) === -1) {
                    finalPath = finalPath + ".jpg"  // 添加默认扩展名
                }
                imageUtils.saveImageToFile(backgroundImage.currentImageUrl, finalPath)
            } else {
                var timestamp = new Date().getTime()
                finalPath = "file:///setu_" + timestamp + ".jpg"
                console.log("使用默认路径: " + finalPath)
                imageUtils.saveImageToFile(backgroundImage.currentImageUrl, finalPath)
            }
        }
    }

    // moonlight-dance
    AnimatedImage {
        id: loadingIndicator
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 10
        }
        opacity: 0.4
        source: "qrc:/res/moonlight-dance.gif"
        width: 40
        height: 40
        playing: visible
        fillMode: Image.PreserveAspectFit
        visible: false
    }

    // 壁纸遮罩强度由软件设置统一控制，默认值仍是原来的 72%。
    Rectangle {
        anchors.fill: parent
        visible: StreamingPreferences.backgroundSource !== StreamingPreferences.BGS_NONE
        color: Qt.rgba(Theme.ink.r, Theme.ink.g, Theme.ink.b,
                       StreamingPreferences.backgroundOverlayOpacity / 100.0)
        z: -1
    }

    ImageUtils {
        id: imageUtils
        onBackgroundReady: function(filePath) {
            loadingIndicator.visible = false
            backgroundImage.handleImageResponse(filePath)
        }
        onBackgroundError: function(errorMessage) {
            loadingIndicator.visible = false
            backgroundImage.handleImageError(errorMessage)
        }
        onBackgroundBusy: backgroundImage.lastRequestWasBusy = true
        onSaveCompleted: function(success, message) {
            if (success) {
                saveNotification.text = qsTr("Image saved to: %1").arg(message)
                // 自动关闭通知
                autoCloseTimer.start()
            } else {
                saveNotification.text = qsTr("Save failed: %1").arg(message)
            }
            saveNotification.open()
        }
    }

    NavigableMessageDialog {
        id: saveNotification
        title: qsTr("Save result")
        standardButtons: DialogButtonBox.Ok

        // 添加自动关闭计时器
        Timer {
            id: autoCloseTimer
            interval: 3000 // 3秒后自动关闭
            repeat: false
            onTriggered: {
                saveNotification.close()
            }
        }
    }
}
