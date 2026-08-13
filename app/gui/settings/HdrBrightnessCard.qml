import QtQuick 2.9
import QtQuick.Controls
import QtQuick.Layouts 1.3

import StreamingPreferences 1.0
import SystemProperties 1.0

// 从旧 SettingsView.qml 1030-1644 行原样抽出，仅改动根节点宽度绑定与外壳配色。
import "."
import "../theme"
import ".."

Item {
    id: hdrBrightnessCard
    width: parent.width
    // 底部留出硬投影的高度
    height: hdrBrightnessContent.implicitHeight + Theme.spaceLg * 2 + Theme.shadowOffset

    // 方角硬投影面板。数值不合法时整块的描边和粗条都转成珊瑚色 ——
    // 比原来只换一圈 1px 描边更扫得见。
    Panel {
        anchors {
            fill: parent
            rightMargin: Theme.shadowOffset
            bottomMargin: Theme.shadowOffset
        }
        fill: Theme.surface2Layer
        borderColor: hdrBrightnessCard.manualValuesValid ? Theme.line : Theme.danger
        accentBarColor: hdrBrightnessCard.manualValuesValid ? Theme.accentDim : Theme.danger
        accentBarWidth: Theme.accentBar
    }

    // 由外部注入：HDR 开关的当前状态（旧代码直接引用 SettingsView 里的 enableHdr id）
    property bool hdrEnabled: SystemProperties.supportsHdr && StreamingPreferences.enableHdr

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
            margins: Theme.spaceLg
            // 躲开左侧粗条和右侧投影
            leftMargin: Theme.spaceLg + Theme.accentBar
            rightMargin: Theme.spaceLg + Theme.shadowOffset
        }
        spacing: Theme.spaceMd

        RowLayout {
            width: parent.width
            spacing: Theme.spaceSm

            Column {
                Layout.fillWidth: true
                spacing: Theme.spaceXs

                Text {
                    text: qsTr("HDR brightness profile")
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pointSize: Theme.fontRowTitle
                    font.weight: Font.ExtraBold
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: Theme.tracking(Theme.fontRowTitle, 0.06)
                }

                Text {
                    width: parent.width
                    text: qsTr("Controls the display metadata used by Foundation Sunshine's virtual display.")
                    color: Theme.textDim
                    font.family: Theme.fontMono
                    font.pointSize: Theme.fontCaption
                    wrapMode: Text.Wrap
                }
            }

            // 状态角标：方角，生效时用酸性绿（这是「正在生效」的状态标记），
            // 未生效时用珊瑚色描边提示要先打开 HDR。
            Rectangle {
                implicitWidth: profileStatusLabel.implicitWidth + Theme.spaceSm * 2
                implicitHeight: profileStatusLabel.implicitHeight + Theme.spaceXs * 2
                radius: 0
                color: hdrBrightnessCard.hdrEnabled ? Theme.acid : "transparent"
                border.width: hdrBrightnessCard.hdrEnabled ? 0 : 1
                border.color: Theme.danger

                MicroLabel {
                    id: profileStatusLabel
                    anchors.centerIn: parent
                    text: hdrBrightnessCard.hdrEnabled ? qsTr("HDR active") : qsTr("Enable HDR to use")
                    color: hdrBrightnessCard.hdrEnabled ? Theme.ink : Theme.danger
                }
            }
        }

        AutoResizingComboBox {
            id: hdrBrightnessModeComboBox
            width: parent.width
            maximumWidth: parent.width
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

        Text {
            width: parent.width
            color: Theme.textDim
            font.family: Theme.fontMono
            font.pointSize: Theme.fontCaption
            wrapMode: Text.Wrap
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
            columnSpacing: Theme.spaceSm
            rowSpacing: Theme.spaceSm
            visible: hdrBrightnessCard.manualMode

            Text {
                text: qsTr("Peak brightness")
                color: Theme.text
                font.family: Theme.fontSans
                font.pointSize: Theme.fontBody
                Layout.fillWidth: true
            }

            TextField {
                id: hdrMaxBrightnessField
                Layout.preferredWidth: 100
                horizontalAlignment: TextInput.AlignRight
                color: Theme.text
                font.family: Theme.fontMono
                font.pointSize: Theme.fontBody
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                text: hdrBrightnessCard.formatBrightness(StreamingPreferences.hdrMaxBrightness, 3)

                // FluentWinUI3 的 TextField 背景是圆角 + 底部一条粗下划线，换成方角框
                background: Rectangle {
                    radius: 0
                    color: Theme.ink
                    border.width: hdrMaxBrightnessField.activeFocus ? 2 : 1
                    border.color: hdrMaxBrightnessField.activeFocus ? Theme.accent : Theme.lineStrong
                }

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

            MicroLabel {
                text: qsTr("nits")
            }

            Text {
                text: qsTr("Minimum brightness")
                color: Theme.text
                font.family: Theme.fontSans
                font.pointSize: Theme.fontBody
                Layout.fillWidth: true
            }

            TextField {
                id: hdrMinBrightnessField
                Layout.preferredWidth: 100
                horizontalAlignment: TextInput.AlignRight
                color: Theme.text
                font.family: Theme.fontMono
                font.pointSize: Theme.fontBody
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                text: hdrBrightnessCard.formatBrightness(StreamingPreferences.hdrMinBrightness, 6)

                background: Rectangle {
                    radius: 0
                    color: Theme.ink
                    border.width: hdrMinBrightnessField.activeFocus ? 2 : 1
                    border.color: hdrMinBrightnessField.activeFocus ? Theme.accent : Theme.lineStrong
                }

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

            MicroLabel {
                text: qsTr("nits")
            }

            Text {
                text: qsTr("Full-frame brightness")
                color: Theme.text
                font.family: Theme.fontSans
                font.pointSize: Theme.fontBody
                Layout.fillWidth: true
            }

            TextField {
                id: hdrMaxAverageBrightnessField
                Layout.preferredWidth: 100
                horizontalAlignment: TextInput.AlignRight
                color: Theme.text
                font.family: Theme.fontMono
                font.pointSize: Theme.fontBody
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                text: hdrBrightnessCard.formatBrightness(StreamingPreferences.hdrMaxAverageBrightness, 3)

                background: Rectangle {
                    radius: 0
                    color: Theme.ink
                    border.width: hdrMaxAverageBrightnessField.activeFocus ? 2 : 1
                    border.color: hdrMaxAverageBrightnessField.activeFocus ? Theme.accent : Theme.lineStrong
                }

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

            MicroLabel {
                text: qsTr("nits")
            }
        }

        Column {
            width: parent.width
            spacing: Theme.spaceSm
            visible: hdrBrightnessCard.manualMode

            MicroLabel {
                text: qsTr("HDR luminance range")
                color: Theme.text
            }

            Text {
                width: parent.width
                text: qsTr("Perceptual PQ scale · drag the markers to adjust. Positions show HDR signal levels, not actual screen brightness.")
                color: Theme.textFaint
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
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
                            color: Theme.lineStrong
                        }

                        Text {
                            id: tickLabel
                            anchors.bottom: parent.bottom
                            text: modelData.label
                            color: Theme.textFaint
                            font.family: Theme.fontMono
                            font.pointSize: 7
                        }
                    }
                }

                // PQ 刻度条。渐变保留 —— 它表达的是亮度斜坡，是信息不是装饰；
                // 但圆角去掉，改成方角 + 1px 描边。
                Rectangle {
                    id: hdrBrightnessScale
                    anchors {
                        left: parent.left
                        right: parent.right
                    }
                    y: 8
                    height: 14
                    radius: 0
                    border.width: 1
                    border.color: Theme.lineStrong
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
                        radius: 0
                        anchors.verticalCenter: parent.verticalCenter
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.accentDim }
                            GradientStop { position: 1.0; color: Theme.accent }
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
                        radius: 0
                        anchors.verticalCenter: parent.verticalCenter
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.accent }
                            GradientStop { position: 1.0; color: Theme.acid }
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

            // 图例。三个色块跟着刻度条上的三段配色：暗青 → 青 → 酸性绿，全部方角。
            RowLayout {
                width: parent.width
                spacing: Theme.spaceSm

                Row {
                    spacing: Theme.spaceXs

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 8
                        height: 8
                        radius: 0
                        color: Theme.accentDim
                    }

                    MicroLabel {
                        text: qsTr("Minimum brightness") + "  " +
                              hdrBrightnessCard.formatBrightness(hdrBrightnessCard.previewMin, 6)
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Row {
                    spacing: Theme.spaceXs

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 3
                        height: 10
                        radius: 0
                        color: Theme.accent
                    }

                    MicroLabel {
                        text: qsTr("Full frame") + "  " +
                              hdrBrightnessCard.formatBrightness(hdrBrightnessCard.previewAverage, 3)
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Row {
                    spacing: Theme.spaceXs

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 8
                        height: 8
                        radius: 0
                        color: Theme.acid
                    }

                    MicroLabel {
                        text: qsTr("Peak") + "  " +
                              hdrBrightnessCard.formatBrightness(hdrBrightnessCard.previewMax, 3)
                        color: Theme.text
                    }
                }
            }
        }

        Text {
            width: parent.width
            visible: hdrBrightnessCard.manualMode && !hdrBrightnessCard.manualValuesValid
            text: qsTr("Enter values in this order: minimum ≤ full-frame ≤ peak brightness.")
            color: Theme.danger
            font.family: Theme.fontMono
            font.pointSize: Theme.fontCaption
            wrapMode: Text.Wrap
        }
    }
}
