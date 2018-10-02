import QtQuick 2.9
import QtQuick.Controls 2.2

import StreamingPreferences 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0

ScrollView {
    id: settingsPage
    objectName: "Settings"

    StreamingPreferences {
        id: prefs
    }

    // The StackView will trigger a visibility change when
    // we're pushed onto it, causing our onVisibleChanged
    // routine to run, but only if we start as invisible
    visible: false

    SdlGamepadKeyNavigation {
        id: gamepadKeyNav
    }

    onVisibleChanged: {
        if (visible) {
            gamepadKeyNav.setSettingsMode(true)
            gamepadKeyNav.enable()
        }
        else {
            gamepadKeyNav.disable()
        }
    }

    Component.onDestruction: {
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
                    color: "white"
                }

                Label {
                    width: parent.width
                    id: resFPSdesc
                    text: qsTr("Setting values too high for your PC may cause lag, stuttering, or errors.")
                    font.pointSize: 9
                    wrapMode: Text.Wrap
                    color: "white"
                }

                Row {
                    spacing: 5

                    ComboBox {
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
                                        screenRect = prefs.getDesktopResolution(displayIndex)
                                    }
                                    else {
                                        screenRect = prefs.getNativeResolution(displayIndex)
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
                        width: 200
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

                    ComboBox {
                        function createModel() {
                            var fpsListModel = Qt.createQmlObject('import QtQuick 2.0; ListModel {}', parent, '')

                            // Get the max supported FPS on this system
                            var max_fps = prefs.getMaximumStreamingFrameRate();

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
                                fpsListModel.append({"text": max_fps+" FPS", "video_fps": ""+max_fps})
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
                        width: 250
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
                    color: "white"
                }

                Label {
                    width: parent.width
                    id: bitrateDesc
                    text: qsTr("Lower bitrate to reduce lag and stuttering. Raise bitrate to increase image quality.")
                    font.pointSize: 9
                    wrapMode: Text.Wrap
                    color: "white"
                }

                Slider {
                    id: slider
                    wheelEnabled: true

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
                    color: "white"
                }

                ComboBox {
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
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)
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
                }

                CheckBox {
                    id: vsyncCheck
                    text: "<font color=\"white\">Enable V-Sync</font>"
                    font.pointSize:  12
                    checked: prefs.enableVsync
                    onCheckedChanged: {
                        prefs.enableVsync = checked
                    }
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
                    color: "white"
                }

                ComboBox {
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
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)
                    textRole: "text"
                    model: ListModel {
                        id: audioListModel
                        ListElement {
                            text: "Stereo"
                            val: StreamingPreferences.AC_FORCE_STEREO
                        }
                        ListElement {
                            text: "5.1 surround sound"
                            val: StreamingPreferences.AC_FORCE_SURROUND
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        prefs.audioConfig = audioListModel.get(currentIndex).val
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

            Row {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: multiControllerCheck
                    text: "<font color=\"white\">Multiple controller support</font>"
                    font.pointSize:  12
                    checked: prefs.multiController
                    onCheckedChanged: {
                        prefs.multiController = checked
                    }
                }

                CheckBox {
                    id: mouseAccelerationCheck
                    text: "<font color=\"white\">Enable mouse acceleration</font>"
                    font.pointSize:  12
                    checked: prefs.mouseAcceleration
                    onCheckedChanged: {
                        prefs.mouseAcceleration = checked
                    }
                }
            }
        }

        GroupBox {
            id: hostSettingsGroupBox
            width: (parent.width - parent.padding)
            padding: 12
            title: "<font color=\"skyblue\">Host Settings</font>"
            font.pointSize: 12

            Row {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: optimizeGameSettingsCheck
                    text: "<font color=\"white\">Optimize game settings</font>"
                    // HACK: Match width of the other checkbox to make the UI not look bad
                    width: multiControllerCheck.width
                    font.pointSize:  12
                    checked: prefs.gameOptimizations
                    onCheckedChanged: {
                        prefs.gameOptimizations = checked
                    }
                }

                CheckBox {
                    id: audioPcCheck
                    text: "<font color=\"white\">Play audio on host PC</font>"
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
                    color: "white"
                }

                ComboBox {
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
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)
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
                    color: "white"
                }

                ComboBox {
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
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)
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
                    text: "<font color=\"white\">Unlock unsupported FPS options</font>"
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
                    text: "<font color=\"white\">Automatically find PCs on the local network (Recommended)</font>"
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
            }
        }
    }
}
