import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import Qt.labs.platform 1.1
import QtCore

import ComputerModel 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0
import ImageUtils 1.0

CenteredGridView {
    property ComputerModel computerModel : createModel()
    property string currentBgUrl: backgroundImage.currentImageUrl  // 添加根组件属性用于外部访问

    id: pcGrid
    focus: true
    activeFocusOnTab: true
    topMargin: 80
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
        if (currentIndex == -1 && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            currentIndex = 0
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
                errorDialog.text += "\n\n" + qsTr("This PC's Internet connection is blocking Moonlight. Streaming over the Internet may not work while connected to this network.")
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

    Row {
        anchors.centerIn: parent
        spacing: 5
        visible: pcGrid.count === 0

        BusyIndicator {
            id: searchSpinner
            visible: StreamingPreferences.enableMdns
        }

        Label {
            height: searchSpinner.height
            elide: Label.ElideRight
            text: StreamingPreferences.enableMdns ? qsTr("Searching for compatible hosts on your local network...")
                                                  : qsTr("Automatic PC discovery is disabled. Add your PC manually.")
            font.pointSize: 20
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }

    model: computerModel

    delegate: NavigableItemDelegate {
        width: 240; height: 240;
        grid: pcGrid

        property alias pcContextMenu : pcContextMenuLoader.item

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
            sourceComponent: NavigableMenu {
                id: pcContextMenu
                MenuItem {
                    text: qsTr("PC Status: %1").arg(model.online ? qsTr("Online") : qsTr("Offline"))
                    font.bold: true
                    enabled: false
                }
                NavigableMenuItem {
                    parentMenu: pcContextMenu
                    text: qsTr("View All Apps")
                    onTriggered: {
                        var component = Qt.createComponent("AppView.qml")
                        var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name, "showHiddenGames": true})
                        stackView.push(appView)
                    }
                    visible: model.online && model.paired
                }
                NavigableMenuItem {
                    parentMenu: pcContextMenu
                    text: qsTr("Wake PC")
                    onTriggered: computerModel.wakeComputer(index)
                    visible: !model.online && model.wakeable
                }
                NavigableMenuItem {
                    parentMenu: pcContextMenu
                    text: qsTr("Test Network")
                    onTriggered: {
                        computerModel.testConnectionForComputer(index)
                        testConnectionDialog.open()
                    }
                }

                NavigableMenuItem {
                    parentMenu: pcContextMenu
                    text: qsTr("Rename PC")
                    onTriggered: {
                        renamePcDialog.pcIndex = index
                        renamePcDialog.originalName = model.name
                        renamePcDialog.open()
                    }
                }
                NavigableMenuItem {
                    parentMenu: pcContextMenu
                    text: qsTr("Delete PC")
                    onTriggered: {
                        deletePcDialog.pcIndex = index
                        deletePcDialog.pcName = model.name
                        deletePcDialog.open()
                    }
                }
                NavigableMenuItem {
                    parentMenu: pcContextMenu
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
                    errorDialog.text = qsTr("The version of GeForce Experience on %1 is not supported by this build of Moonlight. You must update Moonlight to stream from %1.").arg(model.name)
                    errorDialog.helpText = ""
                    errorDialog.open()
                }
                else if (model.paired) {
                    // go to game view
                    var component = Qt.createComponent("AppView.qml")
                    var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name})
                    stackView.push(appView)
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
                pcContextMenu.open()
            }
        }

        onPressAndHold: {
            // popup() ensures the menu appears under the mouse cursor
            if (pcContextMenu.popup) {
                pcContextMenu.popup()
            }
            else {
                // Qt 5.9 doesn't have popup()
                pcContextMenu.open()
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
            pcContextMenu.open()
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

        // Pairing dialog must be modal to prevent double-clicks from triggering
        // pairing twice
        modal: true
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
        
        Settings {
            id: settings
            property string cachedImagePath: ""
            property real lastRefreshTime: Date.now()
        }
        
        onStatusChanged: {
            if (status === Image.Loading) {
                loadingIndicator.visible = true
            } else if (status === Image.Ready) {
                loadingIndicator.visible = false
            } else if (status === Image.Error) {
                loadingIndicator.visible = false
                getBackgroundImage() // 如果缓存图加载失败，尝试加载新图片
            }
        }

        function getBackgroundImage() {
            loadingIndicator.visible = true
            
            var request = new XMLHttpRequest();
            request.open("GET", "https://imgapi.lie.moe/random?sort=pc", true);
            request.timeout = 10000;

            request.onreadystatechange = function() {
                if (request.readyState === XMLHttpRequest.DONE) {
                    loadingIndicator.visible = false
                    if (request.status === 200) {
                        handleImageResponse(request.responseURL)
                    } else {
                        handleImageError(request.status)
                    }
                }
            }

            request.send()
        }

        function handleImageResponse(url) {
            currentImageUrl = url
            pcGrid.currentBgUrl = url  // 同步更新根组件属性
            var cachePath = imageUtils.saveImageFromUrl(url)
            if (cachePath) {
                settings.cachedImagePath = cachePath
                console.log("handleImageResponse: " + cachePath)
                source = "";
                source = "file:///" + encodeURIComponent(cachePath);
                settings.lastRefreshTime = Date.now()
            }
        }

        function handleImageError(status) {
            console.error("Background image load failed:", status)
            if (!source.toString().startsWith("file://")) {
                source = "qrc:/res/gura.jpg"
            }
        }
        
        Component.onCompleted: {
            // 先检查缓存图是否存在
            if (settings.cachedImagePath && imageUtils.fileExists(settings.cachedImagePath)) {
                try {
                    var fileUrl = "file:///" + settings.cachedImagePath.replace(/\\/g, "/").replace(/^\/+/, "");
                    source = fileUrl;
                    console.log("loadBackgroundImageFromCache: " + fileUrl);
                    currentImageUrl = fileUrl;
                    pcGrid.currentBgUrl = fileUrl;  // 初始化时同步属性
                    
                    // 检查是否需要刷新（如果上次刷新时间超过1小时）
                    var oneHour = 60 * 60 * 1000 * 24 * 7;
                    if (Date.now() - settings.lastRefreshTime > oneHour) {
                        loadNewImageTimer.start();
                    }
                } catch (e) {
                    console.log("fail loadBackgroundImageFromCache: " + e);
                    getBackgroundImage();
                }
            } else {
                // 如果没有缓存，立即获取新图片
                getBackgroundImage();
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
        onEntered: {
            drag.accept(Qt.LinkAction)
            dragBorder.visible = true
        }
        onExited: dragBorder.visible = false
        onDropped: {
            dragBorder.visible = false
            if (drop.hasUrls) {
                // 获取拖入的第一个文件路径
                var filePath = drop.urls[0]
                // 检查文件格式
                var ext = filePath.toString().split('.').pop().toLowerCase()
                if (["jpg", "jpeg", "png", "webp"].indexOf(ext) !== -1) {
                    // 更新缓存路径和刷新时间
                    settings.cachedImagePath = filePath.toString().substring(8)
                    settings.lastRefreshTime = Date.now()
                    
                    // 更新背景图 - 处理包含空格的路径
                    if (filePath.toString().startsWith("file:///")) {
                        var pathPart = filePath.toString().substring(8);
                        backgroundImage.source = "file:///" + encodeURIComponent(pathPart).replace(/%2F/g, "/");
                    } else {
                        backgroundImage.source = filePath;
                    }
                    currentImageUrl = filePath;
                    pcGrid.currentBgUrl = filePath;  // 拖放时同步属性
                } else {
                    errorDialog.text = qsTr("不支持的图片格式")
                    errorDialog.open()
                }
            }
        }
    }

    // 拖放时的边框效果
    Rectangle {
        id: dragBorder
        anchors.fill: parent
        color: "transparent"
        border.color: "#4CAF50"
        border.width: 4
        visible: false
        z: 1
    }

    // 拖放提示文字
    Label {
        anchors.centerIn: parent
        text: qsTr("拖放图片设置背景")
        font.pointSize: 24
        color: "white"
        visible: dragBorder.visible
        z: 1
    }

    // 添加右键菜单功能
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        propagateComposedEvents: true
        z: -1  // 确保这个MouseArea位于PC条目之下
        
        onClicked: {
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
        property int lastRefreshTime: 0  // 明确声明属性
        
        NavigableMenuItem {
            parentMenu: backgroundContextMenu
            text: qsTr("下载背景图片")
            onTriggered: {
                console.log("触发下载背景图片")
                saveFileDialog.open()
            }
        }
        
        NavigableMenuItem {
            parentMenu: backgroundContextMenu
            text: qsTr("刷新背景图片")
            onTriggered: {
                var currentTime = Date.now();
                if (currentTime - backgroundContextMenu.lastRefreshTime < 10000) {
                    saveNotification.text = "请稍等片刻再刷新（至少间隔10秒）"
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
            backgroundImage.getBackgroundImage()
        }
    }

    // 文件保存对话框
    FileDialog {
        id: saveFileDialog
        title: qsTr("选择保存位置")
        nameFilters: ["图片文件 (*.jpg *.jpeg *.png *.webp)"]
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
    
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.6)
        z: -1
    }

    ImageUtils {
        id: imageUtils
        onSaveCompleted: {
            if (success) {
                saveNotification.text = "图片已保存到: " + message
                // 自动关闭通知
                autoCloseTimer.start()
            } else {
                saveNotification.text = "保存失败: " + message
            }
            saveNotification.open()
        }
    }

    NavigableMessageDialog {
        id: saveNotification
        title: qsTr("保存结果")
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
