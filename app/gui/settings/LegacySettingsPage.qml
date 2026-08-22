import QtQuick 2.9
import QtQuick.Controls
import "."
import ".."
import "../theme"
import "../Brand.js" as Brand

import StreamingPreferences 1.0
import ComputerManager 1.0
import SystemProperties 1.0

// 这六个分类保留在同一文件中，以维持现有 LegacySettingsPage 翻译上下文。
// 视觉和交互已经迁移到 SettingsCard / SettingsRow / ToggleRow。
Column {
    id: settingsPage

    signal languageChanged()
    signal bitratePreferenceChanged()

    property string category: ""
    // 关闭捕获时保留用户上次选择的作用范围，重新开启后恢复原模式。
    property int captureSysKeysSelection: StreamingPreferences.CSK_FULLSCREEN

    width: parent ? parent.width : 0
    spacing: Theme.spaceLg

    // ================= 音频 =================
    SettingsCard {
        visible: settingsPage.category === "audio" && hasVisibleContent
        title: qsTr("Audio Settings")

        ChoiceRow {
            title: qsTr("Audio configuration")
            selectedValue: StreamingPreferences.audioConfig
            onValueActivated: function(value) { StreamingPreferences.audioConfig = value }

            model: ListModel {
                ListElement { text: qsTr("Stereo"); val: StreamingPreferences.AC_STEREO }
                ListElement { text: qsTr("5.1 surround sound"); val: StreamingPreferences.AC_51_SURROUND }
                ListElement { text: qsTr("7.1 surround sound"); val: StreamingPreferences.AC_71_SURROUND }
                ListElement { text: qsTr("7.1.4 surround sound"); val: StreamingPreferences.AC_714_SURROUND }
            }
        }

        ToggleRow {
            title: qsTr("Mute host PC speakers while streaming")
            description: qsTr("You must restart any game currently in progress for this setting to take effect")
            checked: !StreamingPreferences.playAudioOnHost
            onToggled: function(value) { StreamingPreferences.playAudioOnHost = !value }
        }

        ToggleRow {
            applicable: SystemProperties.hasDesktopEnvironment
            title: Brand.text(qsTr("Mute audio stream when Moonlight is not the active window"))
            description: Brand.text(qsTr("Mutes Moonlight's audio when you Alt+Tab out of the stream or click on a different window."))
            checked: StreamingPreferences.muteOnFocusLoss
            onToggled: function(value) { StreamingPreferences.muteOnFocusLoss = value }
        }

        ToggleRow {
            title: qsTr("Enable microphone streaming (test)")
            checked: StreamingPreferences.enableMicrophone
            onToggled: function(value) { StreamingPreferences.enableMicrophone = value }
        }
    }

    // ================= 主机 =================
    SettingsCard {
        visible: settingsPage.category === "host" && hasVisibleContent
        title: qsTr("Screen Combination Mode")
        subtitle: qsTr("Sunshine Foundation display control")

        ScreenCombinationModeSelector {
            width: parent.width
        }
    }

    SettingsCard {
        visible: settingsPage.category === "host" && hasVisibleContent
        title: qsTr("Host Settings")

        ToggleRow {
            title: qsTr("Optimize game settings for streaming")
            checked: StreamingPreferences.gameOptimizations
            onToggled: function(value) { StreamingPreferences.gameOptimizations = value }
        }

        ToggleRow {
            title: qsTr("Quit app on host PC after ending stream")
            description: qsTr("This will close the app or game you are streaming when you end your stream. You will lose any unsaved progress!")
            checked: StreamingPreferences.quitAppAfter
            onToggled: function(value) { StreamingPreferences.quitAppAfter = value }
        }
    }

    // ================= 界面 =================
    SettingsCard {
        visible: settingsPage.category === "ui" && hasVisibleContent
        title: qsTr("UI Settings")

        ChoiceRow {
            id: languageRow
            title: qsTr("Language")
            maximumControlWidth: 260
            selectedValue: StreamingPreferences.language
            onValueActivated: function(value) {
                if (StreamingPreferences.language === value) {
                    return
                }

                StreamingPreferences.language = value
                if (!StreamingPreferences.retranslate()) {
                    ToolTip.show(Brand.text(qsTr("You must restart Moonlight for this change to take effect")), 5000)
                }
                else {
                    window.clearOnBack = true
                    settingsPage.languageChanged()
                    languageRow.syncSelection()
                }
            }

            model: ListModel {
                ListElement { text: qsTr("Automatic"); val: StreamingPreferences.LANG_AUTO }
                ListElement { text: "Deutsch"; val: StreamingPreferences.LANG_DE }
                ListElement { text: "English"; val: StreamingPreferences.LANG_EN }
                ListElement { text: "Français"; val: StreamingPreferences.LANG_FR }
                ListElement { text: "简体中文"; val: StreamingPreferences.LANG_ZH_CN }
                ListElement { text: "Norwegian Bokmål"; val: StreamingPreferences.LANG_NB_NO }
                ListElement { text: "русский"; val: StreamingPreferences.LANG_RU }
                ListElement { text: "Español"; val: StreamingPreferences.LANG_ES }
                ListElement { text: "日本語"; val: StreamingPreferences.LANG_JA }
                ListElement { text: "Tiếng Việt"; val: StreamingPreferences.LANG_VI }
                ListElement { text: "ภาษาไทย"; val: StreamingPreferences.LANG_TH }
                ListElement { text: "한국어"; val: StreamingPreferences.LANG_KO }
                ListElement { text: "Magyar"; val: StreamingPreferences.LANG_HU }
                ListElement { text: "Nederlands"; val: StreamingPreferences.LANG_NL }
                ListElement { text: "Svenska"; val: StreamingPreferences.LANG_SV }
                ListElement { text: "Türkçe"; val: StreamingPreferences.LANG_TR }
                ListElement { text: "繁體中文"; val: StreamingPreferences.LANG_ZH_TW }
                ListElement { text: "Português"; val: StreamingPreferences.LANG_PT }
                ListElement { text: "Português do Brasil"; val: StreamingPreferences.LANG_PT_BR }
                ListElement { text: "Ελληνικά"; val: StreamingPreferences.LANG_EL }
                ListElement { text: "Italiano"; val: StreamingPreferences.LANG_IT }
                ListElement { text: "Język polski"; val: StreamingPreferences.LANG_PL }
                ListElement { text: "Čeština"; val: StreamingPreferences.LANG_CS }
                ListElement { text: "Български"; val: StreamingPreferences.LANG_BG }
                ListElement { text: "தமிழ்"; val: StreamingPreferences.LANG_TA }
            }
        }

        ChoiceRow {
            applicable: SystemProperties.hasDesktopEnvironment
            title: qsTr("GUI display mode")
            selectedValue: StreamingPreferences.uiDisplayMode
            onValueActivated: function(value) { StreamingPreferences.uiDisplayMode = value }

            model: ListModel {
                ListElement { text: qsTr("Windowed"); val: StreamingPreferences.UI_WINDOWED }
                ListElement { text: qsTr("Maximized"); val: StreamingPreferences.UI_MAXIMIZED }
                ListElement { text: qsTr("Fullscreen"); val: StreamingPreferences.UI_FULLSCREEN }
            }
        }

        ToggleRow {
            applicable: SystemProperties.hasDesktopEnvironment &&
                        (!SystemProperties.isRunningWayland || SystemProperties.isRunningXWayland)
            title: qsTr("Remember window position and size")
            checked: StreamingPreferences.rememberWindowPosition
            onToggled: function(value) { StreamingPreferences.rememberWindowPosition = value }
        }
    }

    SettingsCard {
        visible: settingsPage.category === "ui" && hasVisibleContent
        title: qsTr("Notifications and integrations")

        ToggleRow {
            title: qsTr("Show connection quality warnings")
            checked: StreamingPreferences.connectionWarnings
            onToggled: function(value) { StreamingPreferences.connectionWarnings = value }
        }

        ToggleRow {
            title: qsTr("Show configuration warnings")
            checked: StreamingPreferences.configurationWarnings
            onToggled: function(value) { StreamingPreferences.configurationWarnings = value }
        }

        ToggleRow {
            applicable: SystemProperties.hasDiscordIntegration
            title: qsTr("Discord Rich Presence integration")
            description: qsTr("Updates your Discord status to display the name of the game you're streaming.")
            checked: StreamingPreferences.richPresence
            onToggled: function(value) { StreamingPreferences.richPresence = value }
        }

        ToggleRow {
            title: qsTr("Keep the display awake while streaming")
            description: qsTr("Prevents the screensaver from starting or the display from going to sleep while streaming.")
            checked: StreamingPreferences.keepAwake
            onToggled: function(value) { StreamingPreferences.keepAwake = value }
        }

        ToggleRow {
            title: qsTr("Automatically check for updates")
            description: Brand.text(qsTr("Check for new versions of Moonlight when the app starts."))
            checked: StreamingPreferences.autoUpdateCheck
            onToggled: function(value) { StreamingPreferences.autoUpdateCheck = value }
        }
    }

    // ================= 输入 =================
    SettingsCard {
        visible: settingsPage.category === "input" && hasVisibleContent
        title: qsTr("Mouse")

        ToggleRow {
            title: qsTr("Optimize mouse for remote desktop instead of games")
            description: qsTr("This enables seamless mouse control without capturing the client's mouse cursor. It is ideal for remote desktop usage but will not work in most games.") + " " +
                         qsTr("You can toggle this while streaming using Ctrl+Alt+Shift+M.") + "\n\n" +
                         qsTr("NOTE: Due to a bug in GeForce Experience, this option may not work properly if your host PC has multiple monitors.")
            checked: StreamingPreferences.absoluteMouseMode
            onToggled: function(value) { StreamingPreferences.absoluteMouseMode = value }
        }

        ToggleRow {
            title: qsTr("Show local cursor")
            description: qsTr("This makes the client's mouse cursor visible in the stream.") + " " +
                         qsTr("You can toggle this while streaming using Ctrl+Alt+Shift+C.")
            checked: StreamingPreferences.showLocalCursor
            onToggled: function(value) { StreamingPreferences.showLocalCursor = value }
        }

        ToggleRow {
            title: qsTr("Swap left and right mouse buttons")
            checked: StreamingPreferences.swapMouseButtons
            onToggled: function(value) { StreamingPreferences.swapMouseButtons = value }
        }

        ToggleRow {
            title: qsTr("Swap Alt and Win keys")
            checked: StreamingPreferences.swapWinAltKeys
            onToggled: function(value) { StreamingPreferences.swapWinAltKeys = value }
        }

        ToggleRow {
            title: qsTr("Reverse mouse scrolling direction")
            checked: StreamingPreferences.reverseScrollDirection
            onToggled: function(value) { StreamingPreferences.reverseScrollDirection = value }
        }
    }

    SettingsCard {
        visible: settingsPage.category === "input" && hasVisibleContent
        title: qsTr("Keyboard")

        ToggleRow {
            id: captureSysKeysToggle
            applicable: SystemProperties.hasDesktopEnvironment
            title: qsTr("Capture system keyboard shortcuts")
            description: qsTr("This enables the capture of system-wide keyboard shortcuts like Alt+Tab that would normally be handled by the client OS while streaming.") + "\n\n" +
                         Brand.text(qsTr("NOTE: Certain keyboard shortcuts like Ctrl+Alt+Del on Windows cannot be intercepted by any application, including Moonlight."))
            checked: StreamingPreferences.captureSysKeysMode !== StreamingPreferences.CSK_OFF
            onToggled: function(value) {
                StreamingPreferences.captureSysKeysMode = value
                    ? settingsPage.captureSysKeysSelection
                    : StreamingPreferences.CSK_OFF
            }
        }

        ChoiceRow {
            applicable: SystemProperties.hasDesktopEnvironment
            title: qsTr("Capture mode")
            description: qsTr("Choose when system keyboard shortcuts are captured.")
            controlEnabled: captureSysKeysToggle.checked
            selectedValue: settingsPage.captureSysKeysSelection
            onValueActivated: function(value) {
                settingsPage.captureSysKeysSelection = value
                if (captureSysKeysToggle.checked) {
                    StreamingPreferences.captureSysKeysMode = value
                }
            }

            model: ListModel {
                ListElement { text: qsTr("in fullscreen"); val: StreamingPreferences.CSK_FULLSCREEN }
                ListElement { text: qsTr("always"); val: StreamingPreferences.CSK_ALWAYS }
            }
        }
    }

    SettingsCard {
        visible: settingsPage.category === "input" && hasVisibleContent
        title: qsTr("Touch input")

        ToggleRow {
            title: qsTr("Use touchscreen as a virtual trackpad")
            description: qsTr("When checked, the touchscreen acts like a trackpad. When unchecked, the touchscreen will directly control the mouse pointer.")
            checked: !StreamingPreferences.absoluteTouchMode
            onToggled: function(value) { StreamingPreferences.absoluteTouchMode = !value }
        }

        ToggleRow {
            title: qsTr("Use precision touchpad input when available")
            description: qsTr("Sends native multi-touch trackpad contacts to compatible Sunshine hosts. Unsupported devices and hosts fall back to pointer input. Changes apply to the next stream.")
            checked: StreamingPreferences.enableNativeTouchpad
            onToggled: function(value) { StreamingPreferences.enableNativeTouchpad = value }
        }
    }

    SettingsCard {
        visible: settingsPage.category === "input" && hasVisibleContent
        title: qsTr("DualSense haptics")

        ChoiceRow {
            id: dualSenseHapticsRow
            title: qsTr("DualSense haptics")
            description: selectedValue === StreamingPreferences.DSHM_PHYSICAL
                ? qsTr("Sends the original authored PCM to channels 3 and 4 of a USB-connected DualSense. The endpoint is checked before connecting. Changes apply to the next stream.")
                : qsTr("Receives a compact analyzed haptics signal and renders it through the connected controller's vibration motors. Changes apply to the next stream.")
            selectedValue: StreamingPreferences.dualSenseHapticsMode
            onValueActivated: function(value) { StreamingPreferences.dualSenseHapticsMode = value }

            model: ListModel {
                id: dualSenseHapticsModeListModel
                ListElement { text: qsTr("Physical DualSense (native HD haptics)"); val: StreamingPreferences.DSHM_PHYSICAL }
                ListElement { text: qsTr("Simulated DualSense (analyzed vibration)"); val: StreamingPreferences.DSHM_EMULATED }
            }
        }
    }

    // ================= 手柄 =================
    SettingsCard {
        visible: settingsPage.category === "gamepad" && hasVisibleContent
        title: qsTr("Gamepad Settings")

        ToggleRow {
            title: qsTr("Swap A/B and X/Y gamepad buttons")
            description: qsTr("This switches gamepads into a Nintendo-style button layout")
            checked: StreamingPreferences.swapFaceButtons
            onToggled: function(value) { StreamingPreferences.swapFaceButtons = value }
        }

        ToggleRow {
            title: qsTr("Force gamepad #1 always connected")
            description: qsTr("Forces a single gamepad to always stay connected to the host, even if no gamepads are actually connected to this PC.") + " " +
                         qsTr("Only enable this option when streaming a game that doesn't support gamepads being connected after startup.")
            checked: !StreamingPreferences.multiController
            onToggled: function(value) { StreamingPreferences.multiController = !value }
        }

        ToggleRow {
            title: qsTr("Enable mouse control with gamepads by holding the 'Start' button")
            checked: StreamingPreferences.gamepadMouse
            onToggled: function(value) { StreamingPreferences.gamepadMouse = value }
        }

        ToggleRow {
            applicable: SystemProperties.hasDesktopEnvironment
            title: Brand.text(qsTr("Process gamepad input when Moonlight is in the background"))
            description: Brand.text(qsTr("Allows Moonlight to capture gamepad inputs even if it's not the current window in focus"))
            checked: StreamingPreferences.backgroundGamepad
            onToggled: function(value) { StreamingPreferences.backgroundGamepad = value }
        }
    }

    SettingsCard {
        visible: settingsPage.category === "gamepad" && hasVisibleContent
        title: qsTr("Gamepad quit combo")

        ChoiceRow {
            title: qsTr("Gamepad quit combo")
            description: qsTr("Choose which button combination exits streaming. Use alternatives if the default doesn't work on your device.")
            selectedValue: StreamingPreferences.gamepadQuitCombo
            onValueActivated: function(value) { StreamingPreferences.gamepadQuitCombo = value }

            model: ListModel {
                ListElement { text: qsTr("Start + Select + L1 + R1 (Default)"); val: StreamingPreferences.GQC_DEFAULT }
                ListElement { text: qsTr("Select + L1 + R1 + X"); val: StreamingPreferences.GQC_SELECT_L1_R1_X }
                ListElement { text: qsTr("Select + L1 + R1 + Y"); val: StreamingPreferences.GQC_SELECT_L1_R1_Y }
                ListElement { text: qsTr("Start + L1 + R1 + A"); val: StreamingPreferences.GQC_START_L1_R1_A }
                ListElement { text: qsTr("Start + L1 + R1 + B"); val: StreamingPreferences.GQC_START_L1_R1_B }
                ListElement { text: qsTr("L1 + R1 + X + Y"); val: StreamingPreferences.GQC_L1_R1_X_Y }
                ListElement { text: qsTr("L1 + R1 + A + B"); val: StreamingPreferences.GQC_L1_R1_A_B }
            }
        }
    }

    // ================= 高级 =================
    SettingsCard {
        visible: settingsPage.category === "advanced" && hasVisibleContent
        title: qsTr("Video pipeline")

        ChoiceRow {
            title: qsTr("Video decoder")
            selectedValue: StreamingPreferences.videoDecoderSelection
            onValueActivated: function(value) { StreamingPreferences.videoDecoderSelection = value }

            model: ListModel {
                ListElement { text: qsTr("Automatic (Recommended)"); val: StreamingPreferences.VDS_AUTO }
                ListElement { text: qsTr("Force software decoding"); val: StreamingPreferences.VDS_FORCE_SOFTWARE }
                ListElement { text: qsTr("Force hardware decoding"); val: StreamingPreferences.VDS_FORCE_HARDWARE }
            }
        }

        ChoiceRow {
            title: qsTr("Video codec")
            selectedValue: StreamingPreferences.videoCodecConfig
            onValueActivated: function(value) { StreamingPreferences.videoCodecConfig = value }

            model: ListModel {
                ListElement { text: qsTr("Automatic (Recommended)"); val: StreamingPreferences.VCC_AUTO }
                ListElement { text: qsTr("H.264"); val: StreamingPreferences.VCC_FORCE_H264 }
                ListElement { text: qsTr("HEVC (H.265)"); val: StreamingPreferences.VCC_FORCE_HEVC }
                ListElement { text: qsTr("AV1"); val: StreamingPreferences.VCC_FORCE_AV1 }
            }
        }

        ChoiceRow {
            applicable: SystemProperties.isDarwin
            title: qsTr("Renderer")
            selectedValue: StreamingPreferences.rendererSelection
            onValueActivated: function(value) { StreamingPreferences.rendererSelection = value }

            model: ListModel {
                ListElement { text: qsTr("Automatic (Recommended)"); val: StreamingPreferences.RS_AUTO }
                ListElement { text: "Vulkan"; val: StreamingPreferences.RS_VULKAN }
                ListElement { text: "Metal"; val: StreamingPreferences.RS_METAL }
                ListElement { text: "AVSampleBufferDisplayLayer"; val: StreamingPreferences.RS_AVSBDL }
            }
        }

        ToggleRow {
            title: qsTr("Enable YUV 4:4:4")
            description: qsTr("Good for streaming desktop and text-heavy games, but not recommended for fast-paced games.")
            checked: StreamingPreferences.enableYUV444
            onToggled: function(value) {
                if (StreamingPreferences.enableYUV444 === value) {
                    return
                }

                StreamingPreferences.enableYUV444 = value
                if (StreamingPreferences.autoAdjustBitrate) {
                    StreamingPreferences.bitrateKbps = StreamingPreferences.getDefaultBitrate(
                        StreamingPreferences.width,
                        StreamingPreferences.height,
                        StreamingPreferences.fps,
                        value)
                    settingsPage.bitratePreferenceChanged()
                }
            }
        }
    }

    SettingsCard {
        visible: settingsPage.category === "advanced" && hasVisibleContent
        title: qsTr("Network")

        ToggleRow {
            title: qsTr("Automatically find PCs on the local network (Recommended)")
            checked: StreamingPreferences.enableMdns
            onToggled: function(value) {
                if (StreamingPreferences.enableMdns === value) {
                    return
                }

                StreamingPreferences.enableMdns = value
                if (window.pollingActive) {
                    ComputerManager.stopPollingAsync()
                    ComputerManager.startPolling()
                }
            }
        }

        ToggleRow {
            title: qsTr("Automatically detect blocked connections (Recommended)")
            checked: StreamingPreferences.detectNetworkBlocking
            onToggled: function(value) { StreamingPreferences.detectNetworkBlocking = value }
        }
    }

    SettingsCard {
        visible: settingsPage.category === "advanced" && hasVisibleContent
        title: qsTr("Diagnostics")

        ToggleRow {
            title: qsTr("Show performance stats while streaming")
            description: qsTr("Display real-time stream performance information while streaming.") + "\n\n" +
                         qsTr("You can toggle it at any time while streaming using Ctrl+Alt+Shift+S or Select+L1+R1+X.") + "\n\n" +
                         qsTr("The performance overlay is not supported on Steam Link or Raspberry Pi.")
            checked: StreamingPreferences.showPerformanceOverlay
            onToggled: function(value) { StreamingPreferences.showPerformanceOverlay = value }
        }
    }

    SettingsCard {
        visible: settingsPage.category === "advanced" && hasVisibleContent
        title: qsTr("Overlay menu position")

        ChoiceRow {
            title: qsTr("Overlay menu position")
            selectedValue: StreamingPreferences.overlayMenuPosition
            onValueActivated: function(value) { StreamingPreferences.overlayMenuPosition = value }

            model: ListModel {
                ListElement { text: qsTr("Top edge"); val: StreamingPreferences.OMP_TOP_EDGE }
                ListElement { text: qsTr("Right edge"); val: StreamingPreferences.OMP_RIGHT_EDGE }
                ListElement { text: qsTr("Left edge"); val: StreamingPreferences.OMP_LEFT_EDGE }
                ListElement { text: qsTr("Floating button"); val: StreamingPreferences.OMP_BUTTON }
                ListElement { text: qsTr("Disabled (default)"); val: StreamingPreferences.OMP_DISABLED }
            }
        }
    }

    Component.onCompleted: {
        if (StreamingPreferences.captureSysKeysMode !== StreamingPreferences.CSK_OFF) {
            captureSysKeysSelection = StreamingPreferences.captureSysKeysMode
        }

        if (Qt.platform.os !== "windows") {
            for (var i = 0; i < dualSenseHapticsModeListModel.count; i++) {
                if (dualSenseHapticsModeListModel.get(i).val === StreamingPreferences.DSHM_PHYSICAL) {
                    dualSenseHapticsModeListModel.remove(i)
                    break
                }
            }
            dualSenseHapticsRow.syncSelection()
        }
    }
}
