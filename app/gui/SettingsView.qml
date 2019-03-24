import QtQuick 2.9
import QtQuick.Controls 2.2

import StreamingPreferences 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import SystemProperties 1.0

Flickable {
    id: settingsPage
    objectName: "Settings"
    anchors.fill: parent

    contentWidth: settingsColumn1.width > settingsColumn2.width ? settingsColumn1.width : settingsColumn2.width
    contentHeight: settingsColumn1.height > settingsColumn2.height ? settingsColumn1.height : settingsColumn2.height

    ScrollBar.vertical: ScrollBar {
        parent: settingsPage.parent
        anchors {
            top: settingsPage.top
            left: settingsPage.right
            bottom: settingsPage.bottom

            leftMargin: -10
        }
    }

    StreamingPreferences {
        id: prefs
    }

    StackView.onActivated: {
        SdlGamepadKeyNavigation.setSettingsMode(true)
    }

    StackView.onDeactivating: {
        SdlGamepadKeyNavigation.setSettingsMode(false)
        prefs.save()
    }

    Column {
        padding: 10
        id: settingsColumn1
        width: settingsPage.width / 2 - padding

        GroupBox {
            id: basicSettingsGroupBox
            width: (parent.width - 2 * parent.padding)
            padding: 12
            title: "<font color=\"skyblue\">Basic Settings</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                Label {
                    width: parent.width
                    id: resFPStitle
                    text: qsTr("Resolution and FPS")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                Label {
                    width: parent.width
                    id: resFPSdesc
                    text: qsTr("Setting values too high for your PC or network connection may cause lag, stuttering, or errors.")
                    font.pointSize: 9
                    wrapMode: Text.Wrap
                }

                Row {
                    spacing: 5

                    AutoResizingComboBox {
                        // ignore setting the index at first, and actually set it when the component is loaded
                        Component.onCompleted: {
                            // Add native resolutions for all attached displays
                            var done = false
                            for (var displayIndex = 0; !done; displayIndex++) {
                                for (var displayResIndex = 0; displayResIndex < 2; displayResIndex++) {
                                    var screenRect;

                                    // Some platforms have different desktop resolutions
                                    // and native resolutions (like macOS with Retina displays)
                                    if (displayResIndex == 0) {
                                        screenRect = SystemProperties.getDesktopResolution(displayIndex)
                                    }
                                    else {
                                        screenRect = SystemProperties.getNativeResolution(displayIndex)
                                    }

                                    if (screenRect.width === 0) {
                                        // Exceeded max count of displays
                                        done = true
                                        break
                                    }

                                    var indexToAdd = 0
                                    for (var j = 0; j < resolutionComboBox.count; j++) {
                                        var existing_width = parseInt(resolutionListModel.get(j).video_width);
                                        var existing_height = parseInt(resolutionListModel.get(j).video_height);

                                        if (screenRect.width === existing_width && screenRect.height === existing_height) {
                                            // Duplicate entry, skip
                                            indexToAdd = -1
                                            break
                                        }
                                        else if (screenRect.width * screenRect.height > existing_width * existing_height) {
                                            // Candidate entrypoint after this entry
                                            indexToAdd = j + 1
                                        }
                                    }

                                    // Insert this display's resolution if it's not a duplicate
                                    if (indexToAdd >= 0) {
                                        resolutionListModel.insert(indexToAdd,
                                                                   {
                                                                       "text": "Native ("+screenRect.width+"x"+screenRect.height+")",
                                                                       "video_width": ""+screenRect.width,
                                                                       "video_height": ""+screenRect.height
                                                                   })
                                    }
                                }
                            }

                            // load the saved width/height, and iterate through the ComboBox until a match is found
                            // and set it to that index.
                            var saved_width = prefs.width
                            var saved_height = prefs.height
                            currentIndex = 0
                            for (var i = 0; i < resolutionListModel.count; i++) {
                                var el_width = parseInt(resolutionListModel.get(i).video_width);
                                var el_height = parseInt(resolutionListModel.get(i).video_height);

                                // Pick the highest value lesser or equal to the saved resolution
                                if (saved_width * saved_height >= el_width * el_height) {
                                    currentIndex = i
                                }
                            }

                            // Persist the selected value
                            activated(currentIndex)
                        }

                        id: resolutionComboBox
                        textRole: "text"
                        model: ListModel {
                            id: resolutionListModel
                            // Other elements may be added at runtime
                            // based on attached display resolution
                            ListElement {
                                text: "720p"
                                video_width: "1280"
                                video_height: "720"
                            }
                            ListElement {
                                text: "1080p"
                                video_width: "1920"
                                video_height: "1080"
                            }
                            ListElement {
                                text: "1440p"
                                video_width: "2560"
                                video_height: "1440"
                            }
                            ListElement {
                                text: "4K"
                                video_width: "3840"
                                video_height: "2160"
                            }
                        }
                        // ::onActivated must be used, as it only listens for when the index is changed by a human
                        onActivated : {
                            var selectedWidth = parseInt(resolutionListModel.get(currentIndex).video_width)
                            var selectedHeight = parseInt(resolutionListModel.get(currentIndex).video_height)

                            // Only modify the bitrate if the values actually changed
                            if (prefs.width !== selectedWidth || prefs.height !== selectedHeight) {
                                prefs.width = selectedWidth
                                prefs.height = selectedHeight

                                prefs.bitrateKbps = prefs.getDefaultBitrate(prefs.width, prefs.height, prefs.fps);
                                slider.value = prefs.bitrateKbps
                            }
                        }
                    }

                    AutoResizingComboBox {
                        function createModel() {
                            var fpsListModel = Qt.createQmlObject('import QtQuick 2.0; ListModel {}', parent, '')

                            var max_fps = SystemProperties.maximumStreamingFrameRate

                            // Default entries
                            fpsListModel.append({"text": "30 FPS", "video_fps": "30"})
                            fpsListModel.append({"text": "60 FPS", "video_fps": "60"})

                            // Add unsupported FPS values that come before the display max FPS
                            if (prefs.unsupportedFps) {
                                if (max_fps > 90) {
                                    fpsListModel.append({"text": "90 FPS (Unsupported)", "video_fps": "90"})
                                }
                                if (max_fps > 120) {
                                    fpsListModel.append({"text": "120 FPS (Unsupported)", "video_fps": "120"})
                                }
                            }

                            // Use 64 as the cutoff for adding a separate option to
                            // handle wonky displays that report just over 60 Hz.
                            if (max_fps > 64) {
                                // Mark any FPS value greater than 120 as unsupported
                                if (prefs.unsupportedFps && max_fps > 120) {
                                    fpsListModel.append({"text": max_fps+" FPS (Unsupported)", "video_fps": ""+max_fps})
                                }
                                else if (max_fps > 120) {
                                    fpsListModel.append({"text": "120 FPS", "video_fps": "120"})
                                }
                                else {
                                    fpsListModel.append({"text": max_fps+" FPS", "video_fps": ""+max_fps})
                                }
                            }

                            // Add unsupported FPS values that come after the display max FPS
                            if (prefs.unsupportedFps) {
                                if (max_fps < 90) {
                                    fpsListModel.append({"text": "90 FPS (Unsupported)", "video_fps": "90"})
                                }
                                if (max_fps < 120) {
                                    fpsListModel.append({"text": "120 FPS (Unsupported)", "video_fps": "120"})
                                }
                            }

                            return fpsListModel
                        }

                        function reinitialize() {
                            model = createModel()

                            var saved_fps = prefs.fps
                            currentIndex = 0
                            for (var i = 0; i < model.count; i++) {
                                var el_fps = parseInt(model.get(i).video_fps);

                                // Pick the highest value lesser or equal to the saved FPS
                                if (saved_fps >= el_fps) {
                                    currentIndex = i
                                }
                            }

                            // Persist the selected value
                            activated(currentIndex)
                        }

                        // ignore setting the index at first, and actually set it when the component is loaded
                        Component.onCompleted: {
                            reinitialize()
                        }

                        id: fpsComboBox
                        textRole: "text"
                        // ::onActivated must be used, as it only listens for when the index is changed by a human
                        onActivated : {
                            var selectedFps = parseInt(model.get(currentIndex).video_fps)

                            // Only modify the bitrate if the values actually changed
                            if (prefs.fps !== selectedFps) {
                                prefs.fps = selectedFps

                                prefs.bitrateKbps = prefs.getDefaultBitrate(prefs.width, prefs.height, prefs.fps);
                                slider.value = prefs.bitrateKbps
                            }
                        }
                    }
                }

                Label {
                    width: parent.width
                    id: bitrateTitle
                    text: qsTr("Video bitrate: ")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                Label {
                    width: parent.width
                    id: bitrateDesc
                    text: qsTr("Lower the bitrate on slower connections. Raise the bitrate to increase image quality.")
                    font.pointSize: 9
                    wrapMode: Text.Wrap
                }

                Slider {
                    id: slider

                    value: prefs.bitrateKbps

                    stepSize: 500
                    from : 500
                    to: 150000

                    snapMode: "SnapOnRelease"
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)

                    onValueChanged: {
                        bitrateTitle.text = "Video bitrate: " + (value / 1000.0) + " Mbps"
                        prefs.bitrateKbps = value
                    }
                }

                Label {
                    width: parent.width
                    id: windowModeTitle
                    text: qsTr("Display mode")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var savedWm = prefs.windowMode
                        currentIndex = 0
                        for (var i = 0; i < windowModeListModel.count; i++) {
                             var thisWm = windowModeListModel.get(i).val;
                             if (savedWm === thisWm) {
                                 currentIndex = i
                                 break
                             }
                        }
                        activated(currentIndex)
                    }

                    id: windowModeComboBox
                    hoverEnabled: true
                    textRole: "text"
                    model: ListModel {
                        id: windowModeListModel
                        ListElement {
                            text: "Full-screen (Recommended)"
                            val: StreamingPreferences.WM_FULLSCREEN
                        }
                        ListElement {
                            text: "Borderless windowed"
                            val: StreamingPreferences.WM_FULLSCREEN_DESKTOP
                        }
                        ListElement {
                            text: "Windowed"
                            val: StreamingPreferences.WM_WINDOWED
                        }
                    }
                    onActivated: {
                        prefs.windowMode = windowModeListModel.get(currentIndex).val
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "Full-screen generally provides the best performance, but borderless windowed may work better with features like macOS Spaces, Alt+Tab, screenshot tools, on-screen overlays, etc."
                }

                CheckBox {
                    id: vsyncCheck
                    hoverEnabled: true
                    text: "V-Sync"
                    font.pointSize:  12
                    checked: prefs.enableVsync
                    onCheckedChanged: {
                        prefs.enableVsync = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "Disabling V-Sync allows sub-frame rendering latency, but it can display visible tearing"
                }

                CheckBox {
                    id: framePacingCheck
                    hoverEnabled: true
                    text: "Frame pacing"
                    font.pointSize:  12
                    enabled: prefs.enableVsync
                    checked: prefs.enableVsync && prefs.framePacing
                    onCheckedChanged: {
                        prefs.framePacing = checked
                    }
                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "Frame pacing reduces micro-stutter by delaying frames that come in too early"
                }
            }
        }

        GroupBox {

            id: audioSettingsGroupBox
            width: (parent.width - 2 * parent.padding)
            padding: 12
            title: "<font color=\"skyblue\">Audio Settings</font>"
            font.pointSize: 12

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
                        var saved_audio = prefs.audioConfig
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
                            text: "Stereo"
                            val: StreamingPreferences.AC_STEREO
                        }
                        ListElement {
                            text: "5.1 surround sound"
                            val: StreamingPreferences.AC_51_SURROUND
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        prefs.audioConfig = audioListModel.get(currentIndex).val
                    }
                }

            }
        }

        GroupBox {
            id: uiSettingsGroupBox
            width: (parent.width - 2 * parent.padding)
            padding: 12
            title: "<font color=\"skyblue\">UI Settings</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: startMaximizedCheck
                    text: "Maximize Moonlight window on startup"
                    font.pointSize: 12
                    checked: !prefs.startWindowed
                    onCheckedChanged: {
                        prefs.startWindowed = !checked
                    }
                }

                CheckBox {
                    id: connectionWarningsCheck
                    text: "Show connection quality warnings"
                    font.pointSize: 12
                    checked: prefs.connectionWarnings
                    onCheckedChanged: {
                        prefs.connectionWarnings = checked
                    }
                }
            }
        }
    }

    Column {
        padding: 10
        anchors.left: settingsColumn1.right
        id: settingsColumn2
        width: settingsPage.width / 2 - padding

        GroupBox {
            id: gamepadSettingsGroupBox
            width: (parent.width - parent.padding)
            padding: 12
            title: "<font color=\"skyblue\">Input Settings</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: singleControllerCheck
                    text: "Force gamepad #1 always present"
                    font.pointSize:  12
                    checked: !prefs.multiController
                    onCheckedChanged: {
                        prefs.multiController = !checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: "Forces a single gamepad to always stay connected to the host, even if no gamepads are actually connected to this PC.\n" +
                                  "Only enable this option when streaming a game that doesn't support gamepads being connected after startup."
                }

                CheckBox {
                    id: rawInputCheck
                    hoverEnabled: true
                    text: "Raw mouse input"
                    font.pointSize:  12
                    checked: !prefs.mouseAcceleration
                    onCheckedChanged: {
                        prefs.mouseAcceleration = !checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 3000
                    ToolTip.visible: hovered
                    ToolTip.text: "When checked, mouse input is not accelerated or scaled by the OS before passing to Moonlight"
                }
            }
        }

        GroupBox {
            id: hostSettingsGroupBox
            width: (parent.width - parent.padding)
            padding: 12
            title: "<font color=\"skyblue\">Host Settings</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: optimizeGameSettingsCheck
                    text: "Optimize game settings"
                    font.pointSize:  12
                    checked: prefs.gameOptimizations
                    onCheckedChanged: {
                        prefs.gameOptimizations = checked
                    }
                }

                CheckBox {
                    id: audioPcCheck
                    text: "Play audio on host PC"
                    font.pointSize:  12
                    checked: prefs.playAudioOnHost
                    onCheckedChanged: {
                        prefs.playAudioOnHost = checked
                    }
                }
            }
        }

        GroupBox {
            id: advancedSettingsGroupBox
            width: (parent.width - parent.padding)
            padding: 12
            title: "<font color=\"skyblue\">Advanced Settings</font>"
            font.pointSize: 12

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
                        var saved_vds = prefs.videoDecoderSelection
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
                            text: "Automatic (Recommended)"
                            val: StreamingPreferences.VDS_AUTO
                        }
                        ListElement {
                            text: "Force software decoding"
                            val: StreamingPreferences.VDS_FORCE_SOFTWARE
                        }
                        ListElement {
                            text: "Force hardware decoding"
                            val: StreamingPreferences.VDS_FORCE_HARDWARE
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        prefs.videoDecoderSelection = decoderListModel.get(currentIndex).val
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
                        var saved_vcc = prefs.videoCodecConfig
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
                            text: "Automatic (Recommended)"
                            val: StreamingPreferences.VCC_AUTO
                        }
                        ListElement {
                            text: "Force H.264"
                            val: StreamingPreferences.VCC_FORCE_H264
                        }
                        ListElement {
                            text: "Force HEVC"
                            val: StreamingPreferences.VCC_FORCE_HEVC
                        }
                        // HDR seems to be broken in GFE 3.14.1, and even when that's fixed
                        // we'll probably need to gate this feature on OS support in our
                        // renderers.
                        /* ListElement {
                            text: "Force HEVC HDR"
                            val: StreamingPreferences.VCC_FORCE_HEVC_HDR
                        } */
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        prefs.videoCodecConfig = codecListModel.get(currentIndex).val
                    }
                }

                CheckBox {
                    id: unlockUnsupportedFps
                    text: "Unlock unsupported FPS options"
                    font.pointSize: 12
                    checked: prefs.unsupportedFps
                    onCheckedChanged: {
                        prefs.unsupportedFps = checked

                        // The selectable FPS values depend on whether
                        // this option is enabled or not
                        fpsComboBox.reinitialize()
                    }
                }

                CheckBox {
                    id: enableMdns
                    text: "Automatically find PCs on the local network (Recommended)"
                    font.pointSize: 12
                    checked: prefs.enableMdns
                    onCheckedChanged: {
                        prefs.enableMdns = checked

                        // We must save the updated preference to ensure
                        // ComputerManager can observe the change internally.
                        prefs.save()

                        // Restart polling so the mDNS change takes effect
                        if (window.pollingActive) {
                            ComputerManager.stopPollingAsync()
                            ComputerManager.startPolling()
                        }
                    }
                }

                CheckBox {
                    id: quitAppAfter
                    text: "Quit app after quitting session"
                    font.pointSize: 12
                    checked: prefs.quitAppAfter
                    onCheckedChanged: {
                        prefs.quitAppAfter = checked
                    }
                }
            }
        }
    }
}
