import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2
import QtQuick.Window 2.2

import StreamingPreferences 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import SystemProperties 1.0

Flickable {
    id: settingsPage
    objectName: qsTr("Settings")
    topMargin: 60
    signal languageChanged()

    readonly property bool useCuteChineseTitleFont: StreamingPreferences.language === StreamingPreferences.LANG_ZH_CN ||
                                                   StreamingPreferences.language === StreamingPreferences.LANG_ZH_TW ||
                                                   (StreamingPreferences.language === StreamingPreferences.LANG_AUTO &&
                                                    Qt.locale().name.indexOf("zh") === 0)
    property font defaultTitleFont: Qt.font({ bold: true, pointSize: 13 })
    property font cuteChineseTitleFont: Qt.font({ family: "YouYuan", bold: true, pointSize: 13 })
    property font groupBoxTitleFont: useCuteChineseTitleFont ? cuteChineseTitleFont : defaultTitleFont

    boundsBehavior: Flickable.OvershootBounds

    contentWidth: settingsColumn1.width > settingsColumn2.width ? settingsColumn1.width : settingsColumn2.width
    contentHeight: settingsColumn1.height > settingsColumn2.height ? settingsColumn1.height : settingsColumn2.height

    ScrollBar.vertical: ScrollBar {
        anchors {
            left: parent.right
            leftMargin: -10
        }
    }

    function isChildOfFlickable(item) {
        while (item) {
            if (item.parent === contentItem) {
                return true
            }

            item = item.parent
        }
        return false
    }

    NumberAnimation on contentY {
        id: autoScrollAnimation
        duration: 100
    }

    Window.onActiveFocusItemChanged: {
        var item = Window.activeFocusItem
        if (item) {
            // Ignore non-child elements like the toolbar buttons
            if (!isChildOfFlickable(item)) {
                return
            }

            // Map the focus item's position into our content item's coordinate space
            var pos = item.mapToItem(contentItem, 0, 0)

            // Ensure some extra space is visible around the element we're scrolling to
            var scrollMargin = height > 100 ? 50 : 0

            if (pos.y - scrollMargin < contentY) {
                autoScrollAnimation.from = contentY
                autoScrollAnimation.to = Math.max(pos.y - scrollMargin, 0)
                autoScrollAnimation.start()
            }
            else if (pos.y + item.height + scrollMargin > contentY + height) {
                autoScrollAnimation.from = contentY
                autoScrollAnimation.to = Math.min(pos.y + item.height + scrollMargin - height, contentHeight - height)
                autoScrollAnimation.start()
            }
        }
    }

    StackView.onActivated: {
        // This enables Tab and BackTab based navigation rather than arrow keys.
        // It is required to shift focus between controls on the settings page.
        SdlGamepadKeyNavigation.setUiNavMode(true)

        // Highlight the first item if a gamepad is connected
        if (SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            resolutionComboBox.forceActiveFocus(Qt.TabFocus)
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

    PcView {
        id: pcViewPage
    }

    Rectangle {
        parent: settingsPage
        anchors.fill: parent
        z: -2

        Image {
            anchors.fill: parent
            source: pcViewPage.currentBgUrl || "qrc:/res/gura.png"
            opacity: 0.35
            fillMode: Image.PreserveAspectCrop
        }
    }

    Rectangle {
        parent: settingsPage
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        z: -1
    }

    Column {
        padding: 10
        id: settingsColumn1
        width: settingsPage.width / 2
        spacing: 15
        z: 1

        GroupBox {
            id: basicSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#FFA5D2\">⚙ " + qsTr("Basic Settings") + "</font></b>"
            font: settingsPage.groupBoxTitleFont

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
                    width: parent.width

                    AutoResizingComboBox {
                        property int lastIndexValue

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

                        id: resolutionComboBox
                        maximumWidth: parent.width / 2
                        textRole: "text"
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
                                    slider.value = Math.log(StreamingPreferences.bitrateKbps)
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
                                        resolutionListModel.setProperty(i, "text", "Custom ("+width+"x"+height+")")

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
                                    TextField {
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

                                    TextField {
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
                        property int lastIndexValue

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
                                    slider.value = Math.log(StreamingPreferences.bitrateKbps)
                                }
                            }

                            lastIndexValue = currentIndex
                        }

                        NavigableDialog {
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

                            id: customFpsDialog
                            standardButtons: Dialog.Ok | Dialog.Cancel
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
                                    TextField {
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
                            languageChanged.connect(reinitialize)
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

                        id: fpsComboBox
                        maximumWidth: parent.width / 2
                        textRole: "text"
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

                Label {
                    width: parent.width
                    id: bitrateTitle
                    text: qsTr("Video bitrate:")
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

                Row {
                    spacing: 5
                    width: parent.width

                    Slider {
                        id: slider

                        // 使用对数刻度来实现非线性调整
                        property real logMin: Math.log(500)
                        property real logMax: Math.log(2000000)
                        property real linearThreshold: 100000 // 100 Mbps 的线性调整阈值

                        value: Math.log(StreamingPreferences.bitrateKbps)
                        stepSize: (logMax - logMin) / 200
                        from: logMin
                        to: logMax

                        snapMode: "SnapOnRelease"
                        width: Math.min(bitrateDesc.implicitWidth, parent.width)

                        handle: Rectangle {
                            x: slider.visualPosition * (slider.width - width)
                            y: (slider.height - height) / 2
                            width: 24
                            height: 24
                            radius: 12
                            color: "#FFA5D2"
                            border.color: "#ffffff"
                            border.width: 2
                        }

                        background: Rectangle {
                            x: 0
                            y: (slider.height - height) / 2
                            width: slider.width
                            height: 6
                            radius: 3
                            color: "#e0e0e0"

                            Rectangle {
                                width: slider.visualPosition * parent.width
                                height: parent.height
                                color: "#FFA5D2"
                                radius: 3
                            }
                        }

                        onValueChanged: {
                            var linearValue;
                            if (Math.exp(value) <= linearThreshold) {
                                // 在 100 Mbps 以下使用线性调整
                                linearValue = Math.exp(value);
                            } else {
                                // 在 100 Mbps 以上使用对数调整
                                linearValue = Math.exp(value);
                            }

                            // 根据条件格式化显示文本
                            var displayValue = linearValue / 1000.0;
                            if (displayValue < 100) {
                                bitrateTitle.text = qsTr("Video bitrate: %1 Mbps").arg(displayValue.toFixed(1))
                            } else {
                                bitrateTitle.text = qsTr("Video bitrate: %1 Mbps").arg(Math.round(displayValue))
                            }

                            StreamingPreferences.bitrateKbps = linearValue
                        }

                        onMoved: {
                            StreamingPreferences.autoAdjustBitrate = false
                        }

                        Component.onCompleted: {
                            // Refresh the text after translations change
                            languageChanged.connect(valueChanged)
                        }
                    }
                }
                
                Button {
                    id: resetBitrateButton
                    text: qsTr("Use Default (%1 Mbps)").arg(StreamingPreferences.getDefaultBitrate(StreamingPreferences.width, StreamingPreferences.height, StreamingPreferences.fps, StreamingPreferences.enableYUV444) / 1000.0)
                    visible: StreamingPreferences.bitrateKbps !== StreamingPreferences.getDefaultBitrate(StreamingPreferences.width, StreamingPreferences.height, StreamingPreferences.fps, StreamingPreferences.enableYUV444)
                    onClicked: {
                        var defaultBitrate = StreamingPreferences.getDefaultBitrate(StreamingPreferences.width, StreamingPreferences.height, StreamingPreferences.fps, StreamingPreferences.enableYUV444)
                        StreamingPreferences.bitrateKbps = defaultBitrate
                        StreamingPreferences.autoAdjustBitrate = true
                        slider.value = Math.log(defaultBitrate)
                    }
                    
                    hoverEnabled: true
                }

                CheckBox {
                    id: sunshineAbrCheck
                    width: parent.width
                    hoverEnabled: true
                    text: qsTr("Smart bitrate with Sunshine")
                    font.pointSize: 12
                    checked: StreamingPreferences.enableSunshineAbr
                    onCheckedChanged: {
                        StreamingPreferences.enableSunshineAbr = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 8000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Allows Sunshine to automatically adjust stream bitrate up to the selected video bitrate when the host supports ABR.")
                }

                Label {
                    width: parent.width
                    id: windowModeTitle
                    text: qsTr("Display mode")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                    visible: SystemProperties.hasDesktopEnvironment
                }

                AutoResizingComboBox {
                    function createModel() {
                        var model = Qt.createQmlObject('import QtQuick 2.0; ListModel {}', parent, '')

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
                        if (!visible) {
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
                        languageChanged.connect(reinitialize)
                    }

                    id: windowModeComboBox
                    visible: SystemProperties.hasDesktopEnvironment
                    enabled: !SystemProperties.rendererAlwaysFullScreen
                    hoverEnabled: true
                    textRole: "text"
                    onActivated: {
                        StreamingPreferences.windowMode = model.get(currentIndex).val
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Fullscreen generally provides the best performance, but borderless windowed may work better with features like macOS Spaces, Alt+Tab, screenshot tools, on-screen overlays, etc.")
                }

                CheckBox {
                    id: ignoreAspectRatioCheck
                    width: parent.width
                    hoverEnabled: true
                    text: qsTr("Stretch presentation")
                    font.pointSize:  12
                    checked: StreamingPreferences.ignoreAspectRatio
                    onCheckedChanged: {
                        StreamingPreferences.ignoreAspectRatio = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 12000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Ignores both client and host PC aspect ratios, which is required for displaying Half-SBS (Side-By-Side) 3D signals to AR/XR devices that only support Full-SBS (usually 1920x1080 per eye, meaning a total resolution of 3840x1080)")
                }

                Row {
                    spacing: 5
                    width: parent.width

                    CheckBox {
                        id: vsyncCheck
                        hoverEnabled: true
                        text: qsTr("V-Sync")
                        font.pointSize:  12
                        checked: StreamingPreferences.enableVsync
                        onCheckedChanged: {
                            StreamingPreferences.enableVsync = checked
                        }

                        ToolTip.delay: 1000
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Disabling V-Sync allows sub-frame rendering latency, but it can display visible tearing")
                    }

                    CheckBox {
                        id: framePacingCheck
                        hoverEnabled: true
                        text: qsTr("Frame pacing")
                        font.pointSize:  12
                        enabled: StreamingPreferences.enableVsync
                        checked: StreamingPreferences.enableVsync && StreamingPreferences.framePacing
                        onCheckedChanged: {
                            StreamingPreferences.framePacing = checked
                        }
                        ToolTip.delay: 1000
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Frame pacing reduces micro-stutter by delaying frames that come in too early")
                    }
                }

                CheckBox {
                    id: enableHdr
                    width: parent.width
                    text: qsTr("Enable HDR")
                    font.pointSize: 12

                    enabled: SystemProperties.supportsHdr
                    checked: enabled && StreamingPreferences.enableHdr
                    onCheckedChanged: {
                        StreamingPreferences.enableHdr = checked
                    }

                    // Updating StreamingPreferences.videoCodecConfig is handled above

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: enabled ?
                                      qsTr("The stream will be HDR-capable, but some games may require an HDR monitor on your host PC to enable HDR mode.")
                                    :
                                      qsTr("HDR streaming is not supported on this PC.")
                }

                ComboBox {
                    id: hdrModeComboBox
                    width: parent.width
                    font.pointSize: 12
                    enabled: enableHdr.checked
                    textRole: "text"
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

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("HDR10 (PQ) is the standard HDR format. HLG offers better compatibility with SDR displays when HDR is not active on the host.")
                }

                Rectangle {
                    id: hdrBrightnessCard
                    width: parent.width
                    height: hdrBrightnessContent.implicitHeight + 24
                    radius: 10
                    color: "#1AFFFFFF"
                    border.width: 1
                    border.color: manualValuesValid ? "#35FFFFFF" : "#FFF06A6A"

                    property bool manualMode: StreamingPreferences.hdrBrightnessMode === StreamingPreferences.HBM_MANUAL
                    property bool manualValuesValid: StreamingPreferences.hdrMaxBrightness > 0 &&
                                                     StreamingPreferences.hdrMinBrightness >= 0 &&
                                                     StreamingPreferences.hdrMaxAverageBrightness > 0 &&
                                                     StreamingPreferences.hdrMinBrightness <= StreamingPreferences.hdrMaxAverageBrightness &&
                                                     StreamingPreferences.hdrMaxAverageBrightness <= StreamingPreferences.hdrMaxBrightness
                    property real previewMax: StreamingPreferences.hdrMaxBrightness
                    property real previewMin: StreamingPreferences.hdrMinBrightness
                    property real previewAverage: StreamingPreferences.hdrMaxAverageBrightness

                    function formatBrightness(value, decimals) {
                        return Number(Number(value).toFixed(decimals)).toString()
                    }

                    function pqPosition(value) {
                        // SMPTE ST 2084 maps absolute luminance (0-10000 nits)
                        // to a perceptually uniform HDR signal level.
                        var luminance = Math.max(0, Math.min(10000, value)) / 10000
                        if (luminance === 0) {
                            return 0
                        }

                        var m1 = 2610 / 16384
                        var m2 = 2523 / 32
                        var c1 = 3424 / 4096
                        var c2 = 2413 / 128
                        var c3 = 2392 / 128
                        var luminancePower = Math.pow(luminance, m1)
                        return Math.pow((c1 + c2 * luminancePower) /
                                        (1 + c3 * luminancePower), m2)
                    }

                    function brightnessFromPqPosition(position) {
                        var signal = Math.max(0, Math.min(1, position))
                        if (signal === 0) {
                            return 0
                        }

                        var m1 = 2610 / 16384
                        var m2 = 2523 / 32
                        var c1 = 3424 / 4096
                        var c2 = 2413 / 128
                        var c3 = 2392 / 128
                        var signalPower = Math.pow(signal, 1 / m2)
                        var numerator = Math.max(signalPower - c1, 0)
                        var denominator = c2 - c3 * signalPower
                        if (denominator <= 0) {
                            return 10000
                        }

                        return Math.pow(numerator / denominator, 1 / m1) * 10000
                    }

                    function scaleXForBrightness(value, scaleWidth) {
                        var edgePadding = 8
                        return edgePadding + pqPosition(value) *
                               Math.max(1, scaleWidth - edgePadding * 2)
                    }

                    function snapBrightness(value, handleKind, scaleWidth) {
                        var snapPoints = handleKind === "minimum"
                                ? [0, 0.0001, 0.001, 0.005, 0.01, 0.05, 0.1, 1, 5, 10]
                                : [1, 10, 80, 100, 200, 400, 600, 1000, 1600, 2000, 4000, 10000]
                        var valuePosition = pqPosition(value)
                        var snapThreshold = 8 / Math.max(1, scaleWidth - 16)
                        var closest = value
                        var closestDistance = snapThreshold

                        for (var i = 0; i < snapPoints.length; i++) {
                            var distance = Math.abs(pqPosition(snapPoints[i]) - valuePosition)
                            if (distance <= closestDistance) {
                                closest = snapPoints[i]
                                closestDistance = distance
                            }
                        }

                        return closest
                    }

                    function roundedBrightness(value, handleKind) {
                        if (handleKind === "minimum") {
                            if (value < 0.01) {
                                return Number(value.toFixed(6))
                            }
                            if (value < 1) {
                                return Number(value.toFixed(3))
                            }
                            return Number(value.toFixed(2))
                        }

                        return value < 100 ? Number(value.toFixed(1)) : Math.round(value)
                    }

                    function setBrightnessFromScale(handleKind, scaleX, scaleWidth) {
                        var edgePadding = 8
                        var position = (scaleX - edgePadding) /
                                       Math.max(1, scaleWidth - edgePadding * 2)
                        var value = brightnessFromPqPosition(position)
                        value = snapBrightness(value, handleKind, scaleWidth)

                        if (handleKind === "minimum") {
                            value = Math.max(0, Math.min(10, previewAverage, value))
                            StreamingPreferences.hdrMinBrightness = roundedBrightness(value, handleKind)
                        }
                        else if (handleKind === "average") {
                            value = Math.max(1, previewMin, Math.min(previewMax, value))
                            StreamingPreferences.hdrMaxAverageBrightness = roundedBrightness(value, handleKind)
                        }
                        else {
                            value = Math.max(1, previewAverage, Math.min(10000, value))
                            StreamingPreferences.hdrMaxBrightness = roundedBrightness(value, handleKind)
                        }
                    }

                    function nudgeBrightness(handleKind, direction, accelerated) {
                        var currentValue
                        var step

                        if (handleKind === "minimum") {
                            currentValue = previewMin
                            step = currentValue < 0.01 ? 0.001 :
                                   currentValue < 1 ? 0.01 : 0.1
                        }
                        else {
                            currentValue = handleKind === "average" ? previewAverage : previewMax
                            step = currentValue < 100 ? 1 : currentValue < 1000 ? 10 : 100
                        }

                        if (accelerated) {
                            step *= 10
                        }

                        var newValue = currentValue + direction * step
                        if (handleKind === "minimum") {
                            StreamingPreferences.hdrMinBrightness =
                                    roundedBrightness(Math.max(0, Math.min(10, previewAverage, newValue)), handleKind)
                        }
                        else if (handleKind === "average") {
                            StreamingPreferences.hdrMaxAverageBrightness =
                                    roundedBrightness(Math.max(1, previewMin, Math.min(previewMax, newValue)), handleKind)
                        }
                        else {
                            StreamingPreferences.hdrMaxBrightness =
                                    roundedBrightness(Math.max(1, previewAverage, Math.min(10000, newValue)), handleKind)
                        }
                    }

                    Column {
                        id: hdrBrightnessContent
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            margins: 12
                        }
                        spacing: 10

                        RowLayout {
                            width: parent.width
                            spacing: 8

                            Column {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: qsTr("HDR brightness profile")
                                    color: "#FFFFFF"
                                    font.bold: true
                                    font.pointSize: 12
                                }

                                Label {
                                    width: parent.width
                                    text: qsTr("Controls the display metadata used by Foundation Sunshine's virtual display.")
                                    color: "#BFFFFFFF"
                                    wrapMode: Text.Wrap
                                    font.pointSize: 9
                                }
                            }

                            Rectangle {
                                implicitWidth: profileStatusLabel.implicitWidth + 16
                                implicitHeight: profileStatusLabel.implicitHeight + 8
                                radius: height / 2
                                color: enableHdr.checked ? "#304CAF50" : "#30FFB74D"

                                Label {
                                    id: profileStatusLabel
                                    anchors.centerIn: parent
                                    text: enableHdr.checked ? qsTr("HDR active") : qsTr("Enable HDR to use")
                                    color: enableHdr.checked ? "#A5D6A7" : "#FFCC80"
                                    font.bold: true
                                    font.pointSize: 8
                                }
                            }
                        }

                        ComboBox {
                            id: hdrBrightnessModeComboBox
                            width: parent.width
                            font.pointSize: 11
                            currentIndex: StreamingPreferences.hdrBrightnessMode
                            model: [
                                qsTr("Use host defaults"),
                                qsTr("Detect client display automatically"),
                                qsTr("Set brightness manually")
                            ]
                            onActivated: {
                                StreamingPreferences.hdrBrightnessMode = currentIndex
                            }
                        }

                        Label {
                            width: parent.width
                            color: "#D9FFFFFF"
                            wrapMode: Text.Wrap
                            font.pointSize: 9
                            text: {
                                switch (StreamingPreferences.hdrBrightnessMode) {
                                case StreamingPreferences.HBM_HOST_DEFAULT:
                                    return qsTr("No brightness values are sent. Foundation Sunshine will use its configured defaults.")
                                case StreamingPreferences.HBM_AUTO:
                                    return qsTr("The HDR display is detected when streaming starts. Automatic detection is currently available on Windows.")
                                case StreamingPreferences.HBM_MANUAL:
                                    return qsTr("Use calibrated values when display detection is unavailable or reports inaccurate metadata.")
                                default:
                                    return ""
                                }
                            }
                        }

                        GridLayout {
                            width: parent.width
                            columns: 3
                            columnSpacing: 8
                            rowSpacing: 6
                            visible: hdrBrightnessCard.manualMode

                            Label {
                                text: qsTr("Peak brightness")
                                color: "#F0FFFFFF"
                                Layout.fillWidth: true
                            }

                            TextField {
                                id: hdrMaxBrightnessField
                                Layout.preferredWidth: 100
                                horizontalAlignment: TextInput.AlignRight
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                text: hdrBrightnessCard.formatBrightness(StreamingPreferences.hdrMaxBrightness, 3)
                                validator: DoubleValidator {
                                    bottom: 1
                                    top: 10000
                                    decimals: 3
                                    notation: DoubleValidator.StandardNotation
                                }
                                onTextEdited: {
                                    var value = Number(text)
                                    if (acceptableInput && !isNaN(value)) {
                                        StreamingPreferences.hdrMaxBrightness = value
                                    }
                                }
                                onEditingFinished: {
                                    if (!acceptableInput) {
                                        text = hdrBrightnessCard.formatBrightness(StreamingPreferences.hdrMaxBrightness, 3)
                                    }
                                }
                            }

                            Label {
                                text: qsTr("nits")
                                color: "#BFFFFFFF"
                            }

                            Label {
                                text: qsTr("Minimum brightness")
                                color: "#F0FFFFFF"
                                Layout.fillWidth: true
                            }

                            TextField {
                                id: hdrMinBrightnessField
                                Layout.preferredWidth: 100
                                horizontalAlignment: TextInput.AlignRight
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                text: hdrBrightnessCard.formatBrightness(StreamingPreferences.hdrMinBrightness, 6)
                                validator: DoubleValidator {
                                    bottom: 0
                                    top: 10
                                    decimals: 6
                                    notation: DoubleValidator.StandardNotation
                                }
                                onTextEdited: {
                                    var value = Number(text)
                                    if (acceptableInput && !isNaN(value)) {
                                        StreamingPreferences.hdrMinBrightness = value
                                    }
                                }
                                onEditingFinished: {
                                    if (!acceptableInput) {
                                        text = hdrBrightnessCard.formatBrightness(StreamingPreferences.hdrMinBrightness, 6)
                                    }
                                }
                            }

                            Label {
                                text: qsTr("nits")
                                color: "#BFFFFFFF"
                            }

                            Label {
                                text: qsTr("Full-frame brightness")
                                color: "#F0FFFFFF"
                                Layout.fillWidth: true
                            }

                            TextField {
                                id: hdrMaxAverageBrightnessField
                                Layout.preferredWidth: 100
                                horizontalAlignment: TextInput.AlignRight
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                text: hdrBrightnessCard.formatBrightness(StreamingPreferences.hdrMaxAverageBrightness, 3)
                                validator: DoubleValidator {
                                    bottom: 1
                                    top: 10000
                                    decimals: 3
                                    notation: DoubleValidator.StandardNotation
                                }
                                onTextEdited: {
                                    var value = Number(text)
                                    if (acceptableInput && !isNaN(value)) {
                                        StreamingPreferences.hdrMaxAverageBrightness = value
                                    }
                                }
                                onEditingFinished: {
                                    if (!acceptableInput) {
                                        text = hdrBrightnessCard.formatBrightness(StreamingPreferences.hdrMaxAverageBrightness, 3)
                                    }
                                }
                            }

                            Label {
                                text: qsTr("nits")
                                color: "#BFFFFFFF"
                            }
                        }

                        Column {
                            width: parent.width
                            spacing: 8
                            visible: hdrBrightnessCard.manualMode

                            Label {
                                text: qsTr("HDR luminance range")
                                color: "#D9FFFFFF"
                                font.bold: true
                                font.pointSize: 9
                            }

                            Label {
                                width: parent.width
                                text: qsTr("Perceptual PQ scale · drag the markers to adjust. Positions show HDR signal levels, not actual screen brightness.")
                                color: "#8FFFFFFF"
                                wrapMode: Text.Wrap
                                font.pointSize: 8
                            }

                            Item {
                                width: parent.width
                                height: 54

                                Repeater {
                                    model: [
                                        { value: 0, label: "0" },
                                        { value: 100, label: qsTr("SDR white") + " · 100" },
                                        { value: 1000, label: "1000" },
                                        { value: 10000, label: qsTr("PQ limit") + " · 10000" }
                                    ]

                                    Item {
                                        property real tickPosition: hdrBrightnessCard.pqPosition(modelData.value)
                                        x: Math.max(0, Math.min(parent.width - width,
                                                               tickPosition * parent.width - width / 2))
                                        width: tickLabel.implicitWidth
                                        height: parent.height

                                        Rectangle {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            y: 17
                                            width: 1
                                            height: 10
                                            color: "#45FFFFFF"
                                        }

                                        Label {
                                            id: tickLabel
                                            anchors.bottom: parent.bottom
                                            text: modelData.label
                                            color: "#70FFFFFF"
                                            font.pointSize: 7
                                        }
                                    }
                                }

                                Rectangle {
                                    id: hdrBrightnessScale
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                    }
                                    y: 8
                                    height: 14
                                    radius: height / 2
                                    border.width: 1
                                    border.color: "#45FFFFFF"
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop { position: 0.0; color: "#11161D" }
                                        GradientStop { position: 0.55; color: "#25303C" }
                                        GradientStop { position: 1.0; color: "#3A4654" }
                                    }

                                    Rectangle {
                                        property real rangeStart: hdrBrightnessCard.scaleXForBrightness(
                                                                              hdrBrightnessCard.previewMin,
                                                                              parent.width)
                                        property real rangeEnd: hdrBrightnessCard.scaleXForBrightness(
                                                                            hdrBrightnessCard.previewAverage,
                                                                            parent.width)
                                        x: rangeStart
                                        width: Math.max(0, rangeEnd - rangeStart)
                                        height: parent.height - 4
                                        radius: height / 2
                                        anchors.verticalCenter: parent.verticalCenter
                                        gradient: Gradient {
                                            orientation: Gradient.Horizontal
                                            GradientStop { position: 0.0; color: "#376A8D" }
                                            GradientStop { position: 1.0; color: "#8CB8D5" }
                                        }
                                    }

                                    Rectangle {
                                        property real rangeStart: hdrBrightnessCard.scaleXForBrightness(
                                                                              hdrBrightnessCard.previewAverage,
                                                                              parent.width)
                                        property real rangeEnd: hdrBrightnessCard.scaleXForBrightness(
                                                                            hdrBrightnessCard.previewMax,
                                                                            parent.width)
                                        x: rangeStart
                                        width: Math.max(0, rangeEnd - rangeStart)
                                        height: parent.height - 4
                                        radius: height / 2
                                        anchors.verticalCenter: parent.verticalCenter
                                        gradient: Gradient {
                                            orientation: Gradient.Horizontal
                                            GradientStop { position: 0.0; color: "#8CB8D5" }
                                            GradientStop { position: 1.0; color: "#E9F3FA" }
                                        }
                                    }

                                    HdrBrightnessHandle {
                                        handleStyle: minimumStyle
                                        scaleItem: hdrBrightnessScale
                                        brightnessValue: hdrBrightnessCard.previewMin
                                        maximumValue: Math.min(10, hdrBrightnessCard.previewAverage)
                                        decimalPlaces: 6
                                        valueLabel: qsTr("Minimum brightness")
                                        unitLabel: qsTr("nits")
                                        restingZ: 3
                                        draggedZ: 5
                                        x: hdrBrightnessCard.scaleXForBrightness(
                                                   brightnessValue, parent.width) - width / 2
                                        anchors.verticalCenter: parent.verticalCenter

                                        onDragMoved: hdrBrightnessCard.setBrightnessFromScale(
                                                           "minimum", scaleX, hdrBrightnessScale.width)
                                        onNudgeRequested: hdrBrightnessCard.nudgeBrightness(
                                                               "minimum", direction, accelerated)
                                    }

                                    HdrBrightnessHandle {
                                        handleStyle: averageStyle
                                        scaleItem: hdrBrightnessScale
                                        brightnessValue: hdrBrightnessCard.previewAverage
                                        minimumValue: Math.max(1, hdrBrightnessCard.previewMin)
                                        maximumValue: hdrBrightnessCard.previewMax
                                        valueLabel: qsTr("Full-frame brightness")
                                        unitLabel: qsTr("nits")
                                        restingZ: 4
                                        draggedZ: 6
                                        x: hdrBrightnessCard.scaleXForBrightness(
                                                   brightnessValue, parent.width) - width / 2
                                        anchors.verticalCenter: parent.verticalCenter

                                        onDragMoved: hdrBrightnessCard.setBrightnessFromScale(
                                                           "average", scaleX, hdrBrightnessScale.width)
                                        onNudgeRequested: hdrBrightnessCard.nudgeBrightness(
                                                               "average", direction, accelerated)
                                    }

                                    HdrBrightnessHandle {
                                        handleStyle: peakStyle
                                        scaleItem: hdrBrightnessScale
                                        brightnessValue: hdrBrightnessCard.previewMax
                                        minimumValue: Math.max(1, hdrBrightnessCard.previewAverage)
                                        valueLabel: qsTr("Peak brightness")
                                        unitLabel: qsTr("nits")
                                        restingZ: 5
                                        draggedZ: 7
                                        x: hdrBrightnessCard.scaleXForBrightness(
                                                   brightnessValue, parent.width) - width / 2
                                        anchors.verticalCenter: parent.verticalCenter

                                        onDragMoved: hdrBrightnessCard.setBrightnessFromScale(
                                                           "peak", scaleX, hdrBrightnessScale.width)
                                        onNudgeRequested: hdrBrightnessCard.nudgeBrightness(
                                                               "peak", direction, accelerated)
                                    }
                                }
                            }

                            RowLayout {
                                width: parent.width
                                spacing: 6

                                Row {
                                    spacing: 4

                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 7
                                        height: 7
                                        radius: width / 2
                                        color: "#64B5F6"
                                    }

                                    Label {
                                        text: qsTr("Minimum brightness") + "  " +
                                              hdrBrightnessCard.formatBrightness(hdrBrightnessCard.previewMin, 6)
                                        color: "#A8D9F5"
                                        font.pointSize: 8
                                    }
                                }

                                Item {
                                    Layout.fillWidth: true
                                }

                                Row {
                                    spacing: 4

                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 3
                                        height: 10
                                        radius: 2
                                        color: "#A8D3EC"
                                    }

                                    Label {
                                        text: qsTr("Full frame") + "  " +
                                              hdrBrightnessCard.formatBrightness(hdrBrightnessCard.previewAverage, 3)
                                        color: "#C4E2F2"
                                        font.pointSize: 8
                                    }
                                }

                                Item {
                                    Layout.fillWidth: true
                                }

                                Row {
                                    spacing: 4

                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 8
                                        height: 8
                                        radius: width / 2
                                        color: "#F4FAFF"
                                        border.width: 1
                                        border.color: "#FFB86B"
                                    }

                                    Label {
                                        text: qsTr("Peak") + "  " +
                                              hdrBrightnessCard.formatBrightness(hdrBrightnessCard.previewMax, 3)
                                        color: "#FFD1A3"
                                        font.pointSize: 8
                                    }
                                }
                            }
                        }

                        Label {
                            width: parent.width
                            visible: hdrBrightnessCard.manualMode && !hdrBrightnessCard.manualValuesValid
                            text: qsTr("Enter values in this order: minimum ≤ full-frame ≤ peak brightness.")
                            color: "#FFFF8A80"
                            wrapMode: Text.Wrap
                            font.bold: true
                            font.pointSize: 9
                        }
                    }
                }

                CheckBox {
                    id: videoEnhancementCheck
                    width: parent.width
                    hoverEnabled: true
                    text: qsTr("Video AI-Enhancement")
                    font.pointSize:  12
                    enabled: SystemProperties.isVideoEnhancementCapable()
                    checked: {
                        return SystemProperties.isVideoEnhancementCapable() && StreamingPreferences.videoEnhancement
                    }
                    property bool keepValue: checked;
                    onCheckedChanged: {
                        StreamingPreferences.videoEnhancement = checked
                    }
                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text:
                        qsTr("Enhance video quality by utilizing the GPU's AI-Enhancement capabilities.") + "\n" +
                        qsTr("This feature effectively upscales, reduces compression artifacts and enhances the clarity of streamed content.")+ "\n" + 
                        qsTr("Note:")+ "\n" + 
                        qsTr("If available, ensure that appropriate settings (i.e. RTX Video enhancement) are enabled in your GPU driver configuration.")+ "\n" + 
                        qsTr("HDR rendering has divers issues depending on the GPU used, we are working on it but we advise to currently use Non-HDR.")+ "\n" + 
                        qsTr("Be advised that using this feature on laptops running on battery power may lead to significant battery drain.")

                    Component.onCompleted: {
                        if (!SystemProperties.isVideoEnhancementCapable()){
                            // VSR or SDR->HDR feature could not be initialized by any GPU available
                            text = qsTr("Video AI-Enhancement (Not supported by the GPU)")
                            enabled = false;
                            checked = false;
                        } else if(SystemProperties.isVideoEnhancementExperimental()){
                            // Indicate if the feature is available but not officially deployed by the Vendor
                            text = qsTr("Video AI-Enhancement (Experimental)")
                        }
                    }
                }

                // Stream Resolution Scale
                RowLayout {
                    CheckBox {
                        id: streamResolutionScaleCheck
                        text: qsTr("Stream Resolution Scale")
                        font.pointSize: 12
                        checked: StreamingPreferences.streamResolutionScale
                        onCheckedChanged: {
                            StreamingPreferences.streamResolutionScale = checked
                        }
                    }

                    TextField {
                        id: streamResolutionScaleRatioField
                        maximumLength: 3
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator{bottom:20; top:100}
                        Layout.preferredWidth: 60
                        visible: streamResolutionScaleCheck.checked
                        text: StreamingPreferences.streamResolutionScaleRatio.toString()
                        onTextChanged: {
                            let value = parseInt(text);
                            if (!isNaN(value) && value >= 20 && value <= 100) {
                                StreamingPreferences.streamResolutionScaleRatio = value;
                            }
                        }
                    }

                    Label {
                        text: "%"
                        font.bold: true
                        visible: streamResolutionScaleCheck.checked
                    }
                }

                // Remote Resolution
                RowLayout {
                    CheckBox {
                        id: remoteResolutionCheck
                        Layout.fillWidth: true
                        text: qsTr("Remote Resolution")
                        font.pointSize: 12
                        checked: StreamingPreferences.remoteResolution
                        onCheckedChanged: {
                            StreamingPreferences.remoteResolution = checked
                        }
                    }

                    TextField {
                        id: remoteResolutionWidthField
                        maximumLength: 4
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: "1280"
                        validator: IntValidator{bottom:256; top:8192}
                        Layout.preferredWidth: 120
                        visible: remoteResolutionCheck.checked
                        text: StreamingPreferences.remoteResolutionWidth > 0 ? StreamingPreferences.remoteResolutionWidth.toString() : "800"
                        onTextChanged: {
                            let value = parseInt(text);
                            if (!isNaN(value)) {
                                StreamingPreferences.remoteResolutionWidth = value;
                            }
                        }
                    }
                    
                    Label {
                        text: "x"
                        font.bold: true
                        visible: remoteResolutionCheck.checked
                        Layout.alignment: Qt.AlignVCenter
                    }
                    
                    TextField {
                        id: remoteResolutionHeightField
                        maximumLength: 4
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: "720"
                        validator: IntValidator{bottom:256; top:8192}
                        Layout.preferredWidth: 120
                        visible: remoteResolutionCheck.checked
                        text: StreamingPreferences.remoteResolutionHeight > 0 ? StreamingPreferences.remoteResolutionHeight.toString() : "600"
                        onTextChanged: {
                            let value = parseInt(text);
                            if (!isNaN(value)) {
                                StreamingPreferences.remoteResolutionHeight = value;
                            }
                        }
                    }
                }

                // Remote Frame Rate
                RowLayout {
                    CheckBox {
                        id: remoteFpsCheck
                        text: qsTr("Remote Frame Rate")
                        font.pointSize: 12
                        checked: StreamingPreferences.remoteFps
                        onCheckedChanged: {
                            StreamingPreferences.remoteFps = checked
                        }
                    }
                    
                    TextField {
                        id: remoteFpsRateField
                        maximumLength: 3
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator{bottom:1; top:512}
                        Layout.preferredWidth: 80
                        visible: remoteFpsCheck.checked
                        text: StreamingPreferences.remoteFpsRate > 0 ? StreamingPreferences.remoteFpsRate.toString() : "60"
                        onTextChanged: {
                            let value = parseInt(text);
                            if (!isNaN(value)) {
                                StreamingPreferences.remoteFpsRate = value;
                            }
                        }
                    }
                    
                    Label {
                        id: fpsLabel
                        text: qsTr("FPS")
                        font.bold: true
                        visible: remoteFpsCheck.checked
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }
        }

        GroupBox {
            id: audioSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#FFA5D2\">🎵 " + qsTr("Audio Settings") + "</font></b>"
            font: settingsPage.groupBoxTitleFont

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


                CheckBox {
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

                CheckBox {
                    id: muteOnFocusLossCheck
                    width: parent.width
                    text: qsTr("Mute audio stream when Moonlight is not the active window")
                    font.pointSize: 12
                    visible: SystemProperties.hasDesktopEnvironment
                    checked: StreamingPreferences.muteOnFocusLoss
                    onCheckedChanged: {
                        StreamingPreferences.muteOnFocusLoss = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Mutes Moonlight's audio when you Alt+Tab out of the stream or click on a different window.")
                }
                CheckBox {
                    id: enableMicCheck
                    width: parent.width
                    text: qsTr("Enable microphone streaming(test)")
                    font.pointSize: 12
                    checked: StreamingPreferences.enableMicrophone
                    onCheckedChanged: {
                        StreamingPreferences.enableMicrophone = checked
                    }
                }
            }
        }

        GroupBox {
            id: hostSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#FFA5D2\">🖥️ " + qsTr("Host Settings") + "</font></b>"
            font: settingsPage.groupBoxTitleFont

            Column {
                anchors.fill: parent
                spacing: 5

                // 添加自定义屏幕模式选择器
                Label {
                    id: customScreenModeTitle
                    width: parent.width
                    text: qsTr("Custom Screen Mode")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_mode = (StreamingPreferences.customScreenMode !== undefined && 
                                          StreamingPreferences.customScreenMode !== null) ? 
                                          StreamingPreferences.customScreenMode : -1
                        currentIndex = 0
                        for (var i = 0; i < customScreenModeListModel.count; i++) {
                            var el_mode = customScreenModeListModel.get(i).val;
                            if (saved_mode === el_mode) {
                                currentIndex = i
                                break
                            }
                        }
                        activated(currentIndex)
                    }

                    id: customScreenModeComboBox
                    textRole: "text"
                    model: ListModel {
                        id: customScreenModeListModel
                        ListElement {
                            text: qsTr("Nothing")
                            val: -1
                        }
                        ListElement {
                            text: qsTr("Disabled")
                            val: 0
                        }
                        ListElement {
                            text: qsTr("Activate the display automatically")
                            val: 1
                        }
                        ListElement {
                            text: qsTr("Activate the display automatically and make it a primary display")
                            val: 2
                        }
                        ListElement {
                            text: qsTr("Deactivate other displays and activate only the specified display")
                            val: 3
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated: {
                        if (enabled) {
                            StreamingPreferences.customScreenMode = customScreenModeListModel.get(currentIndex).val
                        }
                    }
                }

                // VDD 屏幕模式选择器
                Label {
                    id: customVddScreenModeTitle
                    width: parent.width
                    text: qsTr("VDD Screen Mode")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                AutoResizingComboBox {
                    Component.onCompleted: {
                        var saved_mode = (StreamingPreferences.customVddScreenMode !== undefined &&
                                          StreamingPreferences.customVddScreenMode !== null) ?
                                          StreamingPreferences.customVddScreenMode : -1
                        currentIndex = 0
                        for (var i = 0; i < customVddScreenModeListModel.count; i++) {
                            var el_mode = customVddScreenModeListModel.get(i).val;
                            if (saved_mode === el_mode) {
                                currentIndex = i
                                break
                            }
                        }
                        activated(currentIndex)
                    }

                    id: customVddScreenModeComboBox
                    textRole: "text"
                    model: ListModel {
                        id: customVddScreenModeListModel
                        ListElement {
                            text: qsTr("Use Sunshine host configuration (default)")
                            val: -1
                        }
                        ListElement {
                            text: qsTr("Keep current layout")
                            val: 0
                        }
                        ListElement {
                            text: qsTr("VDD primary + Physical extended")
                            val: 1
                        }
                        ListElement {
                            text: qsTr("Physical primary + VDD extended")
                            val: 2
                        }
                        ListElement {
                            text: qsTr("VDD only (disable physical displays)")
                            val: 3
                        }
                    }
                    onActivated: {
                        if (enabled) {
                            StreamingPreferences.customVddScreenMode = customVddScreenModeListModel.get(currentIndex).val
                        }
                    }
                }

                CheckBox {
                    id: optimizeGameSettingsCheck
                    width: parent.width
                    text: qsTr("Optimize game settings for streaming")
                    font.pointSize:  12
                    checked: StreamingPreferences.gameOptimizations
                    onCheckedChanged: {
                        StreamingPreferences.gameOptimizations = checked
                    }
                }

                CheckBox {
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

        GroupBox {
            id: uiSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#FFA5D2\">🎨 " + qsTr("UI Settings") + "</font></b>"
            font: settingsPage.groupBoxTitleFont

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
                                ToolTip.show(qsTr("You must restart Moonlight for this change to take effect"), 5000)
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
                        if (!visible) {
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

                CheckBox {
                    id: connectionWarningsCheck
                    width: parent.width
                    text: qsTr("Show connection quality warnings")
                    font.pointSize: 12
                    checked: StreamingPreferences.connectionWarnings
                    onCheckedChanged: {
                        StreamingPreferences.connectionWarnings = checked
                    }
                }

                CheckBox {
                    id: configurationWarningsCheck
                    width: parent.width
                    text: qsTr("Show configuration warnings")
                    font.pointSize: 12
                    checked: StreamingPreferences.configurationWarnings
                    onCheckedChanged: {
                        StreamingPreferences.configurationWarnings = checked
                    }
                }

                CheckBox {
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

                CheckBox {
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

                CheckBox {
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
                    ToolTip.text: qsTr("Check for new versions of Moonlight when the app starts.")
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
    }

    Column {
        padding: 10
        rightPadding: 20
        anchors.left: settingsColumn1.right
        id: settingsColumn2
        width: settingsPage.width / 2
        spacing: 15

        GroupBox {
            id: inputSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#FFA5D2\">⌨️ " + qsTr("Input Settings") + "</font></b>"
            font: settingsPage.groupBoxTitleFont

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
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

                CheckBox {
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

                    CheckBox {
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
                                      qsTr("NOTE: Certain keyboard shortcuts like Ctrl+Alt+Del on Windows cannot be intercepted by any application, including Moonlight.")
                    }

                    AutoResizingComboBox {
                        // ignore setting the index at first, and actually set it when the component is loaded
                        Component.onCompleted: {
                            if (!visible) {
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

                CheckBox {
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

                CheckBox {
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

                CheckBox {
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
                
                CheckBox {
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

                CheckBox {
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

        GroupBox {
            id: gamepadSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#FFA5D2\">🎮 " + qsTr("Gamepad Settings") + "</font></b>"
            font: settingsPage.groupBoxTitleFont

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
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

                CheckBox {
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

                CheckBox {
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

                CheckBox {
                    id: backgroundGamepadCheck
                    width: parent.width
                    text: qsTr("Process gamepad input when Moonlight is in the background")
                    font.pointSize: 12
                    visible: SystemProperties.hasDesktopEnvironment
                    checked: StreamingPreferences.backgroundGamepad
                    onCheckedChanged: {
                        StreamingPreferences.backgroundGamepad = checked
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Allows Moonlight to capture gamepad inputs even if it's not the current window in focus")
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

        GroupBox {
            id: advancedSettingsGroupBox
            width: (parent.width - (parent.leftPadding + parent.rightPadding))
            padding: 12
            title: "<b><font color=\"#FFA5D2\">⚗️ " + qsTr("Advanced Settings") + "</font></b>"
            font: settingsPage.groupBoxTitleFont

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

                CheckBox {
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
                                slider.value = Math.log(StreamingPreferences.bitrateKbps)
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

                CheckBox {
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

                CheckBox {
                    id: detectNetworkBlocking
                    width: parent.width
                    text: qsTr("Automatically detect blocked connections (Recommended)")
                    font.pointSize: 12
                    checked: StreamingPreferences.detectNetworkBlocking
                    onCheckedChanged: {
                        StreamingPreferences.detectNetworkBlocking = checked
                    }
                }

                CheckBox {
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
}
