import QtQuick 2.9
import QtQuick.Controls
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import ".."
import "../theme"
import "../Brand.js" as Brand

import StreamingPreferences 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import SystemProperties 1.0

// 尚未迁移到新架构的 6 组设置（音频/主机/界面/输入/手柄/高级），
// 从旧 SettingsView.qml 原样搬运。根节点沿用 settingsPage 这个 id，
// 供组内跨控件绑定继续使用。
Column {
    id: settingsPage

    signal languageChanged()

    // 由 SettingsView 注入的基本设置页控件（跨组引用，见下方 null 保护）
    // 当前显示的分类，由 SettingsView 的左侧 rail 驱动
    property string category: ""

    property var bitrateSlider: null
    property var videoEnhancementCheck: null

    width: parent ? parent.width : 0
    padding: 0
    spacing: 15

        HardGroupBox {
            id: audioSettingsGroupBox
            visible: settingsPage.category === "audio"
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#39C5BB\">" + qsTr("Audio Settings") + "</font></b>"

            Column {
                anchors.fill: parent
                spacing: 5

                Label {
                    width: parent.width
                    id: resAudioTitle
                    text: qsTr("Audio configuration")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_audio = StreamingPreferences.audioConfig
                        currentIndex = 0
                        for (var i = 0; i < audioListModel.count; i++) {
                            var el_audio = audioListModel.get(i).val;
                            if (saved_audio === el_audio) {
                                currentIndex = i
                                break
                            }
                        }
                        activated(currentIndex)
                    }

                    id: audioComboBox
                    textRole: "text"
                    model: ListModel {
                        id: audioListModel
                        ListElement {
                            text: qsTr("Stereo")
                            val: StreamingPreferences.AC_STEREO
                        }
                        ListElement {
                            text: qsTr("5.1 surround sound")
                            val: StreamingPreferences.AC_51_SURROUND
                        }
                        ListElement {
                            text: qsTr("7.1 surround sound")
                            val: StreamingPreferences.AC_71_SURROUND
                        }
                        ListElement {
                            text: qsTr("7.1.4 surround sound")
                            val: StreamingPreferences.AC_714_SURROUND
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        StreamingPreferences.audioConfig = audioListModel.get(currentIndex).val
                    }
                }


                HardCheckBox {
                    id: audioPcCheck
                    width: parent.width
                    text: qsTr("Mute host PC speakers while streaming")
                    font.pointSize: 12
                    checked: !StreamingPreferences.playAudioOnHost
                    onCheckedChanged: {
                        StreamingPreferences.playAudioOnHost = !checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("You must restart any game currently in progress for this setting to take effect")
                }

                HardCheckBox {
                    id: muteOnFocusLossCheck
                    width: parent.width
                    text: Brand.text(qsTr("Mute audio stream when Moonlight is not the active window"))
                    font.pointSize: 12
                    visible: SystemProperties.hasDesktopEnvironment
                    checked: StreamingPreferences.muteOnFocusLoss
                    onCheckedChanged: {
                        StreamingPreferences.muteOnFocusLoss = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: Brand.text(qsTr("Mutes Moonlight's audio when you Alt+Tab out of the stream or click on a different window."))
                }
                HardCheckBox {
                    id: enableMicCheck
                    width: parent.width
                    text: qsTr("Enable microphone streaming (test)")
                    font.pointSize: 12
                    checked: StreamingPreferences.enableMicrophone
                    onCheckedChanged: {
                        StreamingPreferences.enableMicrophone = checked
                    }
                }
            }
        }

        HardGroupBox {
            id: hostSettingsGroupBox
            visible: settingsPage.category === "host"
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#39C5BB\">" + qsTr("Host Settings") + "</font></b>"

            Column {
                anchors.fill: parent
                spacing: 5

                Label {
                    width: parent.width
                    text: qsTr("Screen Combination Mode")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                Text {
                    width: parent.width
                    text: qsTr("Sunshine Foundation display control")
                    color: Theme.textDim
                    font.family: Theme.fontMono
                    font.pointSize: Theme.fontCaption
                    wrapMode: Text.Wrap
                }

                ScreenCombinationModeSelector {
                    width: parent.width
                }

                HardCheckBox {
                    id: optimizeGameSettingsCheck
                    width: parent.width
                    text: qsTr("Optimize game settings for streaming")
                    font.pointSize:  12
                    checked: StreamingPreferences.gameOptimizations
                    onCheckedChanged: {
                        StreamingPreferences.gameOptimizations = checked
                    }
                }

                HardCheckBox {
                    id: quitAppAfter
                    width: parent.width
                    text: qsTr("Quit app on host PC after ending stream")
                    font.pointSize: 12
                    checked: StreamingPreferences.quitAppAfter
                    onCheckedChanged: {
                        StreamingPreferences.quitAppAfter = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("This will close the app or game you are streaming when you end your stream. You will lose any unsaved progress!")
                }
            }
        }

        HardGroupBox {
            id: uiSettingsGroupBox
            visible: settingsPage.category === "ui"
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#39C5BB\">" + qsTr("UI Settings") + "</font></b>"

            Column {
                anchors.fill: parent
                spacing: 5

                Label {
                    width: parent.width
                    id: languageTitle
                    text: qsTr("Language")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_language = StreamingPreferences.language
                        currentIndex = 0
                        for (var i = 0; i < languageListModel.count; i++) {
                            var el_language = languageListModel.get(i).val;
                            if (saved_language === el_language) {
                                currentIndex = i
                                break
                            }
                        }

                        activated(currentIndex)
                    }

                    id: languageComboBox
                    textRole: "text"
                    model: ListModel {
                        id: languageListModel
                        ListElement {
                            text: qsTr("Automatic")
                            val: StreamingPreferences.LANG_AUTO
                        }
                        ListElement {
                            text: "Deutsch" // German
                            val: StreamingPreferences.LANG_DE
                        }
                        ListElement {
                            text: "English"
                            val: StreamingPreferences.LANG_EN
                        }
                        ListElement {
                            text: "Français" // French
                            val: StreamingPreferences.LANG_FR
                        }
                        ListElement {
                            text: "简体中文" // Simplified Chinese
                            val: StreamingPreferences.LANG_ZH_CN
                        }
                        ListElement {
                            text: "Norwegian Bokmål"
                            val: StreamingPreferences.LANG_NB_NO
                        }
                        ListElement {
                            text: "русский" // Russian
                            val: StreamingPreferences.LANG_RU
                        }
                        ListElement {
                            text: "Español" // Spanish
                            val: StreamingPreferences.LANG_ES
                        }
                        ListElement {
                            text: "日本語" // Japanese
                            val: StreamingPreferences.LANG_JA
                        }
                        ListElement {
                            text: "Tiếng Việt" // Vietnamese
                            val: StreamingPreferences.LANG_VI
                        }
                        ListElement {
                            text: "ภาษาไทย" // Thai
                            val: StreamingPreferences.LANG_TH
                        }
                        ListElement {
                            text: "한국어" // Korean
                            val: StreamingPreferences.LANG_KO
                        }
                        ListElement {
                            text: "Magyar" // Hungarian
                            val: StreamingPreferences.LANG_HU
                        }
                        ListElement {
                            text: "Nederlands" // Dutch
                            val: StreamingPreferences.LANG_NL
                        }
                        ListElement {
                            text: "Svenska" // Swedish
                            val: StreamingPreferences.LANG_SV
                        }
                        ListElement {
                            text: "Türkçe" // Turkish
                            val: StreamingPreferences.LANG_TR
                        }
                        /* ListElement {
                            text: "Українська" // Ukrainian
                            val: StreamingPreferences.LANG_UK
                        } */
                        ListElement {
                            text: "繁體中文" // Traditional Chinese
                            val: StreamingPreferences.LANG_ZH_TW
                        }
                        ListElement {
                            text: "Português" // Portuguese
                            val: StreamingPreferences.LANG_PT
                        }
                        ListElement {
                            text: "Português do Brasil" // Brazilian Portuguese
                            val: StreamingPreferences.LANG_PT_BR
                        }
                        ListElement {
                            text: "Ελληνικά" // Greek
                            val: StreamingPreferences.LANG_EL
                        }
                        ListElement {
                            text: "Italiano" // Italian
                            val: StreamingPreferences.LANG_IT
                        }
                        /* ListElement {
                            text: "हिन्दी, हिंदी" // Hindi
                            val: StreamingPreferences.LANG_HI
                        } */
                        ListElement {
                            text: "Język polski" // Polish
                            val: StreamingPreferences.LANG_PL
                        }
                        ListElement {
                            text: "Čeština" // Czech
                            val: StreamingPreferences.LANG_CS
                        }
                        /* ListElement {
                            text: "עִבְרִית" // Hebrew
                            val: StreamingPreferences.LANG_HE
                        } */
                        /* ListElement {
                            text: "کرمانجیی خواروو" // Central Kurdish
                            val: StreamingPreferences.LANG_CKB
                        } */
                        /* ListElement {
                            text: "Lietuvių kalba" // Lithuanian
                            val: StreamingPreferences.LANG_LT
                        } */
                        /* ListElement {
                            text: "Eesti" // Estonian
                            val: StreamingPreferences.LANG_ET
                        } */
                        ListElement {
                            text: "Български" // Bulgarian
                            val: StreamingPreferences.LANG_BG
                        }
                        /* ListElement {
                            text: "Esperanto"
                            val: StreamingPreferences.LANG_EO
                        } */
                        ListElement {
                            text: "தமிழ்" // Tamil
                            val: StreamingPreferences.LANG_TA
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        // Retranslating is expensive, so only do it if the language actually changed
                        var new_language = languageListModel.get(currentIndex).val
                        if (StreamingPreferences.language !== new_language) {
                            StreamingPreferences.language = languageListModel.get(currentIndex).val
                            if (!StreamingPreferences.retranslate()) {
                                ToolTip.show(Brand.text(qsTr("You must restart Moonlight for this change to take effect")), 5000)
                            }
                            else {
                                // Force the back operation to pop any AppView pages that exist.
                                // The AppView stops working after retranslate() for some reason.
                                window.clearOnBack = true

                                // Signal other controls to adjust their text
                                languageChanged()
                            }
                        }
                    }
                }

                Label {
                    width: parent.width
                    id: uiDisplayModeTitle
                    text: qsTr("GUI display mode")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                    visible: SystemProperties.hasDesktopEnvironment
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        // 注意不能用 visible：设置页按分类切换后，非当前分类的控件
                        // 创建时 visible 就是假的，用它做判断会导致这里永远读不到已保存的值。
                        if (!SystemProperties.hasDesktopEnvironment) {
                            // Do nothing if the control won't even be visible
                            return
                        }

                        var saved_uidisplaymode = StreamingPreferences.uiDisplayMode
                        currentIndex = 0
                        for (var i = 0; i < uiDisplayModeListModel.count; i++) {
                            var el_uidisplaymode = uiDisplayModeListModel.get(i).val;
                            if (saved_uidisplaymode === el_uidisplaymode) {
                                currentIndex = i
                                break
                            }
                        }

                        activated(currentIndex)
                    }

                    id: uiDisplayModeComboBox
                    visible: SystemProperties.hasDesktopEnvironment
                    textRole: "text"
                    model: ListModel {
                        id: uiDisplayModeListModel
                        ListElement {
                            text: qsTr("Windowed")
                            val: StreamingPreferences.UI_WINDOWED
                        }
                        ListElement {
                            text: qsTr("Maximized")
                            val: StreamingPreferences.UI_MAXIMIZED
                        }
                        ListElement {
                            text: qsTr("Fullscreen")
                            val: StreamingPreferences.UI_FULLSCREEN
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        StreamingPreferences.uiDisplayMode = uiDisplayModeListModel.get(currentIndex).val
                    }
                }

                ToggleRow {
                    applicable: SystemProperties.hasDesktopEnvironment &&
                                (!SystemProperties.isRunningWayland || SystemProperties.isRunningXWayland)
                    title: qsTr("Remember window position and size")
                    checked: StreamingPreferences.rememberWindowPosition
                    onToggled: function(value) {
                        StreamingPreferences.rememberWindowPosition = value
                    }
                }

                HardCheckBox {
                    id: connectionWarningsCheck
                    width: parent.width
                    text: qsTr("Show connection quality warnings")
                    font.pointSize: 12
                    checked: StreamingPreferences.connectionWarnings
                    onCheckedChanged: {
                        StreamingPreferences.connectionWarnings = checked
                    }
                }

                HardCheckBox {
                    id: configurationWarningsCheck
                    width: parent.width
                    text: qsTr("Show configuration warnings")
                    font.pointSize: 12
                    checked: StreamingPreferences.configurationWarnings
                    onCheckedChanged: {
                        StreamingPreferences.configurationWarnings = checked
                    }
                }

                HardCheckBox {
                    visible: SystemProperties.hasDiscordIntegration
                    id: discordPresenceCheck
                    width: parent.width
                    text: qsTr("Discord Rich Presence integration")
                    font.pointSize: 12
                    checked: StreamingPreferences.richPresence
                    onCheckedChanged: {
                        StreamingPreferences.richPresence = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Updates your Discord status to display the name of the game you're streaming.")
                }

                HardCheckBox {
                    id: keepAwakeCheck
                    width: parent.width
                    text: qsTr("Keep the display awake while streaming")
                    font.pointSize: 12
                    checked: StreamingPreferences.keepAwake
                    onCheckedChanged: {
                        StreamingPreferences.keepAwake = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Prevents the screensaver from starting or the display from going to sleep while streaming.")
                }

                HardCheckBox {
                    id: autoUpdateCheckBox
                    width: parent.width
                    text: qsTr("Automatically check for updates")
                    font.pointSize: 12
                    checked: StreamingPreferences.autoUpdateCheck
                    onCheckedChanged: {
                        StreamingPreferences.autoUpdateCheck = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: Brand.text(qsTr("Check for new versions of Moonlight when the app starts."))
                }

                Label {
                    width: parent.width
                    text: qsTr("Overlay menu position")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    Component.onCompleted: {
                        var saved = StreamingPreferences.overlayMenuPosition
                        currentIndex = 0
                        for (var i = 0; i < overlayMenuModel.count; i++) {
                            if (overlayMenuModel.get(i).val === saved) {
                                currentIndex = i
                                break
                            }
                        }
                        activated(currentIndex)
                    }

                    id: overlayMenuComboBox
                    textRole: "text"
                    model: ListModel {
                        id: overlayMenuModel
                        ListElement {
                            text: qsTr("Right edge (default)")
                            val: StreamingPreferences.OMP_RIGHT_EDGE
                        }
                        ListElement {
                            text: qsTr("Left edge")
                            val: StreamingPreferences.OMP_LEFT_EDGE
                        }
                        ListElement {
                            text: qsTr("Floating button")
                            val: StreamingPreferences.OMP_BUTTON
                        }
                        ListElement {
                            text: qsTr("Disabled")
                            val: StreamingPreferences.OMP_DISABLED
                        }
                    }
                    onActivated: {
                        StreamingPreferences.overlayMenuPosition = overlayMenuModel.get(currentIndex).val
                    }
                }
            }
        }

        HardGroupBox {
            id: inputSettingsGroupBox
            visible: settingsPage.category === "input"
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#39C5BB\">" + qsTr("Input Settings") + "</font></b>"

            Column {
                anchors.fill: parent
                spacing: 5

                HardCheckBox {
                    id: absoluteMouseCheck
                    hoverEnabled: true
                    width: parent.width
                    text: qsTr("Optimize mouse for remote desktop instead of games")
                    font.pointSize:  12
                    checked: StreamingPreferences.absoluteMouseMode
                    onCheckedChanged: {
                        StreamingPreferences.absoluteMouseMode = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 10000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("This enables seamless mouse control without capturing the client's mouse cursor. It is ideal for remote desktop usage but will not work in most games.") + " " +
                                  qsTr("You can toggle this while streaming using Ctrl+Alt+Shift+M.") + "\n\n" +
                                  qsTr("NOTE: Due to a bug in GeForce Experience, this option may not work properly if your host PC has multiple monitors.")
                }

                HardCheckBox {
                    id: showLocalCursorCheck
                    hoverEnabled: true
                    width: parent.width
                    text: qsTr("Show local cursor")
                    font.pointSize:  12
                    checked: StreamingPreferences.showLocalCursor
                    onCheckedChanged: {
                        StreamingPreferences.showLocalCursor = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 10000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("This makes the client's mouse cursor visible in the stream.") + " " +
                                  qsTr("You can toggle this while streaming using Ctrl+Alt+Shift+C.")
                }

                Row {
                    spacing: 5
                    width: parent.width

                    HardCheckBox {
                        id: captureSysKeysCheck
                        hoverEnabled: true
                        text: qsTr("Capture system keyboard shortcuts")
                        font.pointSize: 12
                        enabled: SystemProperties.hasDesktopEnvironment
                        checked: StreamingPreferences.captureSysKeysMode !== StreamingPreferences.CSK_OFF || !SystemProperties.hasDesktopEnvironment

                        ToolTip.delay: 1000
                        ToolTip.timeout: 10000
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("This enables the capture of system-wide keyboard shortcuts like Alt+Tab that would normally be handled by the client OS while streaming.") + "\n\n" +
                                      Brand.text(qsTr("NOTE: Certain keyboard shortcuts like Ctrl+Alt+Del on Windows cannot be intercepted by any application, including Moonlight."))
                    }

                    AutoResizingComboBox {
                        // ignore setting the index at first, and actually set it when the component is loaded
                        Component.onCompleted: {
                            // 同上：不能用 visible 判断，分类切换会让它在创建时为假。
                            if (!SystemProperties.hasDesktopEnvironment) {
                                // Do nothing if the control won't even be visible
                                return
                            }

                            var saved_syskeysmode = StreamingPreferences.captureSysKeysMode
                            currentIndex = 0
                            for (var i = 0; i < captureSysKeysModeListModel.count; i++) {
                                var el_syskeysmode = captureSysKeysModeListModel.get(i).val;
                                if (saved_syskeysmode === el_syskeysmode) {
                                    currentIndex = i
                                    break
                                }
                            }

                            activated(currentIndex)
                        }

                        enabled: captureSysKeysCheck.checked && captureSysKeysCheck.enabled
                        textRole: "text"
                        model: ListModel {
                            id: captureSysKeysModeListModel
                            ListElement {
                                text: qsTr("in fullscreen")
                                val: StreamingPreferences.CSK_FULLSCREEN
                            }
                            ListElement {
                                text: qsTr("always")
                                val: StreamingPreferences.CSK_ALWAYS
                            }
                        }

                        function updatePref() {
                            if (!enabled) {
                                StreamingPreferences.captureSysKeysMode = StreamingPreferences.CSK_OFF
                            }
                            else {
                                StreamingPreferences.captureSysKeysMode = captureSysKeysModeListModel.get(currentIndex).val
                            }
                        }

                        // ::onActivated must be used, as it only listens for when the index is changed by a human
                        onActivated: {
                            updatePref()
                        }

                        // This handles transition of the checkbox state
                        onEnabledChanged: {
                            updatePref()
                        }
                    }
                }

                HardCheckBox {
                    id: absoluteTouchCheck
                    hoverEnabled: true
                    width: parent.width
                    text: qsTr("Use touchscreen as a virtual trackpad")
                    font.pointSize:  12
                    checked: !StreamingPreferences.absoluteTouchMode
                    onCheckedChanged: {
                        StreamingPreferences.absoluteTouchMode = !checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("When checked, the touchscreen acts like a trackpad. When unchecked, the touchscreen will directly control the mouse pointer.")
                }

                HardCheckBox {
                    id: nativeTouchpadCheck
                    hoverEnabled: true
                    width: parent.width
                    text: qsTr("Use precision touchpad input when available")
                    font.pointSize: 12
                    checked: StreamingPreferences.enableNativeTouchpad
                    onCheckedChanged: {
                        StreamingPreferences.enableNativeTouchpad = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 10000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Sends native multi-touch trackpad contacts to compatible Sunshine hosts. Unsupported devices and hosts fall back to pointer input. Changes apply to the next stream.")
                }

                Row {
                    spacing: 5
                    width: parent.width

                    Text {
                        text: qsTr("DualSense haptics")
                        font.pointSize: 12
                        color: "white"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    AutoResizingComboBox {
                        id: dualSenseHapticsModeComboBox
                        textRole: "text"
                        model: ListModel {
                            id: dualSenseHapticsModeListModel
                            ListElement {
                                text: qsTr("Physical DualSense (native HD haptics)")
                                val: StreamingPreferences.DSHM_PHYSICAL
                            }
                            ListElement {
                                text: qsTr("Simulated DualSense (analyzed vibration)")
                                val: StreamingPreferences.DSHM_EMULATED
                            }
                        }
                        Component.onCompleted: {
                            if (Qt.platform.os !== "windows") {
                                dualSenseHapticsModeListModel.remove(0)
                            }
                            for (var i = 0; i < dualSenseHapticsModeListModel.count; i++) {
                                if (dualSenseHapticsModeListModel.get(i).val === StreamingPreferences.dualSenseHapticsMode) {
                                    currentIndex = i
                                    break
                                }
                            }
                        }
                        onActivated: {
                            StreamingPreferences.dualSenseHapticsMode = dualSenseHapticsModeListModel.get(currentIndex).val
                        }

                        ToolTip.delay: 1000
                        ToolTip.timeout: 10000
                        ToolTip.visible: hovered
                        ToolTip.text: currentIndex >= 0 &&
                                      dualSenseHapticsModeListModel.get(currentIndex).val === StreamingPreferences.DSHM_PHYSICAL ?
                            qsTr("Sends the original authored PCM to channels 3 and 4 of a USB-connected DualSense. The endpoint is checked before connecting. Changes apply to the next stream.") :
                            qsTr("Receives a compact analyzed haptics signal and renders it through the connected controller's vibration motors. Changes apply to the next stream.")
                    }
                }

                HardCheckBox {
                    id: swapMouseButtonsCheck
                    hoverEnabled: true
                    width: parent.width
                    text: qsTr("Swap left and right mouse buttons")
                    font.pointSize:  12
                    checked: StreamingPreferences.swapMouseButtons
                    onCheckedChanged: {
                        StreamingPreferences.swapMouseButtons = checked
                    }
                }

                HardCheckBox {
                    id: swapWinAltKeysCheck
                    hoverEnabled: true
                    width: parent.width
                    text: qsTr("Swap Alt and Win keys")
                    font.pointSize:  12
                    checked: StreamingPreferences.swapWinAltKeys
                    onCheckedChanged: {
                        StreamingPreferences.swapWinAltKeys = checked
                    }
                }

                HardCheckBox {
                    id: reverseScrollButtonsCheck
                    hoverEnabled: true
                    width: parent.width
                    text: qsTr("Reverse mouse scrolling direction")
                    font.pointSize: 12
                    checked: StreamingPreferences.reverseScrollDirection
                    onCheckedChanged: {
                        StreamingPreferences.reverseScrollDirection = checked
                    }
                }
            }
        }

        HardGroupBox {
            id: gamepadSettingsGroupBox
            visible: settingsPage.category === "gamepad"
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#39C5BB\">" + qsTr("Gamepad Settings") + "</font></b>"

            Column {
                anchors.fill: parent
                spacing: 5

                HardCheckBox {
                    id: swapFaceButtonsCheck
                    width: parent.width
                    text: qsTr("Swap A/B and X/Y gamepad buttons")
                    font.pointSize: 12
                    checked: StreamingPreferences.swapFaceButtons
                    onCheckedChanged: {
                        StreamingPreferences.swapFaceButtons = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("This switches gamepads into a Nintendo-style button layout")
                }

                HardCheckBox {
                    id: singleControllerCheck
                    width: parent.width
                    text: qsTr("Force gamepad #1 always connected")
                    font.pointSize:  12
                    checked: !StreamingPreferences.multiController
                    onCheckedChanged: {
                        StreamingPreferences.multiController = !checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Forces a single gamepad to always stay connected to the host, even if no gamepads are actually connected to this PC.") + " " +
                                  qsTr("Only enable this option when streaming a game that doesn't support gamepads being connected after startup.")
                }

                HardCheckBox {
                    id: gamepadMouseCheck
                    hoverEnabled: true
                    width: parent.width
                    text: qsTr("Enable mouse control with gamepads by holding the 'Start' button")
                    font.pointSize: 12
                    checked: StreamingPreferences.gamepadMouse
                    onCheckedChanged: {
                        StreamingPreferences.gamepadMouse = checked
                    }
                }

                HardCheckBox {
                    id: backgroundGamepadCheck
                    width: parent.width
                    text: Brand.text(qsTr("Process gamepad input when Moonlight is in the background"))
                    font.pointSize: 12
                    visible: SystemProperties.hasDesktopEnvironment
                    checked: StreamingPreferences.backgroundGamepad
                    onCheckedChanged: {
                        StreamingPreferences.backgroundGamepad = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: Brand.text(qsTr("Allows Moonlight to capture gamepad inputs even if it's not the current window in focus"))
                }

                Label {
                    width: parent.width
                    text: qsTr("Gamepad quit combo")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    Component.onCompleted: {
                        var saved_combo = StreamingPreferences.gamepadQuitCombo
                        currentIndex = 0
                        for (var i = 0; i < gamepadQuitComboListModel.count; i++) {
                            if (saved_combo === gamepadQuitComboListModel.get(i).val) {
                                currentIndex = i
                                break
                            }
                        }
                        activated(currentIndex)
                    }

                    id: gamepadQuitComboComboBox
                    textRole: "text"
                    model: ListModel {
                        id: gamepadQuitComboListModel
                        ListElement {
                            text: qsTr("Start + Select + L1 + R1 (Default)")
                            val: StreamingPreferences.GQC_DEFAULT
                        }
                        ListElement {
                            text: qsTr("Select + L1 + R1 + X")
                            val: StreamingPreferences.GQC_SELECT_L1_R1_X
                        }
                        ListElement {
                            text: qsTr("Select + L1 + R1 + Y")
                            val: StreamingPreferences.GQC_SELECT_L1_R1_Y
                        }
                        ListElement {
                            text: qsTr("Start + L1 + R1 + A")
                            val: StreamingPreferences.GQC_START_L1_R1_A
                        }
                        ListElement {
                            text: qsTr("Start + L1 + R1 + B")
                            val: StreamingPreferences.GQC_START_L1_R1_B
                        }
                        ListElement {
                            text: qsTr("L1 + R1 + X + Y")
                            val: StreamingPreferences.GQC_L1_R1_X_Y
                        }
                        ListElement {
                            text: qsTr("L1 + R1 + A + B")
                            val: StreamingPreferences.GQC_L1_R1_A_B
                        }
                    }

                    onActivated: {
                        StreamingPreferences.gamepadQuitCombo = gamepadQuitComboListModel.get(currentIndex).val
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Choose which button combination exits streaming. Use alternatives if the default doesn't work on your device.")
                }
            }
        }

        HardGroupBox {
            id: advancedSettingsGroupBox
            visible: settingsPage.category === "advanced"
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#39C5BB\">" + qsTr("Advanced Settings") + "</font></b>"

            Column {
                anchors.fill: parent
                spacing: 5

                Label {
                    width: parent.width
                    id: resVDSTitle
                    text: qsTr("Video decoder")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_vds = StreamingPreferences.videoDecoderSelection
                        currentIndex = 0
                        for (var i = 0; i < decoderListModel.count; i++) {
                            var el_vds = decoderListModel.get(i).val;
                            if (saved_vds === el_vds) {
                                currentIndex = i
                                break
                            }
                        }
                        activated(currentIndex)
                    }

                    id: decoderComboBox
                    textRole: "text"
                    model: ListModel {
                        id: decoderListModel
                        ListElement {
                            text: qsTr("Automatic (Recommended)")
                            val: StreamingPreferences.VDS_AUTO
                        }
                        ListElement {
                            text: qsTr("Force software decoding")
                            val: StreamingPreferences.VDS_FORCE_SOFTWARE
                        }
                        ListElement {
                            text: qsTr("Force hardware decoding")
                            val: StreamingPreferences.VDS_FORCE_HARDWARE
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated: {
                        if (enabled) {
                            StreamingPreferences.videoDecoderSelection = decoderListModel.get(currentIndex).val
                        }
                    }
                    onCurrentIndexChanged: {
                        // 基本设置页在另一个文件里，创建顺序不保证，先判空
                        if (!videoEnhancementCheck) {
                            return;
                        }
                        if(decoderListModel.get(currentIndex).val === StreamingPreferences.VDS_FORCE_SOFTWARE){
                            videoEnhancementCheck.enabled = false;
                            videoEnhancementCheck.keepValue = videoEnhancementCheck.checked;
                            videoEnhancementCheck.checked = false;
                        } else {
                            videoEnhancementCheck.enabled = true;
                            videoEnhancementCheck.checked = videoEnhancementCheck.keepValue;
                        }
                    }
                }

                Label {
                    width: parent.width
                    id: resVCCTitle
                    text: qsTr("Video codec")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_vcc = StreamingPreferences.videoCodecConfig

                        // Default to Automatic (relevant if HDR is enabled,
                        // where we will match none of the codecs in the list)
                        currentIndex = 0

                        for(var i = 0; i < codecListModel.count; i++) {
                            var el_vcc = codecListModel.get(i).val;
                            if (saved_vcc === el_vcc) {
                                currentIndex = i
                                break
                            }
                        }

                        activated(currentIndex)
                    }

                    id: codecComboBox
                    textRole: "text"
                    model: ListModel {
                        id: codecListModel
                        ListElement {
                            text: qsTr("Automatic (Recommended)")
                            val: StreamingPreferences.VCC_AUTO
                        }
                        ListElement {
                            text: qsTr("H.264")
                            val: StreamingPreferences.VCC_FORCE_H264
                        }
                        ListElement {
                            text: qsTr("HEVC (H.265)")
                            val: StreamingPreferences.VCC_FORCE_HEVC
                        }
                        ListElement {
                            text: qsTr("AV1")
                            val: StreamingPreferences.VCC_FORCE_AV1
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        if (enabled) {
                            StreamingPreferences.videoCodecConfig = codecListModel.get(currentIndex).val
                        }
                    }
                }

                Label {
                    width: parent.width
                    id: rendererTitle
                    text: qsTr("Renderer")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                    visible: SystemProperties.isDarwin
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_rs = StreamingPreferences.rendererSelection

                        // Default to Automatic
                        currentIndex = 0

                        for(var i = 0; i < rendererListModel.count; i++) {
                            var el_rs = rendererListModel.get(i).val;
                            if (saved_rs === el_rs) {
                                currentIndex = i
                                break
                            }
                        }

                        activated(currentIndex)
                    }

                    id: rendererComboBox
                    visible: SystemProperties.isDarwin
                    textRole: "text"
                    model: ListModel {
                        id: rendererListModel
                        ListElement {
                            text: qsTr("Automatic (Recommended)")
                            val: StreamingPreferences.RS_AUTO
                        }
                        ListElement {
                            text: "Vulkan"
                            val: StreamingPreferences.RS_VULKAN
                        }
                        ListElement {
                            text: "Metal"
                            val: StreamingPreferences.RS_METAL
                        }
                        ListElement {
                            text: "AVSampleBufferDisplayLayer"
                            val: StreamingPreferences.RS_AVSBDL
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        StreamingPreferences.rendererSelection = rendererListModel.get(currentIndex).val
                    }
                }

                HardCheckBox {
                    id: enableYUV444
                    width: parent.width
                    text: qsTr("Enable YUV 4:4:4")
                    font.pointSize: 12

                    checked: StreamingPreferences.enableYUV444
                    onCheckedChanged: {
                        // This is called on init, so only reset to default bitrate when checked state changes.
                        if (StreamingPreferences.enableYUV444 != checked) {
                            StreamingPreferences.enableYUV444 = checked
                            if (StreamingPreferences.autoAdjustBitrate) {
                                StreamingPreferences.bitrateKbps = StreamingPreferences.getDefaultBitrate(StreamingPreferences.width,
                                                                                                          StreamingPreferences.height,
                                                                                                          StreamingPreferences.fps,
                                                                                                          StreamingPreferences.enableYUV444);
                                if (bitrateSlider) {
                                    bitrateSlider.value = Math.log(StreamingPreferences.bitrateKbps)
                                }
                            }
                        }
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: enabled ?
                                      qsTr("Good for streaming desktop and text-heavy games, but not recommended for fast-paced games.")
                                    :
                                      qsTr("YUV 4:4:4 is not supported on this PC.")
                }

                HardCheckBox {
                    id: enableMdns
                    width: parent.width
                    text: qsTr("Automatically find PCs on the local network (Recommended)")
                    font.pointSize: 12
                    checked: StreamingPreferences.enableMdns
                    onCheckedChanged: {
                        // This is called on init, so only do the work if we've
                        // actually changed the value.
                        if (StreamingPreferences.enableMdns != checked) {
                            StreamingPreferences.enableMdns = checked

                            // Restart polling so the mDNS change takes effect
                            if (window.pollingActive) {
                                ComputerManager.stopPollingAsync()
                                ComputerManager.startPolling()
                            }
                        }
                    }
                }

                HardCheckBox {
                    id: detectNetworkBlocking
                    width: parent.width
                    text: qsTr("Automatically detect blocked connections (Recommended)")
                    font.pointSize: 12
                    checked: StreamingPreferences.detectNetworkBlocking
                    onCheckedChanged: {
                        StreamingPreferences.detectNetworkBlocking = checked
                    }
                }

                HardCheckBox {
                    id: showPerformanceOverlay
                    width: parent.width
                    text: qsTr("Show performance stats while streaming")
                    font.pointSize: 12
                    checked: StreamingPreferences.showPerformanceOverlay
                    onCheckedChanged: {
                        StreamingPreferences.showPerformanceOverlay = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Display real-time stream performance information while streaming.") + "\n\n" +
                                  qsTr("You can toggle it at any time while streaming using Ctrl+Alt+Shift+S or Select+L1+R1+X.") + "\n\n" +
                                  qsTr("The performance overlay is not supported on Steam Link or Raspberry Pi.")
                }
            }
        }
}
