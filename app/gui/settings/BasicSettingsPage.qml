import QtQuick 2.9
import QtQuick.Controls
import QtQuick.Layouts 1.3
import "."
import "../theme"
import ".."

import StreamingPreferences 1.0
import SystemProperties 1.0

// 「基本设置」——第一个迁移到新架构的分类。
// 逻辑与旧 SettingsView 的 basicSettingsGroupBox 完全一致，只是换成卡片 + 设置行。
Column {
    id: basicPage

    signal languageChanged()

    // 供 LegacySettingsPage 的高级组跨页引用
    property alias bitrateSlider: bitrateSlider
    property alias videoEnhancementCheck: videoEnhancementSwitch

    // 手柄进入设置页时的初始焦点
    property alias firstControl: resolutionComboBox

    width: parent ? parent.width : 0
    spacing: Theme.spaceLg

    // ================= 画面 =================
    SettingsCard {
        title: qsTr("Video")
        subtitle: qsTr("Setting values too high for your PC or network connection may cause lag, stuttering, or errors.")

        SettingsRow {
            title: qsTr("Resolution and FPS")

            Row {
                spacing: Theme.spaceSm

                AutoResizingComboBox {
                    id: resolutionComboBox

                    property int lastIndexValue

                    maximumWidth: 300
                    textRole: "text"

                    function addDetectedResolution(friendlyNamePrefix, rect) {
                        var indexToAdd = 0
                        for (var j = 0; j < resolutionComboBox.count; j++) {
                            var existing_width = parseInt(resolutionListModel.get(j).video_width);
                            var existing_height = parseInt(resolutionListModel.get(j).video_height);

                            if (rect.width === existing_width && rect.height === existing_height) {
                                // Duplicate entry, skip
                                indexToAdd = -1
                                break
                            }
                            else if (rect.width * rect.height > existing_width * existing_height) {
                                // Candidate entrypoint after this entry
                                indexToAdd = j + 1
                            }
                        }

                        // Insert this display's resolution if it's not a duplicate
                        if (indexToAdd >= 0) {
                            resolutionListModel.insert(indexToAdd,
                                                       {
                                                           "text": friendlyNamePrefix+" ("+rect.width+"x"+rect.height+")",
                                                           "video_width": ""+rect.width,
                                                           "video_height": ""+rect.height,
                                                           "is_custom": false
                                                       })
                        }
                    }

                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        // Refresh display data before using it to build the list
                        SystemProperties.refreshDisplays()

                        // Add native and safe area resolutions for all attached displays
                        var done = false
                        for (var displayIndex = 0; !done; displayIndex++) {
                            var screenRect = SystemProperties.getNativeResolution(displayIndex);
                            var safeAreaRect = SystemProperties.getSafeAreaResolution(displayIndex);

                            if (screenRect.width === 0) {
                                // Exceeded max count of displays
                                done = true
                                break
                            }

                            addDetectedResolution(qsTr("Native"), screenRect)
                            addDetectedResolution(qsTr("Native (Excluding Notch)"), safeAreaRect)
                        }

                        // Prune resolutions that are over the decoder's maximum
                        var max_pixels = SystemProperties.maximumResolution.width * SystemProperties.maximumResolution.height;
                        if (max_pixels > 0) {
                            for (var j = 0; j < resolutionComboBox.count; j++) {
                                var existing_width = parseInt(resolutionListModel.get(j).video_width);
                                var existing_height = parseInt(resolutionListModel.get(j).video_height);

                                if (existing_width * existing_height > max_pixels) {
                                    resolutionListModel.remove(j)
                                    j--
                                }
                            }
                        }

                        // load the saved width/height, and iterate through the ComboBox until a match is found
                        // and set it to that index.
                        var saved_width = StreamingPreferences.width
                        var saved_height = StreamingPreferences.height
                        var index_set = false
                        for (var i = 0; i < resolutionListModel.count; i++) {
                            var el_width = parseInt(resolutionListModel.get(i).video_width);
                            var el_height = parseInt(resolutionListModel.get(i).video_height);

                            if (saved_width === el_width && saved_height === el_height) {
                                currentIndex = i
                                index_set = true
                                break
                            }
                        }

                        if (!index_set) {
                            // We did not find a match. This must be a custom resolution.
                            resolutionListModel.append({
                                                           "text": qsTr("Custom")+" ("+StreamingPreferences.width+"x"+StreamingPreferences.height+")",
                                                           "video_width": ""+StreamingPreferences.width,
                                                           "video_height": ""+StreamingPreferences.height,
                                                           "is_custom": true
                                                       })
                            currentIndex = resolutionListModel.count - 1
                        }
                        else {
                            resolutionListModel.append({
                                                           "text": qsTr("Custom"),
                                                           "video_width": "",
                                                           "video_height": "",
                                                           "is_custom": true
                                                       })
                        }

                        // Since we don't call activate() here, we need to trigger
                        // width calculation manually
                        recalculateWidth()

                        lastIndexValue = currentIndex
                    }

                    model: ListModel {
                        id: resolutionListModel
                        // Other elements may be added at runtime
                        // based on attached display resolution
                        ListElement {
                            text: qsTr("720p")
                            video_width: "1280"
                            video_height: "720"
                            is_custom: false
                        }
                        ListElement {
                            text: qsTr("1080p")
                            video_width: "1920"
                            video_height: "1080"
                            is_custom: false
                        }
                        ListElement {
                            text: qsTr("1440p")
                            video_width: "2560"
                            video_height: "1440"
                            is_custom: false
                        }
                        ListElement {
                            text: qsTr("4K")
                            video_width: "3840"
                            video_height: "2160"
                            is_custom: false
                        }
                    }

                    function updateBitrateForSelection() {
                        var selectedWidth = parseInt(resolutionListModel.get(currentIndex).video_width)
                        var selectedHeight = parseInt(resolutionListModel.get(currentIndex).video_height)

                        // Only modify the bitrate if the values actually changed
                        if (StreamingPreferences.width !== selectedWidth || StreamingPreferences.height !== selectedHeight) {
                            StreamingPreferences.width = selectedWidth
                            StreamingPreferences.height = selectedHeight

                            if (StreamingPreferences.autoAdjustBitrate) {
                                StreamingPreferences.bitrateKbps = StreamingPreferences.getDefaultBitrate(StreamingPreferences.width,
                                                                                                          StreamingPreferences.height,
                                                                                                          StreamingPreferences.fps,
                                                                                                          StreamingPreferences.enableYUV444);
                                bitrateSlider.value = Math.log(StreamingPreferences.bitrateKbps)
                            }
                        }

                        lastIndexValue = currentIndex
                    }

                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        if (resolutionListModel.get(currentIndex).is_custom) {
                            customResolutionDialog.open()
                        }
                        else {
                            updateBitrateForSelection()
                        }
                    }

                    NavigableDialog {
                        id: customResolutionDialog
                        standardButtons: Dialog.Ok | Dialog.Cancel
                        onOpened: {
                            // Force keyboard focus on the textbox so keyboard navigation works
                            widthField.forceActiveFocus()

                            // standardButton() was added in Qt 5.10, so we must check for it first
                            if (customResolutionDialog.standardButton) {
                                customResolutionDialog.standardButton(Dialog.Ok).enabled = customResolutionDialog.isInputValid()
                            }
                        }

                        onClosed: {
                            widthField.clear()
                            heightField.clear()
                        }

                        onRejected: {
                            resolutionComboBox.currentIndex = resolutionComboBox.lastIndexValue
                        }

                        function isInputValid() {
                            // If we have text in either textbox that isn't valid,
                            // reject the input.
                            if ((!widthField.acceptableInput && widthField.text) ||
                                    (!heightField.acceptableInput && heightField.text)) {
                                return false
                            }

                            // The textboxes need to have text or placeholder text
                            if ((!widthField.text && !widthField.placeholderText) ||
                                    (!heightField.text && !heightField.placeholderText)) {
                                return false
                            }

                            return true
                        }

                        onAccepted: {
                            // Reject if there's invalid input
                            if (!isInputValid()) {
                                reject()
                                return
                            }

                            var width = widthField.text ? widthField.text : widthField.placeholderText
                            var height = heightField.text ? heightField.text : heightField.placeholderText

                            // Find and update the custom entry
                            for (var i = 0; i < resolutionListModel.count; i++) {
                                if (resolutionListModel.get(i).is_custom) {
                                    resolutionListModel.setProperty(i, "video_width", width)
                                    resolutionListModel.setProperty(i, "video_height", height)
                                    resolutionListModel.setProperty(i, "text", qsTr("Custom")+" ("+width+"x"+height+")")

                                    // Now update the bitrate using the custom resolution
                                    resolutionComboBox.currentIndex = i
                                    resolutionComboBox.updateBitrateForSelection()

                                    // Update the combobox width too
                                    resolutionComboBox.recalculateWidth()
                                    break
                                }
                            }
                        }

                        ColumnLayout {
                            Label {
                                text: qsTr("Custom resolutions are not officially supported by GeForce Experience, so it will not set your host display resolution. You will need to set it manually while in game.") + "\n\n" +
                                      qsTr("Resolutions that are not supported by your client or host PC may cause streaming errors.") + "\n"
                                wrapMode: Label.WordWrap
                                Layout.maximumWidth: 300
                            }

                            Label {
                                text: qsTr("Enter a custom resolution:")
                                font.bold: true
                            }

                            RowLayout {
                                HardTextField {
                                    id: widthField
                                    maximumLength: 5
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    placeholderText: resolutionListModel.get(resolutionComboBox.currentIndex).video_width
                                    validator: IntValidator{bottom:256; top:8192}
                                    focus: true

                                    onTextChanged: {
                                        // standardButton() was added in Qt 5.10, so we must check for it first
                                        if (customResolutionDialog.standardButton) {
                                            customResolutionDialog.standardButton(Dialog.Ok).enabled = customResolutionDialog.isInputValid()
                                        }
                                    }

                                    Keys.onReturnPressed: {
                                        customResolutionDialog.accept()
                                    }

                                    Keys.onEnterPressed: {
                                        customResolutionDialog.accept()
                                    }
                                }

                                Label {
                                    text: "x"
                                    font.bold: true
                                }

                                HardTextField {
                                    id: heightField
                                    maximumLength: 5
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    placeholderText: resolutionListModel.get(resolutionComboBox.currentIndex).video_height
                                    validator: IntValidator{bottom:256; top:8192}

                                    onTextChanged: {
                                        // standardButton() was added in Qt 5.10, so we must check for it first
                                        if (customResolutionDialog.standardButton) {
                                            customResolutionDialog.standardButton(Dialog.Ok).enabled = customResolutionDialog.isInputValid()
                                        }
                                    }

                                    Keys.onReturnPressed: {
                                        customResolutionDialog.accept()
                                    }

                                    Keys.onEnterPressed: {
                                        customResolutionDialog.accept()
                                    }
                                }
                            }
                        }
                    }
                }

                AutoResizingComboBox {
                    id: fpsComboBox

                    property int lastIndexValue

                    maximumWidth: 180
                    textRole: "text"

                    function updateBitrateForSelection() {
                        // Only modify the bitrate if the values actually changed
                        var selectedFps = parseInt(model.get(fpsComboBox.currentIndex).video_fps)
                        if (StreamingPreferences.fps !== selectedFps) {
                            StreamingPreferences.fps = selectedFps

                            if (StreamingPreferences.autoAdjustBitrate) {
                                StreamingPreferences.bitrateKbps = StreamingPreferences.getDefaultBitrate(StreamingPreferences.width,
                                                                                                          StreamingPreferences.height,
                                                                                                          StreamingPreferences.fps,
                                                                                                          StreamingPreferences.enableYUV444);
                                bitrateSlider.value = Math.log(StreamingPreferences.bitrateKbps)
                            }
                        }

                        lastIndexValue = currentIndex
                    }

                    NavigableDialog {
                        id: customFpsDialog
                        standardButtons: Dialog.Ok | Dialog.Cancel

                        function isInputValid() {
                            // If we have text that isn't valid, reject the input.
                            if (!fpsField.acceptableInput && fpsField.text) {
                                return false
                            }

                            // The textbox needs to have text or placeholder text
                            if (!fpsField.text && !fpsField.placeholderText) {
                                return false
                            }

                            return true
                        }

                        onOpened: {
                            // Force keyboard focus on the textbox so keyboard navigation works
                            fpsField.forceActiveFocus()

                            // standardButton() was added in Qt 5.10, so we must check for it first
                            if (customFpsDialog.standardButton) {
                                customFpsDialog.standardButton(Dialog.Ok).enabled = customFpsDialog.isInputValid()
                            }
                        }

                        onClosed: {
                            fpsField.clear()
                        }

                        onRejected: {
                            fpsComboBox.currentIndex = fpsComboBox.lastIndexValue
                        }

                        onAccepted: {
                            // Reject if there's invalid input
                            if (!isInputValid()) {
                                reject()
                                return
                            }

                            var fps = fpsField.text ? fpsField.text : fpsField.placeholderText

                            // Find and update the custom entry
                            for (var i = 0; i < fpsListModel.count; i++) {
                                if (fpsListModel.get(i).is_custom) {
                                    fpsListModel.setProperty(i, "video_fps", fps)
                                    fpsListModel.setProperty(i, "text", qsTr("Custom (%1 FPS)").arg(fps))

                                    // Now update the bitrate using the custom resolution
                                    fpsComboBox.currentIndex = i
                                    fpsComboBox.updateBitrateForSelection()

                                    // Update the combobox width too
                                    fpsComboBox.recalculateWidth()
                                    break
                                }
                            }
                        }

                        ColumnLayout {
                            Label {
                                text: qsTr("Enter a custom frame rate:")
                                font.bold: true
                            }

                            RowLayout {
                                HardTextField {
                                    id: fpsField
                                    maximumLength: 4
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    placeholderText: fpsListModel.get(fpsComboBox.currentIndex).video_fps
                                    validator: IntValidator{bottom:10; top:9999}
                                    focus: true

                                    onTextChanged: {
                                        // standardButton() was added in Qt 5.10, so we must check for it first
                                        if (customFpsDialog.standardButton) {
                                            customFpsDialog.standardButton(Dialog.Ok).enabled = customFpsDialog.isInputValid()
                                        }
                                    }

                                    Keys.onReturnPressed: {
                                        customFpsDialog.accept()
                                    }

                                    Keys.onEnterPressed: {
                                        customFpsDialog.accept()
                                    }
                                }
                            }
                        }
                    }

                    function addRefreshRateOrdered(fpsListModel, refreshRate, description, custom) {
                        var indexToAdd = 0
                        for (var j = 0; j < fpsListModel.count; j++) {
                            var existing_fps = parseInt(fpsListModel.get(j).video_fps);

                            if (refreshRate === existing_fps || (custom && fpsListModel.get(j).is_custom)) {
                                // Duplicate entry, skip
                                indexToAdd = -1
                                break
                            }
                            else if (refreshRate > existing_fps) {
                                // Candidate entrypoint after this entry
                                indexToAdd = j + 1
                            }
                        }

                        // Insert this frame rate if it's not a duplicate
                        if (indexToAdd >= 0) {
                            // Custom values always go at the end of the list
                            if (custom) {
                                indexToAdd = fpsListModel.count
                            }

                            fpsListModel.insert(indexToAdd,
                                                {
                                                    "text": description,
                                                    "video_fps": ""+refreshRate,
                                                    "is_custom": custom
                                                })
                        }

                        return indexToAdd
                    }

                    function reinitialize() {
                        // Add native refresh rate for all attached displays
                        var done = false
                        for (var displayIndex = 0; !done; displayIndex++) {
                            var refreshRate = SystemProperties.getRefreshRate(displayIndex);
                            if (refreshRate === 0) {
                                // Exceeded max count of displays
                                done = true
                                break
                            }

                            addRefreshRateOrdered(fpsListModel, refreshRate, qsTr("%1 FPS").arg(refreshRate), false)
                        }

                        var saved_fps = StreamingPreferences.fps
                        var found = false
                        for (var i = 0; i < model.count; i++) {
                            var el_fps = parseInt(model.get(i).video_fps);

                            // Look for a matching frame rate
                            if (saved_fps === el_fps) {
                                currentIndex = i
                                found = true
                                break
                            }
                        }

                        // If we didn't find one, add a custom frame rate for the current value
                        if (!found) {
                            currentIndex = addRefreshRateOrdered(model, saved_fps, qsTr("Custom (%1 FPS)").arg(saved_fps), true)
                        }
                        else {
                            addRefreshRateOrdered(model, "", qsTr("Custom"), true)
                        }

                        recalculateWidth()

                        lastIndexValue = currentIndex
                    }

                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        reinitialize()
                        basicPage.languageChanged.connect(reinitialize)
                    }

                    model: ListModel {
                        id: fpsListModel
                        // Other elements may be added at runtime
                        ListElement {
                            text: qsTr("30 FPS")
                            video_fps: "30"
                            is_custom: false
                        }
                        ListElement {
                            text: qsTr("60 FPS")
                            video_fps: "60"
                            is_custom: false
                        }
                    }

                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        if (model.get(currentIndex).is_custom) {
                            customFpsDialog.open()
                        }
                        else {
                            updateBitrateForSelection()
                        }
                    }
                }
            }
        }

        SettingsRow {
            id: bitrateRow
            title: qsTr("Video bitrate")
            description: qsTr("Lower the bitrate on slower connections. Raise the bitrate to increase image quality.")

            Text {
                text: {
                    var mbps = Math.exp(bitrateSlider.value) / 1000.0
                    return mbps < 100 ? mbps.toFixed(1) + " Mbps" : Math.round(mbps) + " Mbps"
                }
                color: Theme.accent
                // 数字走等宽 + tabular-nums，拖滑条时数字不会左右抖
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCardTitle
                font.weight: Font.Medium
            }
        }

        // 码率滑条单独占一整行，塞进 SettingsRow 右侧会太窄
        Item {
            width: parent.width
            // 同样不能用 visible，见 SettingsCard.hasVisibleContent 的注释
            visible: bitrateRow.applicable
            height: visible ? bitrateControls.implicitHeight : 0

            Column {
                id: bitrateControls
                anchors {
                    left: parent.left
                    right: parent.right
                    leftMargin: Theme.spaceMd
                    rightMargin: Theme.spaceMd
                }
                spacing: Theme.spaceSm

                HardSlider {
                    id: bitrateSlider
                    width: parent.width

                    // 使用对数刻度来实现非线性调整
                    property real logMin: Math.log(500)
                    property real logMax: Math.log(2000000)

                    value: Math.log(StreamingPreferences.bitrateKbps)
                    stepSize: (logMax - logMin) / 200
                    from: logMin
                    to: logMax
                    snapMode: Slider.SnapOnRelease

                    onValueChanged: {
                        StreamingPreferences.bitrateKbps = Math.exp(value)
                    }

                    onMoved: {
                        StreamingPreferences.autoAdjustBitrate = false
                    }
                }

                // 「恢复默认」是个次要动作，做成滑条右下角的小方按钮
                Item {
                    width: parent.width
                    visible: resetBitrateButton.shown
                    height: visible ? resetBitrateButton.implicitHeight : 0

                    Button {
                        id: resetBitrateButton

                        anchors.right: parent.right
                        hoverEnabled: true
                        padding: 0

                        readonly property int defaultBitrate: StreamingPreferences.getDefaultBitrate(StreamingPreferences.width, StreamingPreferences.height, StreamingPreferences.fps, StreamingPreferences.enableYUV444)

                        text: qsTr("Use Default (%1 Mbps)").arg(defaultBitrate / 1000.0)
                        readonly property bool shown: StreamingPreferences.bitrateKbps !== defaultBitrate

                        contentItem: Text {
                            text: resetBitrateButton.text
                            color: resetBitrateButton.hovered ? Theme.accentStrong : Theme.accent
                            font.family: Theme.fontMono
                            font.pointSize: Theme.fontCaption
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: Theme.tracking(Theme.fontCaption, 0.08)
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            implicitHeight: 30
                            implicitWidth: resetBitrateButton.contentItem.implicitWidth + Theme.spaceLg * 2
                            radius: 0
                            color: resetBitrateButton.down ? Theme.accentSoft
                                                           : (resetBitrateButton.hovered ? Theme.surface2 : "transparent")
                            border.width: 1
                            border.color: resetBitrateButton.activeFocus ? Theme.accent : Theme.lineStrong

                            Behavior on color {
                                ColorAnimation { duration: Theme.durFast }
                            }
                        }

                        onClicked: {
                            StreamingPreferences.bitrateKbps = defaultBitrate
                            StreamingPreferences.autoAdjustBitrate = true
                            bitrateSlider.value = Math.log(defaultBitrate)
                        }
                    }
                }
            }
        }

        ToggleRow {
            title: qsTr("Smart bitrate with Sunshine")
            description: qsTr("Allows Sunshine to automatically adjust stream bitrate up to the selected video bitrate when the host supports ABR.")
            checked: StreamingPreferences.enableSunshineAbr
            onToggled: function(value) { StreamingPreferences.enableSunshineAbr = value }
        }
    }

    // ================= 显示 =================
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
                    basicPage.languageChanged.connect(reinitialize)
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

    // ================= 画质增强 =================
    SettingsCard {
        title: qsTr("Enhancements")

        SettingsRow {
            title: videoEnhancementSwitch.labelText
            description: qsTr("Enhance video quality by utilizing the GPU's AI-Enhancement capabilities.") + "\n" +
                         qsTr("This feature effectively upscales, reduces compression artifacts and enhances the clarity of streamed content.") + "\n" +
                         qsTr("Note:") + "\n" +
                         qsTr("If available, ensure that appropriate settings (i.e. RTX Video enhancement) are enabled in your GPU driver configuration.") + "\n" +
                         qsTr("HDR rendering has diverse issues depending on the GPU used, we are working on it but we advise to currently use Non-HDR.") + "\n" +
                         qsTr("Be advised that using this feature on laptops running on battery power may lead to significant battery drain.")

            HardSwitch {
                id: videoEnhancementSwitch

                // 高级组的解码器下拉会读写 keepValue，别删
                property bool keepValue: checked
                property string labelText: qsTr("Video AI-Enhancement")

                hoverEnabled: true
                enabled: SystemProperties.isVideoEnhancementCapable()
                checked: SystemProperties.isVideoEnhancementCapable() && StreamingPreferences.videoEnhancement
                onCheckedChanged: {
                    StreamingPreferences.videoEnhancement = checked
                }

                Component.onCompleted: {
                    if (!SystemProperties.isVideoEnhancementCapable()){
                        // VSR or SDR->HDR feature could not be initialized by any GPU available
                        labelText = qsTr("Video AI-Enhancement (Not supported by the GPU)")
                        enabled = false;
                        checked = false;
                    } else if(SystemProperties.isVideoEnhancementExperimental()){
                        // Indicate if the feature is available but not officially deployed by the Vendor
                        labelText = qsTr("Video AI-Enhancement (Experimental)")
                    }
                }
            }
        }

        SettingsRow {
            title: qsTr("Stream Resolution Scale")
            description: qsTr("Renders the stream below the selected resolution and upscales it on the client.")

            Row {
                spacing: Theme.spaceSm

                HardSwitch {
                    id: streamResolutionScaleSwitch
                    anchors.verticalCenter: parent.verticalCenter
                    checked: StreamingPreferences.streamResolutionScale
                    onCheckedChanged: {
                        StreamingPreferences.streamResolutionScale = checked
                    }
                }

                HardTextField {
                    id: streamResolutionScaleRatioField
                    anchors.verticalCenter: parent.verticalCenter
                    maximumLength: 3
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator{bottom:20; top:100}
                    width: 64
                    visible: streamResolutionScaleSwitch.checked
                    text: StreamingPreferences.streamResolutionScaleRatio.toString()
                    onTextChanged: {
                        var value = parseInt(text);
                        if (!isNaN(value) && value >= 20 && value <= 100) {
                            StreamingPreferences.streamResolutionScaleRatio = value;
                        }
                    }
                }

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "%"
                    font.bold: true
                    visible: streamResolutionScaleSwitch.checked
                }
            }
        }
    }

    // ================= 远程覆盖 =================
    SettingsCard {
        title: qsTr("Remote overrides")
        subtitle: qsTr("Used instead of the values above when streaming over the internet.")

        SettingsRow {
            title: qsTr("Remote Resolution")

            Row {
                spacing: Theme.spaceSm

                HardSwitch {
                    id: remoteResolutionSwitch
                    anchors.verticalCenter: parent.verticalCenter
                    checked: StreamingPreferences.remoteResolution
                    onCheckedChanged: {
                        StreamingPreferences.remoteResolution = checked
                    }
                }

                HardTextField {
                    id: remoteResolutionWidthField
                    anchors.verticalCenter: parent.verticalCenter
                    maximumLength: 4
                    inputMethodHints: Qt.ImhDigitsOnly
                    placeholderText: "1280"
                    validator: IntValidator{bottom:256; top:8192}
                    width: 96
                    visible: remoteResolutionSwitch.checked
                    text: StreamingPreferences.remoteResolutionWidth > 0 ? StreamingPreferences.remoteResolutionWidth.toString() : ""
                    onEditingFinished: {
                        var value = parseInt(text);
                        if (!isNaN(value)) {
                            StreamingPreferences.remoteResolutionWidth = value;
                        }
                    }
                }

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "x"
                    font.bold: true
                    visible: remoteResolutionSwitch.checked
                }

                HardTextField {
                    id: remoteResolutionHeightField
                    anchors.verticalCenter: parent.verticalCenter
                    maximumLength: 4
                    inputMethodHints: Qt.ImhDigitsOnly
                    placeholderText: "720"
                    validator: IntValidator{bottom:256; top:8192}
                    width: 96
                    visible: remoteResolutionSwitch.checked
                    text: StreamingPreferences.remoteResolutionHeight > 0 ? StreamingPreferences.remoteResolutionHeight.toString() : ""
                    onEditingFinished: {
                        var value = parseInt(text);
                        if (!isNaN(value)) {
                            StreamingPreferences.remoteResolutionHeight = value;
                        }
                    }
                }
            }
        }

        SettingsRow {
            title: qsTr("Remote Frame Rate")

            Row {
                spacing: Theme.spaceSm

                HardSwitch {
                    id: remoteFpsSwitch
                    anchors.verticalCenter: parent.verticalCenter
                    checked: StreamingPreferences.remoteFps
                    onCheckedChanged: {
                        StreamingPreferences.remoteFps = checked
                    }
                }

                HardTextField {
                    id: remoteFpsRateField
                    anchors.verticalCenter: parent.verticalCenter
                    maximumLength: 3
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator{bottom:1; top:512}
                    placeholderText: "60"
                    width: 80
                    visible: remoteFpsSwitch.checked
                    text: StreamingPreferences.remoteFpsRate > 0 ? StreamingPreferences.remoteFpsRate.toString() : ""
                    onEditingFinished: {
                        var value = parseInt(text);
                        if (!isNaN(value)) {
                            StreamingPreferences.remoteFpsRate = value;
                        }
                    }
                }

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("FPS")
                    font.bold: true
                    visible: remoteFpsSwitch.checked
                }
            }
        }
    }
}
