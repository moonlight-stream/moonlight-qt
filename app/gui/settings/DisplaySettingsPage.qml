import QtQuick 2.9
import QtQuick.Controls
import QtQuick.Layouts 1.3
import "."
import "../theme"
import ".."

import StreamingPreferences 1.0
import SystemProperties 1.0

// 「显示」分类：画面怎么呈现在这块屏幕上 —— 窗口模式、垂直同步、帧同步，以及 HDR。
//
// 这些原本挤在「基本设置」里。那一页同时装着「传多少数据」（分辨率、帧率、码率）和
// 「怎么显示」两类完全不同的问题，加上 HDR 亮度那张大卡，一屏根本读不完。按用户找
// 设置时的问法拆开：调清晰度去基本设置，调全屏/撕裂/HDR 来这里。
Column {
    id: displayPage

    // 换界面语言时要重建窗口模式下拉的文案。信号由 SettingsView 转发过来 ——
    // 原来这段在基本设置页里，直接连的是 basicPage.languageChanged；搬过来之后那个 id
    // 不在本文件作用域里，Component.onCompleted 一执行就是 ReferenceError。
    signal languageChanged()

    width: parent ? parent.width : 0
    spacing: Theme.spaceLg

    SettingsCard {
        title: qsTr("Display")

        SettingsRow {
            applicable: SystemProperties.hasDesktopEnvironment
            title: qsTr("Display mode")
            description: qsTr("Fullscreen generally provides the best performance, but borderless windowed may work better with features like macOS Spaces, Alt+Tab, screenshot tools, on-screen overlays, etc.")

            AutoResizingComboBox {
                id: windowModeComboBox
                maximumWidth: 260
                textRole: "text"
                enabled: !SystemProperties.rendererAlwaysFullScreen
                hoverEnabled: true

                function createModel() {
                    var model = Qt.createQmlObject('import QtQuick 2.0; ListModel {}', windowModeComboBox, '')

                    model.append({
                                     text: qsTr("Fullscreen"),
                                     val: StreamingPreferences.WM_FULLSCREEN
                                 })

                    model.append({
                                     text: qsTr("Borderless windowed"),
                                     val: StreamingPreferences.WM_FULLSCREEN_DESKTOP
                                 })

                    model.append({
                                     text: qsTr("Windowed"),
                                     val: StreamingPreferences.WM_WINDOWED
                                 })

                    // Set the recommended option based on the OS
                    for (var i = 0; i < model.count; i++) {
                        var thisWm = model.get(i).val;
                        if (thisWm === StreamingPreferences.recommendedFullScreenMode) {
                            model.get(i).text += " " + qsTr("(Recommended)")
                            model.move(i, 0, 1)
                            break
                        }
                    }

                    return model
                }

                // This is used on initialization and upon retranslation
                function reinitialize() {
                    if (!SystemProperties.hasDesktopEnvironment) {
                        // Do nothing if the control won't even be visible
                        return
                    }

                    model = createModel()
                    currentIndex = 0

                    // Set the current value based on the saved preferences
                    var savedWm = StreamingPreferences.windowMode
                    for (var i = 0; i < model.count; i++) {
                         var thisWm = model.get(i).val;
                         if (savedWm === thisWm) {
                             currentIndex = i
                             break
                         }
                    }

                    activated(currentIndex)
                }

                Component.onCompleted: {
                    reinitialize()
                    displayPage.languageChanged.connect(reinitialize)
                }

                onActivated: {
                    StreamingPreferences.windowMode = model.get(currentIndex).val
                }
            }
        }

        ToggleRow {
            title: qsTr("Stretch presentation")
            description: qsTr("Ignores both client and host PC aspect ratios, which is required for displaying Half-SBS (Side-By-Side) 3D signals to AR/XR devices that only support Full-SBS (usually 1920x1080 per eye, meaning a total resolution of 3840x1080)")
            checked: StreamingPreferences.ignoreAspectRatio
            onToggled: function(value) { StreamingPreferences.ignoreAspectRatio = value }
        }

        ToggleRow {
            id: vsyncToggle
            title: qsTr("V-Sync")
            description: qsTr("Disabling V-Sync allows sub-frame rendering latency, but it can display visible tearing")
            checked: StreamingPreferences.enableVsync
            onToggled: function(value) { StreamingPreferences.enableVsync = value }
        }

        ToggleRow {
            title: qsTr("Frame pacing")
            description: qsTr("Frame pacing reduces micro-stutter by delaying frames that come in too early")
            controlEnabled: StreamingPreferences.enableVsync
            checked: StreamingPreferences.enableVsync && StreamingPreferences.framePacing
            onToggled: function(value) { StreamingPreferences.framePacing = value }
        }
    }

    // ================= HDR =================
    SettingsCard {
        title: qsTr("HDR")

        ToggleRow {
            title: qsTr("Enable HDR")
            description: SystemProperties.supportsHdr ?
                             qsTr("The stream will be HDR-capable, but some games may require an HDR monitor on your host PC to enable HDR mode.")
                           :
                             qsTr("HDR streaming is not supported on this PC.")
            controlEnabled: SystemProperties.supportsHdr
            checked: SystemProperties.supportsHdr && StreamingPreferences.enableHdr
            onToggled: function(value) { StreamingPreferences.enableHdr = value }
        }

        SettingsRow {
            id: hdrModeRow
            title: qsTr("HDR format")
            description: qsTr("HDR10 (PQ) is the standard HDR format. HLG offers better compatibility with SDR displays when HDR is not active on the host.")

            AutoResizingComboBox {
                id: hdrModeComboBox
                maximumWidth: 220
                textRole: "text"
                enabled: SystemProperties.supportsHdr && StreamingPreferences.enableHdr
                hoverEnabled: true

                model: ListModel {
                    id: hdrModeListModel
                    ListElement {
                        text: "HDR10 (PQ)"
                        val: 1
                    }
                    ListElement {
                        text: "HLG"
                        val: 2
                    }
                }

                Component.onCompleted: {
                    for (var i = 0; i < hdrModeListModel.count; i++) {
                        if (hdrModeListModel.get(i).val === StreamingPreferences.hdrMode) {
                            currentIndex = i
                            break
                        }
                    }
                }

                onActivated: {
                    if (enabled) {
                        StreamingPreferences.hdrMode = hdrModeListModel.get(currentIndex).val
                    }
                }
            }
        }

        // HDR 亮度卡片体量太大，原样保留在自己的文件里
        Item {
            width: parent.width
            // 同样不能用 visible，见 SettingsCard.hasVisibleContent 的注释
            visible: hdrModeRow.applicable
            height: visible ? hdrCard.height : 0

            HdrBrightnessCard {
                id: hdrCard
                width: parent.width
            }
        }
    }
}
