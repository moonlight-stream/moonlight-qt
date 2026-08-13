import QtQuick 2.0
import QtQuick.Controls
import QtQuick.Window 2.2

import SdlGamepadKeyNavigation 1.0
import Session 1.0
import SystemProperties 1.0

import "theme"
import "Brand.js" as Brand

Item {
    property Session session
    property string appName

    // 正在启动的这个游戏的封面。加载页用它做背景，而不是首页的主机壁纸——
    // 你正要进的是这个游戏，画面就该先切过去。取不到时退回主机壁纸。
    property string boxArtUrl: ""

    // 自带背景，main.qml 的全局壁纸层不用再垫一层
    readonly property bool usesOwnBackground: true
    property string stageText : isResume ? qsTr("Resuming %1...").arg(appName) :
                                           qsTr("Starting %1...").arg(appName)
    property bool isResume : false
    property bool quitAfter : false

    function stageStarting(stage)
    {
        // Update the spinner text
        stageText = qsTr("Starting %1...").arg(stage)
    }

    function stageFailed(stage, errorCode, failingPorts)
    {
        // Display the error dialog after Session::exec() returns
        streamSegueErrorDialog.text = qsTr("Starting %1 failed: Error %2").arg(stage).arg(errorCode)

        if (failingPorts) {
            streamSegueErrorDialog.text += "\n\n" + qsTr("Check your firewall and port forwarding rules for port(s): %1").arg(failingPorts)
        }
    }

    function hideForStreaming()
    {
        // Hide the UI contents so the user doesn't
        // see them briefly when we pop off the StackView
        stageSpinner.visible = false
        stageLabel.visible = false
        hintText.visible = false

        // 窗口本身不在这里藏，由 Session::exec() 在串流窗口进入全屏之后隐藏。
        // 提前藏的话，macOS 切进新 Space 的整个动画期间旧 Space 露出来的是桌面，
        // 而不是这层已经全黑的幕。
    }

    function connectionStarted()
    {
        // 淡出到全黑。Session::exec() 会等这条动画跑完再创建串流窗口，
        // 所以交接是在一块纯黑上完成的，中间不会闪。
        backgroundZoomAnimation.stop()
        exitAnimation.start()
    }

    function displayLaunchError(text)
    {
        // Display the error dialog after Session::exec() returns
        streamSegueErrorDialog.text = text
        console.error(text)
    }

    function quitStarting()
    {
        // Avoid the push transition animation
        var component = Qt.createComponent("QuitSegue.qml")
        stackView.replace(stackView.currentItem, component.createObject(stackView, {"appName": appName}), StackView.Immediate)

        // Show the Qt window again to show quit segue
        window.visible = true
    }

    function sessionFinished(portTestResult)
    {
        if (portTestResult !== 0 && portTestResult !== -1 && streamSegueErrorDialog.text) {
            streamSegueErrorDialog.text += "\n\n" + Brand.text(qsTr("This PC's Internet connection is blocking Moonlight. Streaming over the Internet may not work while connected to this network."))
        }

        // Re-enable GUI gamepad usage now
        SdlGamepadKeyNavigation.enable()

        // Pop the StreamSegue off the stack if this is a GUI-based app launch
        if (!quitAfter) {
            stackView.pop()
        }

        if (quitAfter && !streamSegueErrorDialog.text) {
            // If this was a CLI launch without errors, exit now
            Qt.quit()
        }
        else {
            // Show the Qt window again after streaming
            window.visible = true

            // Display any launch errors. We do this after
            // the Qt UI is visible again to prevent losing
            // focus on the dialog which would impact gamepad
            // users.
            if (streamSegueErrorDialog.text) {
                streamSegueErrorDialog.quitAfter = quitAfter
                streamSegueErrorDialog.open()
            }
        }
    }

    function sessionReadyForDeletion()
    {
        // Garbage collect the Session object since it's pretty heavyweight
        // and keeps other libraries (like SDL_TTF) around until it is deleted.
        session = null
        gc()
    }

    StackView.onDeactivating: {
        // Show the toolbar again when popped off the stack
        toolBar.shown = true

        // Re-enable GUI gamepad usage now
        SdlGamepadKeyNavigation.enable()
    }

    StackView.onActivated: {
        // Hide the toolbar before we start loading
        toolBar.shown = false

        // Hook up our signals
        session.stageStarting.connect(stageStarting)
        session.stageFailed.connect(stageFailed)
        session.connectionStarted.connect(connectionStarted)
        session.displayLaunchError.connect(displayLaunchError)
        session.quitStarting.connect(quitStarting)
        session.sessionFinished.connect(sessionFinished)
        session.readyForDeletion.connect(sessionReadyForDeletion)

        // Ensure the SystemProperties async thread is finished,
        // since it may currently be using the SDL video subsystem
        SystemProperties.waitForAsyncLoad()

        enterAnimation.start()
        backgroundZoomAnimation.start()

        // Kick off the stream
        spinnerTimer.start()
        streamLoader.active = true
    }

    // 上一页（游戏列表）的背景先留在最底层。封面在它上面淡入，
    // 这样从列表切到加载页不是整张图硬换，而是接着上一张继续。
    Image {
        id: previousBackground

        anchors.fill: parent
        source: window.backgroundImageUrl
        visible: source != ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        opacity: 0.3
        z: -3
    }

    // 封面加载失败过一次就别再试了，直接退回主机壁纸。
    // 只看 boxArtUrl 是不是空串不够：地址在但图取不下来（换过封面、缓存失效、
    // 主机没这张图）时 status 会停在 Error，而 opacity 绑的是 status === Ready，
    // 结果整层永远是全透明的，加载页只剩一块压暗的底。
    property bool boxArtFailed: false

    onBoxArtUrlChanged: boxArtFailed = false

    Image {
        id: segueBackground

        anchors.fill: parent
        source: (boxArtUrl !== "" && !boxArtFailed)
                    ? boxArtUrl
                    : (window.backgroundImageUrl !== "" ? window.backgroundImageUrl
                                                        : "qrc:/res/gura.png")
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        z: -2

        // 声明式地跟着加载状态淡入。不要用 onStatusChanged 触发动画：
        // 封面通常已经在缓存里，status 在处理器挂上之前就已经是 Ready，
        // 那样动画永远不会触发，背景会一直停在全透明。
        opacity: status === Image.Ready ? 1 : 0

        // 失败要靠事件记下来。同样因为缓存的关系，也可能在处理器挂上之前
        // 就已经是 Error 了，所以创建时再补查一次。
        onStatusChanged: if (status === Image.Error) boxArtFailed = true
        Component.onCompleted: if (status === Image.Error) boxArtFailed = true

        Behavior on opacity {
            NumberAnimation { duration: 700; easing.type: Easing.OutCubic }
        }

        // 缓慢推近，让等待的这几秒不是一张死图
        transform: Scale {
            id: backgroundZoom
            origin.x: segueBackground.width / 2
            origin.y: segueBackground.height / 2
        }
    }

    ParallelAnimation {
        id: backgroundZoomAnimation
        NumberAnimation {
            target: backgroundZoom; property: "xScale"
            from: 1.0; to: 1.08; duration: 14000; easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: backgroundZoom; property: "yScale"
            from: 1.0; to: 1.08; duration: 14000; easing.type: Easing.InOutSine
        }
    }

    // 压暗，保证进度条和文字在任何封面上都读得清
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(Theme.ink.r, Theme.ink.g, Theme.ink.b, 0.72)
        z: -1
    }

    // 进入串流时盖上来的幕，替代原来「一帧之内直接隐藏窗口」的硬切。
    //
    // 这一层刻意用纯黑而不是 Theme.ink：接手它的是 SDL 串流窗口，而 SDL 窗口在拿到
    // 第一帧之前就是纯黑的（实测 macOS 上是 0,0,0）。两边同色，交接那一刻才没有色阶跳变。
    Rectangle {
        id: exitVeil
        anchors.fill: parent
        color: "black"
        opacity: 0
        visible: opacity > 0
        z: 10
    }

    ParallelAnimation {
        id: enterAnimation
        NumberAnimation {
            target: contentRoot; property: "opacity"
            from: 0; to: 1; duration: 420; easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: contentShift; property: "y"
            from: 14; to: 0; duration: 480; easing.type: Easing.OutCubic
        }
    }

    // 进入串流：内容淡出、背景轻微推近、黑幕盖上来，三件事一起做，
    // 读起来像是「被带进游戏」而不是窗口突然不见了。
    ParallelAnimation {
        id: exitAnimation

        NumberAnimation {
            target: contentRoot; property: "opacity"
            to: 0; duration: 260; easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: backgroundZoom; property: "xScale"
            to: 1.14; duration: 340; easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: backgroundZoom; property: "yScale"
            to: 1.14; duration: 340; easing.type: Easing.InOutQuad
        }
        SequentialAnimation {
            NumberAnimation {
                target: exitVeil; property: "opacity"
                to: 1; duration: 340; easing.type: Easing.InOutQuad
            }
            ScriptAction {
                script: hideForStreaming()
            }
        }
    }

    Timer {
        id: spinnerTimer

        // Display the spinner appearance a bit to allow us to reach
        // the code in Session.exec() that pumps the event loop.
        // If we display it immediately, it will briefly hang in the
        // middle of the animation on Windows, which looks very
        // obviously broken.
        interval: 100
        onTriggered: stageSpinner.visible = true
    }

    Timer {
        id: startSessionTimer
        onTriggered: {
            // Garbage collect QML stuff before we start streaming,
            // since we'll probably be streaming for a while and we
            // won't be able to GC during the stream.
            gc()

            // Run the streaming session to completion
            session.start()
        }
    }

    Loader {
        id: streamLoader
        active: false
        asynchronous: true

        onLoaded: {
            // Set the hint text. We do this here rather than
            // in the hintText control itself to synchronize
            // with Session.exec() which requires no concurrent
            // gamepad usage.
            hintText.text = qsTr("Tip:") + " " + qsTr("Press %1 to disconnect your session").arg(SdlGamepadKeyNavigation.getConnectedGamepads() > 0 ?
                                                  qsTr("Start+Select+L1+R1") : qsTr("Ctrl+Alt+Shift+Q"))

            // Stop GUI gamepad usage now
            SdlGamepadKeyNavigation.disable()

            // Initialize the session and probe for host/client capabilities
            if (!session.initialize(window)) {
                sessionFinished(0);
                sessionReadyForDeletion();
                return;
            }

            // Don't wait unless we have toasts to display
            startSessionTimer.interval = 0

            // Display the toasts together in a vertical centered arrangement
            var yOffset = 0
            for (var i = 0; i < session.launchWarnings.length; i++) {
                var text = session.launchWarnings[i]
                console.warn(text)

                // Show the tooltip for 3 seconds
                var toast = Qt.createQmlObject('import QtQuick.Controls 2.2; ToolTip {}', parent, '')
                toast.timeout = 3000
                toast.text = text
                toast.y += yOffset
                toast.visible = true

                // Offset the next toast below the previous one
                yOffset = toast.y + toast.padding + toast.height

                // Allow an extra 500 ms for the tooltip's fade-out animation to finish
                startSessionTimer.interval = toast.timeout + 500;
            }

            // Start the timer to wait for toasts (or start the session immediately)
            startSessionTimer.start()
        }

        sourceComponent: Item {}
    }

    Item {
        id: contentRoot

        anchors.fill: parent
        opacity: 0

        // 淡入的同时轻微上浮
        transform: Translate {
            id: contentShift
            y: 14
        }

        // 阶段文字 + 斜条纹读条。转圈的 BusyIndicator 换成 HardProgress。
        // stageSpinner 这个 id 和 visible 语义保持不变：spinnerTimer 和
        // hideForStreaming() 都在用。
        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width - Theme.spaceXl * 2, 620)
            spacing: Theme.spaceLg

            Text {
                id: stageLabel

                width: parent.width
                text: stageText
                color: Theme.text
                font.family: Theme.fontSans
                font.pointSize: 24
                font.weight: Font.ExtraBold
                font.letterSpacing: Theme.trackingTight(24)
                // 左对齐。居中大字是那种「优雅」排版的做法，这套风格里所有东西都
                // 咬着一条左基线走（工具栏字标、卡片标题、设置行），读条上的阶段文字
                // 也一样 —— 而且它会随阶段变长变短，居中的话每换一句都在左右横跳。
                horizontalAlignment: Text.AlignLeft
                wrapMode: Text.Wrap
            }

            HardProgress {
                id: stageSpinner

                width: parent.width
                visible: false
            }
        }

        Text {
            id: hintText
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 50
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.textDim
            font.family: Theme.fontMono
            font.pointSize: Theme.fontBody
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            wrapMode: Text.Wrap
        }
    }
}
