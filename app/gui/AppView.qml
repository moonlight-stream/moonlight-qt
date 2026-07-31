import QtQuick 2.9
import QtQuick.Controls
import QtQuick.Window 2.2

import AppModel 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import StreamingPreferences 1.0

import "theme"

CenteredGridView {
    // 这一页自带壁纸，main.qml 不用再垫一层
    readonly property bool usesOwnBackground: true
    readonly property int nameRole: AppModel.NameRole
    readonly property int runningRole: AppModel.RunningRole
    readonly property int boxArtRole: AppModel.BoxArtRole
    readonly property int hiddenRole: AppModel.HiddenRole
    readonly property int appIdRole: AppModel.AppIdRole
    readonly property int directLaunchRole: AppModel.DirectLaunchRole
    readonly property int appCollectorGameRole: AppModel.AppCollectorGameRole

    property int computerIndex
    property AppModel appModel : createModel()
    property bool activated
    property bool showHiddenGames
    property bool showGames

    id: appGrid
    focus: true
    activeFocusOnTab: true
    topMargin: 72   // 工具栏 56 + 一格间距
    bottomMargin: 5
    cellWidth: 230; cellHeight: 297;

    // 当前选中显示器: "" = 未选, "vdd" = VDD, 其他 = 物理显示器 guid
    property string selectedDisplayId: ""
    // 当前选中是否 VDD
    property bool isVddSelected: selectedDisplayId === "vdd"
    // 物理显示器列表
    property var displayList: []
    // 是否有多个连接地址
    property bool hasMultipleAddresses: appModel.hasMultipleConnectionAddresses()
    // 当前活动地址信息
    property var activeAddressInfo: appModel.getActiveAddressInfo()
    // 是否使用自动选择模式
    property bool useAutoAddress: true

    // 加载显示器列表
    function loadDisplays() {
        var displays = appModel.getDisplayList()
        displayList = displays
        displayListModel.clear()
        for (var i = 0; i < displays.length; i++) {
            displayListModel.append({
                "displayName": displays[i].name,
                "displayGuid": displays[i].guid,
                "displayIndex": displays[i].index
            })
        }
    }

    // 屏幕选择弹窗
    function openDisplayDialog() {
        loadDisplays()
        displayDialog.open()
    }

    // IP选择弹窗
    function openIpDialog() {
        var addresses = appModel.getConnectionAddresses()
        ipDialog.addresses = addresses
        // Find current active index
        var activeIdx = 0
        if (useAutoAddress) {
            activeIdx = 0 // "Auto (default)" is always index 0
        } else {
            for (var i = 0; i < addresses.length; i++) {
                if (addresses[i].isActive && !addresses[i].isAuto) {
                    activeIdx = i
                    break
                }
            }
        }
        ipCombo.currentIndex = activeIdx
        ipDialog.open()
    }

    Popup {
        id: displayDialog
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        anchors.centerIn: parent
        width: Math.min(500, appGrid.width - 40)
        padding: Theme.spaceXl

        background: Panel {
            fill: Theme.surfaceLayer
            accentBarWidth: Theme.accentBar
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Theme.spaceLg

            // 标题
            Text {
                text: qsTr("Display Settings")
                color: Theme.text
                font.family: Theme.fontSans
                font.pointSize: Theme.fontCardTitle
                font.weight: Font.ExtraBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: Theme.tracking(Theme.fontCardTitle, 0.08)
            }

            // 分隔线
            Rectangle {
                width: parent.width
                height: 1
                color: Theme.line
            }

            // 显示器选择区
            MicroLabel {
                text: qsTr("Select Display:")
            }

            Flow {
                width: parent.width
                spacing: Theme.spaceSm

                // 动态物理显示器按钮
                Repeater {
                    model: ListModel { id: displayListModel }

                    Rectangle {
                        readonly property bool selected: selectedDisplayId === model.displayGuid

                        width: displayBtnLabel.implicitWidth + Theme.spaceXl
                        height: 32
                        color: selected ? Theme.accent : Theme.surface2
                        border.color: selected ? Theme.accentStrong : Theme.lineStrong
                        border.width: 1

                        Text {
                            id: displayBtnLabel
                            anchors.centerIn: parent
                            text: model.displayName
                            color: parent.selected ? Theme.ink : Theme.textDim
                            font.family: Theme.fontSans
                            font.pointSize: Theme.fontBody
                            font.bold: parent.selected
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                selectedDisplayId = model.displayGuid
                                var saved = StreamingPreferences.customScreenMode
                                for (var i = 0; i < physicalModeModel.count; i++) {
                                    if (physicalModeModel.get(i).val === saved) {
                                        combinationModeCombo.currentIndex = i
                                        break
                                    }
                                }
                            }
                        }
                    }
                }

                // VDD 按钮
                Rectangle {
                    width: vddBtnLabel.implicitWidth + Theme.spaceXl
                    height: 32
                    color: isVddSelected ? Theme.acid : Theme.surface2
                    border.color: isVddSelected ? Theme.acid : Theme.lineStrong
                    border.width: 1

                    Text {
                        id: vddBtnLabel
                        anchors.centerIn: parent
                        text: qsTr("VDD Display")
                        color: isVddSelected ? Theme.ink : Theme.textDim
                        font.family: Theme.fontSans
                        font.pointSize: Theme.fontBody
                        font.bold: isVddSelected
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            selectedDisplayId = "vdd"
                            var saved = StreamingPreferences.customVddScreenMode
                            for (var i = 0; i < vddModeModel.count; i++) {
                                if (vddModeModel.get(i).val === saved) {
                                    combinationModeCombo.currentIndex = i
                                    break
                                }
                            }
                        }
                    }
                }
            }

            // 组合模式区（选中显示器后显示）
            Column {
                width: parent.width
                spacing: Theme.spaceSm
                visible: selectedDisplayId !== ""

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.line
                }

                MicroLabel {
                    text: isVddSelected ? qsTr("VDD Combination Mode:") : qsTr("Screen Combination Mode:")
                }

                AutoResizingComboBox {
                    id: combinationModeCombo
                    width: parent.width
                    maximumWidth: parent.width
                    textRole: "text"
                    model: isVddSelected ? vddModeModel : physicalModeModel

                    Component.onCompleted: {
                        var saved = StreamingPreferences.customScreenMode
                        for (var i = 0; i < physicalModeModel.count; i++) {
                            if (physicalModeModel.get(i).val === saved) {
                                currentIndex = i
                                break
                            }
                        }
                    }

                    onActivated: {
                        var val = combinationModeCombo.model.get(currentIndex).val
                        if (isVddSelected) {
                            StreamingPreferences.customVddScreenMode = val
                        } else {
                            StreamingPreferences.customScreenMode = val
                        }
                        StreamingPreferences.save()
                    }
                }
            }
        }
    }

    // 物理显示器组合模式
    ListModel {
        id: physicalModeModel
        ListElement { text: qsTr("Use host config (default)"); val: -1 }
        ListElement { text: qsTr("Do not change"); val: 0 }
        ListElement { text: qsTr("Ensure active"); val: 1 }
        ListElement { text: qsTr("Ensure primary"); val: 2 }
        ListElement { text: qsTr("Only display"); val: 3 }
    }

    // VDD 显示器组合模式
    ListModel {
        id: vddModeModel
        ListElement { text: qsTr("Use host config (default)"); val: -1 }
        ListElement { text: qsTr("Keep current layout"); val: 0 }
        ListElement { text: qsTr("VDD primary + Physical extended"); val: 1 }
        ListElement { text: qsTr("Physical primary + VDD extended"); val: 2 }
        ListElement { text: qsTr("VDD only (disable physical)"); val: 3 }
    }

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
    }

    // Re-syncs the "running" badge against the latest NvComputer state. The
    // polling thread can update currentGameId through a code path that
    // doesn't reach AppModel's slot (e.g. mDNS PendingAddTask folding under
    // contended locks), leaving the badge stale after we return from a
    // stream. We re-check on activation and again a few ticks later to cover
    // the case where the host hasn't finished tearing down the prior
    // session by the time onActivated fires.
    Timer {
        id: postActivationResyncTimer
        interval: 500
        repeat: true
        property int ticksLeft: 0
        onTriggered: {
            appModel.forceSyncCurrentGame()
            if (--ticksLeft <= 0) {
                stop()
            }
        }
        function kick() {
            ticksLeft = 4   // 0.5s, 1.0s, 1.5s, 2.0s
            restart()
        }
    }

    StackView.onActivated: {
        appModel.computerLost.connect(computerLost)
        activated = true

        // 从服务端加载显示器列表
        loadDisplays()

        // Self-heal the running-game indicator in case our cached state
        // drifted from NvComputer's actual currentGameId while we were
        // on another page (typically during a streaming session).
        appModel.forceSyncCurrentGame()
        postActivationResyncTimer.kick()

        // Highlight the first item if a gamepad is connected
        if (currentIndex === -1 && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            currentIndex = 0
        }

        if (!showGames && !showHiddenGames) {
            // Check if there's a direct launch app
            var directLaunchAppIndex = model.getDirectLaunchAppIndex();
            if (directLaunchAppIndex >= 0) {
                // Start the direct launch app if nothing else is running
                currentIndex = directLaunchAppIndex
                currentItem.launchOrResumeSelectedApp(false)

                // Set showGames so we will not loop when the stream ends
                showGames = true
            }
        }
    }

    StackView.onDeactivating: {
        appModel.computerLost.disconnect(computerLost)
        activated = false
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import AppModel 1.0; AppModel {}', parent, '')
        model.initialize(ComputerManager, computerIndex, showHiddenGames)
        return model
    }

    model: appModel

    delegate: NavigableItemDelegate {
        id: appTile

        width: 220; height: 287;
        grid: appGrid
        padding: 0

        property alias appContextMenu: appContextMenuLoader.item
        property alias appNameText: appNameTextLoader.item

        // hover / 手柄高亮 / 键盘焦点走同一套视觉，别分三种状态
        readonly property bool active: hovered || highlighted

        // Dim the app if it's hidden
        opacity: model.hidden ? 0.4 : 1.0

        // 抬起时的位移。Panel 自己也会算一份，但内容层在 background 外面，
        // 两边必须共用同一个动画值才不会错开。
        property real tileShift: active ? -3 : 0

        Behavior on tileShift {
            NumberAnimation { duration: Theme.durFast; easing.type: Theme.easing }
        }

        // FluentWinUI3 给 ItemDelegate 的默认背景是圆角 + hover 高亮块，整块替掉。
        // background 里只放这块硬卡片本身，别放任何要点的东西 —— 原因见 tileBody。
        background: Panel {
            lifted: appTile.active
            liftShift: appTile.tileShift
            fill: Theme.ink
            borderColor: appTile.active ? Theme.accent : Theme.line

            // 正在运行 → 左侧酸性绿粗条。整个应用里只有这里和 LIVE 徽标用酸性绿。
            accentBarColor: Theme.acid
            accentBarWidth: model.running ? Theme.accentBarStrong : 0
        }

        // 封面、信息条、徽标、Resume/Quit 按钮都住在 background 外面。
        //
        // 一开始它们是 Panel 的子项，结果运行中的游戏上那两个按钮点了没反应：
        // Control 会把 background 压到 z = -1，而 Qt 的命中测试顺序是
        // 「z >= 0 的子项 → 控件自己 → z < 0 的子项」，所以 ItemDelegate 自己的
        // onClicked 永远先把点击吃掉，按钮根本等不到。
        //
        // 代价是位移要自己跟：Panel 的「抬起」是把本体往左上挪 3px，这一层必须用
        // 同一个 tileShift，否则封面会和描边错开。
        Item {
            id: tileBody

            // 运行时左边让出那条酸性绿粗条的位置
            readonly property int barInset: model.running ? Theme.accentBarStrong : 0

            x: appTile.tileShift + 1 + barInset   // +1 是别盖住 Panel 那 1px 描边
            y: appTile.tileShift + 1
            width: appTile.width - 2 - barInset
            height: appTile.height - 2
            clip: true

            Image {
                property bool isPlaceholder: false
                readonly property real requestedDpr:
                    Window.window ? Window.window.devicePixelRatio : 1

                id: appIcon

                // 封面铺满整块 tile，不再是居中的 200×267 + 顶部 10px 偏移
                anchors.fill: parent
                source: model.boxart
                sourceSize: Qt.size(Math.max(1, Math.ceil(width * requestedDpr)),
                                    Math.max(1, Math.ceil(height * requestedDpr)))
                fillMode: Image.PreserveAspectCrop
                asynchronous: true

                onSourceChanged: {
                    // BoxArtManager normalizes host placeholders to this resource
                    // before display, so source-size decoding cannot break detection.
                    isPlaceholder = source.toString() === "qrc:/res/no_app_image.png"
                }

                // Display a tooltip with the full name if it's truncated
                ToolTip.text: model.name
                ToolTip.delay: 1000
                ToolTip.timeout: 5000
                ToolTip.visible: appTile.active &&
                                 (nameLabel.truncated || (appNameText && appNameText.truncated))
            }

            // 占位封面的大字名。声明在这里（紧跟封面之后）是为了排在下面那层
            // 运行态蒙版之下 —— 那层蒙版带 visible: !isPlaceholder，所以永远不会
            // 挡住这段大字名，而 Resume/Quit 按钮仍然画在它上面，和改动前一致。
            Loader {
                id: appNameTextLoader
                active: appIcon.isPlaceholder

                // This loader is not asynchronous to avoid noticeable differences
                // in the time in which the text loads for each game.

                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                }
                height: model.running ? 175 : parent.height

                sourceComponent: Text {
                    id: appNameText
                    text: model.name
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pointSize: 20
                    font.weight: Font.ExtraBold
                    font.letterSpacing: Theme.trackingTight(20)
                    leftPadding: Theme.spaceLg
                    rightPadding: Theme.spaceLg
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }
            }

            // 底部信息条。以前游戏名只在占位封面时才画出来，有真封面的游戏
            // 根本读不到名字 —— 这条就是修那个。占位封面仍旧走下面的大字名。
            Rectangle {
                id: infoBar

                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                }
                height: infoColumn.implicitHeight + Theme.spaceSm * 2
                color: Qt.rgba(Theme.ink.r, Theme.ink.g, Theme.ink.b, 0.92)
                visible: !appIcon.isPlaceholder

                Rectangle {
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: 1
                    color: Theme.line
                }

                Column {
                    id: infoColumn

                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: Theme.spaceSm
                        rightMargin: Theme.spaceSm
                    }
                    spacing: 2

                    Text {
                        id: nameLabel

                        width: parent.width
                        text: model.name
                        color: Theme.text
                        font.family: Theme.fontSans
                        font.pointSize: Theme.fontRowTitle
                        font.weight: Font.DemiBold
                        font.letterSpacing: Theme.trackingTight(Theme.fontRowTitle)
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    MicroLabel {
                        width: parent.width
                        text: qsTr("Direct Launch")
                        color: Theme.accent
                        visible: model.directLaunch
                        height: visible ? implicitHeight : 0
                    }
                }
            }

            Loader {
                active: model.running
                asynchronous: true

                // 别盖住信息条，游戏名在运行时也要能读
                anchors.fill: parent
                anchors.bottomMargin: infoBar.visible ? infoBar.height : 0

                sourceComponent: Item {
                    // 压暗封面让按钮读得出来。占位封面本来就是一块平灰，不用压。
                    Rectangle {
                        anchors.fill: parent
                        color: Qt.rgba(Theme.ink.r, Theme.ink.g, Theme.ink.b, 0.55)
                        visible: !appIcon.isPlaceholder
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        // 占位封面的大字名占满整块，按钮往上挪开
                        anchors.verticalCenterOffset: appIcon.isPlaceholder ? -70 : 0
                        spacing: Theme.spaceMd

                        HardButton {
                            // Don't steal focus from the toolbar buttons
                            focusPolicy: Qt.NoFocus

                            width: 62; height: 62

                            icon.source: "qrc:/res/play_arrow_FILL1_wght700_GRAD200_opsz48.svg"
                            icon.width: 34
                            icon.height: 34
                            icon.color: Theme.text

                            onClicked: {
                                launchOrResumeSelectedApp(true)
                            }

                            ToolTip.text: qsTr("Resume Game")
                            ToolTip.delay: 1000
                            ToolTip.timeout: 3000
                            ToolTip.visible: hovered
                        }

                        HardButton {
                            // Don't steal focus from the toolbar buttons
                            focusPolicy: Qt.NoFocus

                            width: 62; height: 62

                            icon.source: "qrc:/res/stop_FILL1_wght700_GRAD200_opsz48.svg"
                            icon.width: 34
                            icon.height: 34
                            icon.color: Theme.danger

                            onClicked: {
                                doQuitGame()
                            }

                            ToolTip.text: qsTr("Quit Game")
                            ToolTip.delay: 1000
                            ToolTip.timeout: 3000
                            ToolTip.visible: hovered
                        }
                    }
                }
            }

            // LIVE / HIDDEN 徽标声明在最后，也就是画在最上层。运行态蒙版把封面
            // 压到 55%，徽标要是排在它下面就会被一起压暗（实测酸性绿被压成
            // (49,55,63)）—— 状态标记必须是整块 tile 里最亮的东西。
            //
            // 光晕垫在徽标之前才在下层：QtGraphicalEffects 不一定可用，直接用一圈
            // 半透明酸性绿顶替参考站的 box-shadow: 0 0 12px。
            Rectangle {
                anchors.fill: liveBadge
                anchors.margins: -3
                color: Theme.acidGlow
                visible: liveBadge.visible
            }

            Rectangle {
                id: liveBadge

                anchors {
                    left: parent.left
                    top: parent.top
                    leftMargin: Theme.spaceSm
                    topMargin: Theme.spaceSm
                }
                width: liveText.implicitWidth + Theme.spaceSm * 2
                height: liveText.implicitHeight + Theme.spaceXs * 2
                color: Theme.acid
                visible: model.running

                MicroLabel {
                    id: liveText
                    anchors.centerIn: parent
                    text: "● " + qsTr("Live")
                    color: Theme.ink
                }
            }

            Rectangle {
                anchors {
                    right: parent.right
                    top: parent.top
                    rightMargin: Theme.spaceSm
                    topMargin: Theme.spaceSm
                }
                width: hiddenText.implicitWidth + Theme.spaceSm * 2
                height: hiddenText.implicitHeight + Theme.spaceXs * 2
                color: Theme.surface2
                border.width: 1
                border.color: Theme.lineStrong
                visible: model.hidden

                MicroLabel {
                    id: hiddenText
                    anchors.centerIn: parent
                    text: qsTr("Hidden")
                }
            }
        }

        function launchOrResumeSelectedApp(quitExistingApp)
        {
            var runningId = appModel.getRunningAppId()
            if (runningId !== 0 && runningId !== model.appid) {
                if (quitExistingApp) {
                    quitAppDialog.appName = appModel.getRunningAppName()
                    quitAppDialog.segueToStream = true
                    quitAppDialog.nextAppName = model.name
                    quitAppDialog.nextAppIndex = index
                    quitAppDialog.open()
                }

                return
            }

            var component = Qt.createComponent("StreamSegue.qml")
            var segue = component.createObject(stackView, {
                                                   "appName": model.name,
                                                   "boxArtUrl": model.boxart,
                                                   "session": appModel.createSessionForApp(index),
                                                   "isResume": runningId === model.appid
                                               })
            stackView.push(segue)
        }

        onClicked: {
            // Only allow clicking on the box art for non-running games.
            // For running games, buttons will appear to resume or quit which
            // will handle starting the game and clicks on the box art will
            // be ignored.
            if (!model.running) {
                launchOrResumeSelectedApp(true)
            }
        }

        onPressAndHold: {
            // popup() ensures the menu appears under the mouse cursor
            if (appContextMenu.popup) {
                appContextMenu.popup()
            }
            else {
                // Qt 5.9 doesn't have popup()
                appContextMenu.open()
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton;
            onClicked: {
                parent.pressAndHold()
            }
        }

        Keys.onReturnPressed: {
            // Open the app context menu if activated via the gamepad or keyboard
            // for running games. If the game isn't running, the above onClicked
            // method will handle the launch.
            if (model.running) {
                // This will be keyboard/gamepad driven so use
                // open() instead of popup()
                appContextMenu.open()
            }
        }

        Keys.onEnterPressed: {
            // Open the app context menu if activated via the gamepad or keyboard
            // for running games. If the game isn't running, the above onClicked
            // method will handle the launch.
            if (model.running) {
                // This will be keyboard/gamepad driven so use
                // open() instead of popup()
                appContextMenu.open()
            }
        }

        Keys.onMenuPressed: {
            // This will be keyboard/gamepad driven so use open() instead of popup()
            appContextMenu.open()
        }

        function doQuitGame() {
            quitAppDialog.appName = appModel.getRunningAppName()
            quitAppDialog.segueToStream = false
            quitAppDialog.open()
        }

        Loader {
            id: appContextMenuLoader
            asynchronous: true
            sourceComponent: NavigableMenu {
                id: appContextMenu
                initiator: appContextMenuLoader.parent
                NavigableMenuItem {
                    text: model.running ? qsTr("Resume Game") : qsTr("Launch Game")
                    onTriggered: launchOrResumeSelectedApp(true)
                }
                NavigableMenuItem {
                    text: qsTr("Quit Game")
                    onTriggered: doQuitGame()
                    visible: model.running
                }
                NavigableMenuItem {
                    checkable: true
                    checked: model.directLaunch
                    text: qsTr("Direct Launch")
                    onTriggered: appModel.setAppDirectLaunch(model.index, !model.directLaunch)
                    enabled: !model.hidden

                    ToolTip.text: qsTr("Launch this app immediately when the host is selected, bypassing the app selection grid.")
                    ToolTip.delay: 1000
                    ToolTip.timeout: 3000
                    ToolTip.visible: hovered
                }
                NavigableMenuItem {
                    checkable: true
                    checked: model.hidden
                    text: qsTr("Hide Game")
                    onTriggered: appModel.setAppHidden(model.index, !model.hidden)
                    enabled: model.hidden || (!model.running && !model.directLaunch)

                    ToolTip.text: qsTr("Hide this game from the app grid. To access hidden games, right-click on the host and choose %1.").arg(qsTr("View All Apps"))
                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                }
            }
        }
    }

    // 空状态：Manrope 800 大标题 + DM Mono 暗色副行
    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spaceXl * 2, 520)
        spacing: Theme.spaceMd
        visible: appGrid.count === 0

        Text {
            width: parent.width
            text: qsTr("No Apps")
            color: Theme.text
            font.family: Theme.fontSans
            font.pointSize: 26
            font.weight: Font.ExtraBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: Theme.trackingTight(26)
            horizontalAlignment: Text.AlignHCenter
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.line
        }

        Text {
            width: parent.width
            text: qsTr("This computer doesn't seem to have any applications or some applications are hidden")
            color: Theme.textDim
            font.family: Theme.fontMono
            font.pointSize: Theme.fontBody
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }

    Popup {
        id: ipDialog
        property var addresses: []

        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        anchors.centerIn: parent
        width: Math.min(500, appGrid.width - 40)
        padding: Theme.spaceXl

        background: Panel {
            fill: Theme.surfaceLayer
            accentBarWidth: Theme.accentBar
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Theme.spaceLg

            Text {
                text: qsTr("Connection IP Settings")
                color: Theme.text
                font.family: Theme.fontSans
                font.pointSize: Theme.fontCardTitle
                font.weight: Font.ExtraBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: Theme.tracking(Theme.fontCardTitle, 0.08)
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.line
            }

            MicroLabel {
                width: parent.width
                text: qsTr("Select the IP address to connect to this PC:")
                // 这句比一般微标签长，允许折行（MicroLabel 默认是单行省略）
                elide: Text.ElideNone
                wrapMode: Text.Wrap
            }

            // 走 AutoResizingComboBox 而不是裸 ComboBox：方角背景和方角展开面板
            // 都在那个文件里统一定义，裸 ComboBox 会退回 FluentWinUI3 的圆角面板。
            AutoResizingComboBox {
                id: ipCombo
                width: parent.width
                maximumWidth: parent.width
                model: ipDialog.addresses
                textRole: "display"
            }

            // 地址类型 / 未验证告警：都是机读信息，走等宽
            Text {
                visible: ipCombo.currentIndex >= 0 &&
                         ipCombo.currentIndex < ipDialog.addresses.length
                text: visible ? qsTr("Type: %1").arg(ipDialog.addresses[ipCombo.currentIndex].type) : ""
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontBody
                wrapMode: Text.Wrap
                width: parent.width
            }

            Text {
                visible: ipCombo.currentIndex > 0 &&
                         ipCombo.currentIndex < ipDialog.addresses.length &&
                         !ipDialog.addresses[ipCombo.currentIndex].isTested
                text: qsTr("Warning: This address has not been verified by polling yet.")
                color: Theme.danger
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
                width: parent.width
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.line
            }

            Row {
                spacing: Theme.spaceMd
                anchors.horizontalCenter: parent.horizontalCenter

                HardButton {
                    text: qsTr("Apply")
                    onClicked: {
                        if (ipCombo.currentIndex < 0 || ipCombo.currentIndex >= ipDialog.addresses.length) {
                            return
                        }

                        var selected = ipDialog.addresses[ipCombo.currentIndex]
                        if (selected.isAuto) {
                            useAutoAddress = true
                        } else {
                            useAutoAddress = false
                            appModel.setActiveAddress(selected.address, selected.port)
                            activeAddressInfo = appModel.getActiveAddressInfo()
                        }
                        ipDialog.close()
                    }
                }

                HardButton {
                    text: qsTr("Cancel")
                    onClicked: ipDialog.close()
                }
            }

            Text {
                text: qsTr("\"Auto\" uses the default address selection with automatic fallback. Selecting a specific IP will pin the connection to that address.")
                color: Theme.textFaint
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
                width: parent.width
            }
        }
    }

    NavigableMessageDialog {
        id: quitAppDialog
        property string appName : ""
        property bool segueToStream : false
        property string nextAppName: ""
        property int nextAppIndex: 0
        text:qsTr("Are you sure you want to quit %1? Any unsaved progress will be lost.").arg(appName)
        standardButtons: Dialog.Yes | Dialog.No

        function quitApp() {
            var component = Qt.createComponent("QuitSegue.qml")
            var params = {"appName": appName, "quitRunningAppFn": function() { appModel.quitRunningApp() }}
            if (segueToStream) {
                // Store the session and app name if we're going to stream after
                // successfully quitting the old app.
                params.nextAppName = nextAppName
                params.nextBoxArtUrl = appModel.data(appModel.index(nextAppIndex, 0), boxArtRole)
                params.nextSession = appModel.createSessionForApp(nextAppIndex)
            }
            else {
                params.nextAppName = null
                params.nextBoxArtUrl = ""
                params.nextSession = null
            }

            stackView.push(component.createObject(stackView, params))
        }

        onAccepted: quitApp()
    }

    ScrollBar.vertical: ScrollBar {}

    // 壁纸和它的遮罩都是 GridView contentItem 的兄弟节点，和 delegate 同一个父级。
    // z 相同的话按声明顺序绘制，而这两块声明在 delegate 之后 —— 所以必须给负 z，
    // 否则遮罩会盖在所有 tile 上面（封面会被压成一片灰，实测峰值只剩 48%）。
    Image {
        id: backgroundImage
        anchors.fill: parent
        source: getBackgroundSource()
        // 壁纸压得比以前更暗（0.3 → 0.18）：新风格里 tile 要读起来像一块硬物件，
        // 底图越安静，方角 + 硬投影的层次就越清楚。
        opacity: 0.18
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        z: -2
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(Theme.ink.r, Theme.ink.g, Theme.ink.b, 0.55)
        z: -1
    }


    function getBackgroundSource() {
        // 优先使用正在运行的应用的封面
        let runningAppId = appModel.getRunningAppId()
        if (runningAppId !== 0) {
            for (let i = 0; i < appModel.rowCount(); i++) {
                let appIndex = appModel.index(i, 0)
                let appId = appModel.data(appIndex, appIdRole)
                if (appId === runningAppId) {
                    let boxArt = appModel.data(appIndex, boxArtRole)
                    return boxArt || "qrc:/res/gura.png"
                }
            }
        }

        // 没有运行应用时使用第一个应用的封面
        if (appModel.rowCount() > 0) {
            let firstAppIndex = appModel.index(0, 0)
            let boxArt = appModel.data(firstAppIndex, boxArtRole)
            return boxArt || "qrc:/res/gura.png"
        }

        return "qrc:/res/gura.png"
    }

    // 修改数据变化监听
    Connections {
        target: appModel
        function onDataChanged() {
            // 使用Qt.callLater防止重复更新
            Qt.callLater(function() {
                let newSource = getBackgroundSource()
                if (backgroundImage.source !== newSource) {
                    backgroundImage.source = newSource
                }
            })
        }
    }
}
