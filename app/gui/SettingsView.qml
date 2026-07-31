import QtQuick 2.9
import QtQuick.Controls
import QtQuick.Window 2.2

import StreamingPreferences 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import SystemProperties 1.0

import "settings"
import "theme"

// 设置页外壳：左侧分类 rail + 右侧卡片内容。
// 「基本设置」已迁移到 settings/BasicSettingsPage.qml；
// 其余 6 组仍走旧的 GroupBox 路径（settings/LegacySettingsPage.qml），逐步迁移。
Item {
    id: settingsPage
    // 这一页自带壁纸，main.qml 不用再垫一层
    readonly property bool usesOwnBackground: true
    objectName: qsTr("Settings")

    signal languageChanged()

    // 窄窗口时 rail 折叠成顶部横向 tab 条
    readonly property bool compact: width < Theme.compactBreakpoint

    property string category: "basic"

    // 图标取自 Microsoft Fluent UI System Icons（MIT），和 FluentWinUI3 是同一套设计语言。
    // 之前用 emoji，各平台字体不同，渲染出来大小、粗细、配色都对不齐。
    readonly property var categories: [
        { key: "basic",    icon: "qrc:/res/fluent/cat-basic.svg",    title: qsTr("Basic Settings") },
        { key: "audio",    icon: "qrc:/res/fluent/cat-audio.svg",    title: qsTr("Audio Settings") },
        { key: "host",     icon: "qrc:/res/fluent/cat-host.svg",     title: qsTr("Host Settings") },
        { key: "ui",       icon: "qrc:/res/fluent/cat-ui.svg",       title: qsTr("UI Settings") },
        { key: "input",    icon: "qrc:/res/fluent/cat-input.svg",    title: qsTr("Input Settings") },
        { key: "gamepad",  icon: "qrc:/res/fluent/cat-gamepad.svg",  title: qsTr("Gamepad Settings") },
        { key: "advanced", icon: "qrc:/res/fluent/cat-advanced.svg", title: qsTr("Advanced Settings") }
    ]

    StackView.onActivated: {
        // This enables Tab and BackTab based navigation rather than arrow keys.
        // It is required to shift focus between controls on the settings page.
        SdlGamepadKeyNavigation.setUiNavMode(true)

        // Highlight the first item if a gamepad is connected
        //
        // category 会跨次进入保留下来，所以不能无条件去点基本设置页的第一个控件：
        // 当前分类不是 basic 时那个控件是不可见的，forceActiveFocus() 静默失效，
        // 手柄用户就一个可用焦点都没有（firstControl 万一是 undefined 还会抛）。
        if (SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            if (settingsPage.category === "basic" && basicPage.firstControl) {
                basicPage.firstControl.forceActiveFocus(Qt.TabFocus)
            }
            else {
                rail.forceActiveFocus(Qt.TabFocus)
            }
        }
    }

    StackView.onDeactivating: {
        SdlGamepadKeyNavigation.setUiNavMode(false)

        // Save the prefs so the Session can observe the changes
        StreamingPreferences.save()
    }

    Component.onDestruction: {
        // Also save preferences on destruction, since we won't get a
        // deactivating callback if the user just closes Moonlight
        StreamingPreferences.save()
    }

    // 手柄 LB/RB 映射成 PageUp/PageDown，用来切分类
    Keys.onPressed: {
        if (event.key === Qt.Key_PageUp) {
            rail.step(-1)
            event.accepted = true
        }
        else if (event.key === Qt.Key_PageDown) {
            rail.step(1)
            event.accepted = true
        }
    }

    // Reuse the background already owned by the application window. Creating a
    // hidden PcView here duplicated its model, network work, and scene graph.
    Image {
        anchors.fill: parent
        source: Window.window && Window.window.backgroundImageUrl !== ""
                ? Window.window.backgroundImageUrl
                : "qrc:/res/gura.png"
        opacity: 0.35
        fillMode: Image.PreserveAspectCrop
        z: -2
    }

    Rectangle {
        anchors.fill: parent
        // 只做一点点补压：全局壁纸遮罩（main.qml 里的 60% 黑）已经压过一次了，
        // 这里再叠满会糊成一团，所以这层只负责把色调拉回 --background-darker。
        color: Qt.rgba(0.059, 0.090, 0.165, 0.25)
        z: -1
    }

    Item {
        id: body

        anchors {
            fill: parent
            // 让出顶部工具栏（56）+ 一格间距
            topMargin: 72
            leftMargin: Theme.spaceLg
            rightMargin: Theme.spaceLg
            bottomMargin: Theme.spaceSm
        }

        Rectangle {
            id: railBackground

            anchors {
                left: parent.left
                top: parent.top
            }
            width: settingsPage.compact ? body.width : Theme.railWidth
            height: settingsPage.compact ? 52 : body.height

            // 分类栏底板。方角 + 1px 描边，不用硬投影：它贴着窗口左边，
            // 投影只会在右侧和内容区挤在一起。
            radius: 0
            color: Theme.surfaceLayer
            border.width: 1
            border.color: Theme.line

            CategoryRail {
                id: rail
                anchors {
                    fill: parent
                    margins: Theme.spaceXs
                }
                compact: settingsPage.compact
                categories: settingsPage.categories
                currentCategory: settingsPage.category
                onCategoryPicked: {
                    settingsPage.category = category
                    scrollArea.contentY = 0
                }
            }
        }

        SettingsScrollArea {
            id: scrollArea

            anchors {
                top: settingsPage.compact ? railBackground.bottom : parent.top
                topMargin: settingsPage.compact ? Theme.spaceMd : 0
                left: settingsPage.compact ? parent.left : railBackground.right
                leftMargin: settingsPage.compact ? 0 : Theme.spaceLg
                right: parent.right
                bottom: parent.bottom
            }

            BasicSettingsPage {
                id: basicPage
                width: parent.width
                visible: settingsPage.category === "basic"
                height: visible ? implicitHeight : 0
            }

            LegacySettingsPage {
                id: legacyPage
                y: basicPage.height
                width: parent.width
                category: settingsPage.category

                onLanguageChanged: settingsPage.languageChanged()
            }
        }
    }

    Component.onCompleted: {
        // 高级设置组会回写码率滑条、被解码器下拉联动的画质增强开关，
        // 这两个控件现在住在基本设置页里，启动时注入进去。
        legacyPage.bitrateSlider = basicPage.bitrateSlider
        legacyPage.videoEnhancementCheck = basicPage.videoEnhancementCheck

        // 语言切换需要重建若干下拉的模型
        settingsPage.languageChanged.connect(basicPage.languageChanged)
    }
}
