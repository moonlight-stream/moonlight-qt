pragma ComponentBehavior: Bound
import QtQuick 2.9
import QtQuick.Controls
import QtQuick.Layouts 1.3

import StreamingPreferences 1.0

import "theme"

// 全新安装只显示一次的背景风格选择。这里只收集选择，偏好保存和后续
// 配置检查仍由 ApplicationWindow 统一安排，避免多个启动弹窗重叠。
NavigableDialog {
    id: dialog

    signal sourceChosen(int source)

    title: qsTr("Choose a background style")
    closePolicy: Popup.CloseOnEscape
    width: Math.max(280, Math.min(480, Overlay.overlay.width - Theme.spaceLg * 2))

    readonly property var choices: [
        { label: qsTr("Photography"), source: StreamingPreferences.BGS_PHOTOGRAPHY },
        { label: qsTr("Anime"), source: StreamingPreferences.BGS_ANIME },
        { label: qsTr("No background"), source: StreamingPreferences.BGS_NONE }
    ]

    function chooseSource(source) {
        sourceChosen(source)
        close()
    }

    onOpened: {
        var firstButton = choiceRepeater.itemAt(0)
        if (firstButton) {
            firstButton.forceActiveFocus(Qt.TabFocusReason)
        }
    }

    ColumnLayout {
        width: parent ? parent.width : 0
        spacing: Theme.spaceMd

        Text {
            Layout.fillWidth: true
            text: qsTr("Choose the style of random backgrounds shown on the computer list. You can change this at any time in Software Settings.")
            color: Theme.textSettingsSubtitle
            font.family: Theme.fontSans
            font.pointSize: Theme.fontSettingsSubtitle
            font.weight: Font.Medium
            wrapMode: Text.Wrap
        }

        Repeater {
            id: choiceRepeater
            model: dialog.choices

            HardButton {
                required property var modelData

                Layout.fillWidth: true
                Layout.preferredHeight: 42
                text: modelData.label
                onClicked: dialog.chooseSource(modelData.source)
            }
        }
    }

    footer: DialogButtonBox {
        padding: Theme.spaceXl
        topPadding: Theme.spaceMd
        alignment: Qt.AlignRight
        background: Item {}

        HardButton {
            text: qsTr("Decide later")
            onClicked: dialog.chooseSource(StreamingPreferences.BGS_PHOTOGRAPHY)
        }
    }
}
