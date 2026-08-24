pragma ComponentBehavior: Bound
import QtQuick 2.9
import QtQuick.Layouts 1.3
import "."
import "../theme"

// 基地客户端生态：左侧选择平台，右侧展示对应客户端和官方入口。
// 数据只包含固定项目与固定 URL，不保存选择，也不参与任何串流状态。
Item {
    id: catalog

    property string pcUrl: ""
    property string pcDownloadUrl: ""
    property string androidUrl: ""
    property string androidDownloadUrl: ""
    property string harmonyUrl: ""
    property string harmonyStoreUrl: ""
    property string iosUrl: ""
    property string macosUrl: ""
    property string macosDownloadUrl: ""

    signal openRequested(string url)

    property int selectedPlatformIndex: 0
    readonly property var platformKeys: ["pc", "android", "harmony", "ios", "macosEnhanced"]
    readonly property string selectedPlatformKey: platformKeys[selectedPlatformIndex]
    readonly property bool splitLayout: width >= Theme.compactBreakpoint

    width: parent ? parent.width : 0
    implicitHeight: layout.implicitHeight

    onSplitLayoutChanged: {
        if (platformViewport) {
            platformViewport.contentX = 0
            platformViewport.contentY = 0
        }
    }

    function platformLabel(key) {
        switch (key) {
        case "pc": return qsTr("PC")
        case "android": return qsTr("Android")
        case "harmony": return qsTr("HarmonyOS")
        case "ios": return qsTr("iOS")
        case "macosEnhanced": return qsTr("macOS Enhanced")
        default: return ""
        }
    }

    function clientName(key) {
        switch (key) {
        case "pc": return "Moonlight V+ for PC"
        case "android": return "Moonlight V+"
        case "harmony": return qsTr("Moonlight V+ for HarmonyOS")
        case "ios": return "VoidLink"
        case "macosEnhanced": return "Moonlight macOS Enhanced"
        default: return ""
        }
    }

    function clientDescription(key) {
        switch (key) {
        case "pc": return qsTr("AlkaidLab's cross-platform desktop client for Windows, Linux, macOS, and Steam Link")
        case "android": return qsTr("AlkaidLab's Android client for phones, tablets, TVs, head units, and Meta Quest devices")
        case "harmony": return qsTr("AlkaidLab's native HarmonyOS NEXT client for phones, tablets, and 2-in-1 devices")
        case "ios": return qsTr("Touch-oriented and controller-friendly VoidLink client for iPhone and iPad")
        case "macosEnhanced": return qsTr("AlkaidLab's native macOS client with enhanced HDR, audio, clipboard, and input paths")
        default: return ""
        }
    }

    function supportTags(key) {
        switch (key) {
        case "pc": return [qsTr("Windows x64 / ARM64"), "Linux", "macOS", "Steam Link"]
        case "android": return [
            qsTr("Android Phone"),
            qsTr("Android Tablet"),
            qsTr("Android TV / TV Box"),
            qsTr("Android Head Unit"),
            "Meta Quest 2 / 3"
        ]
        case "harmony": return [
            "HarmonyOS NEXT",
            qsTr("HarmonyOS Phone"),
            qsTr("HarmonyOS Tablet"),
            qsTr("HarmonyOS 2-in-1")
        ]
        case "ios": return ["iPhone", "iPad"]
        case "macosEnhanced": return ["Apple Silicon", "Intel"]
        default: return []
        }
    }

    function capabilityTags(key) {
        switch (key) {
        case "pc": return [
            qsTr("Advanced HDR"),
            qsTr("Smart Bitrate"),
            qsTr("Desktop Collaboration"),
            qsTr("DualSense Haptics")
        ]
        case "android": return [
            qsTr("Precise Frame Sync"),
            qsTr("Audio Haptics"),
            qsTr("Super Menu"),
            qsTr("Native Touch"),
            qsTr("Native Pen")
        ]
        case "harmony": return [
            qsTr("HDR Vivid"),
            qsTr("Smart Bitrate"),
            qsTr("Audio Haptics"),
            qsTr("Remote Microphone"),
            qsTr("Gyro Aim")
        ]
        case "ios": return [
            qsTr("Touch-oriented Interface"),
            qsTr("Controller Friendly"),
            qsTr("In-stream Widgets")
        ]
        case "macosEnhanced": return [
            qsTr("Advanced HDR"),
            qsTr("Image Enhancement"),
            qsTr("Immersive Audio"),
            qsTr("Clipboard Sync"),
            qsTr("Low-latency Input")
        ]
        default: return []
        }
    }

    function officialLinks(key) {
        switch (key) {
        case "pc": return [
            { label: qsTr("Visit Now"), url: pcUrl, primary: false },
            { label: qsTr("Download"), url: pcDownloadUrl, primary: true }
        ]
        case "android": return [
            { label: qsTr("Visit Now"), url: androidUrl, primary: false },
            { label: qsTr("Download"), url: androidDownloadUrl, primary: true }
        ]
        case "harmony": return [
            { label: qsTr("Visit Now"), url: harmonyUrl, primary: false },
            { label: qsTr("Download"), url: harmonyStoreUrl, primary: true }
        ]
        case "ios": return [{ label: qsTr("Download"), url: iosUrl, primary: true }]
        case "macosEnhanced": return [
            { label: qsTr("Visit Now"), url: macosUrl, primary: false },
            { label: qsTr("Download"), url: macosDownloadUrl, primary: true }
        ]
        default: return []
        }
    }

    function ensurePlatformVisible(item) {
        if (splitLayout) {
            if (item.y < platformViewport.contentY) {
                platformViewport.contentY = item.y
            }
            else if (item.y + item.height > platformViewport.contentY + platformViewport.height) {
                platformViewport.contentY = item.y + item.height - platformViewport.height
            }
        }
        else if (item.x < platformViewport.contentX) {
            platformViewport.contentX = item.x
        }
        else if (item.x + item.width > platformViewport.contentX + platformViewport.width) {
            platformViewport.contentX = item.x + item.width - platformViewport.width
        }
    }

    GridLayout {
        id: layout

        width: parent.width
        columns: catalog.splitLayout ? 2 : 1
        columnSpacing: Theme.spaceLg
        rowSpacing: Theme.spaceMd

        Flickable {
            id: platformViewport

            Layout.fillWidth: !catalog.splitLayout
            Layout.preferredWidth: catalog.splitLayout ? 208 : layout.width
            Layout.preferredHeight: catalog.splitLayout ? 236 : 44
            contentWidth: platformGrid.width
            contentHeight: platformGrid.height
            flickableDirection: catalog.splitLayout
                                ? Flickable.VerticalFlick
                                : Flickable.HorizontalFlick
            interactive: contentWidth > width || contentHeight > height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Grid {
                id: platformGrid

                columns: catalog.splitLayout ? 1 : catalog.platformKeys.length
                rows: catalog.splitLayout ? catalog.platformKeys.length : 1
                rowSpacing: Theme.spaceXs
                columnSpacing: Theme.spaceXs

                Repeater {
                    model: catalog.platformKeys

                    PlatformNavButton {
                        required property int index
                        required property string modelData

                        width: catalog.splitLayout ? platformViewport.width : implicitWidth
                        height: 44
                        text: catalog.platformLabel(modelData)
                        selected: index === catalog.selectedPlatformIndex
                        onClicked: catalog.selectedPlatformIndex = index
                        onActiveFocusChanged: {
                            if (activeFocus) {
                                catalog.ensurePlatformVisible(this)
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: detailPanel

            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(236, detailColumn.implicitHeight + Theme.spaceLg * 2)
            radius: 0
            color: Theme.surface2Layer
            border.width: 1
            border.color: Theme.line

            Rectangle {
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                }
                width: Theme.accentBar
                color: Theme.accent
            }

            Column {
                id: detailColumn

                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: Theme.spaceLg
                }
                spacing: Theme.spaceMd

                Column {
                    width: parent.width
                    spacing: Theme.spaceXs

                    Text {
                        width: parent.width
                        text: catalog.clientName(catalog.selectedPlatformKey)
                        color: Theme.text
                        font.family: Theme.fontSans
                        font.pointSize: Theme.fontCardTitle
                        font.weight: Font.Medium
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        text: catalog.clientDescription(catalog.selectedPlatformKey)
                        visible: text !== ""
                        color: Theme.textDim
                        font.family: Theme.fontMono
                        font.pointSize: Theme.fontCaption
                        wrapMode: Text.Wrap
                    }
                }

                Text {
                    text: qsTr("Supported platforms")
                    color: Theme.textDim
                    font.family: Theme.fontSans
                    font.pointSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }

                SpecLine {
                    width: parent.width
                    tags: catalog.supportTags(catalog.selectedPlatformKey)
                }

                Text {
                    text: qsTr("Featured capabilities")
                    color: Theme.textDim
                    font.family: Theme.fontSans
                    font.pointSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }

                SpecLine {
                    width: parent.width
                    tags: catalog.capabilityTags(catalog.selectedPlatformKey)
                }

                Text {
                    text: qsTr("Official links")
                    color: Theme.textDim
                    font.family: Theme.fontSans
                    font.pointSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }

                Flow {
                    width: parent.width
                    spacing: Theme.spaceSm

                    Repeater {
                        model: catalog.officialLinks(catalog.selectedPlatformKey)
                               .filter(function(link) { return !link.primary })

                        HardLink {
                            required property var modelData

                            text: modelData.label
                            enabled: modelData.url !== ""
                            onClicked: catalog.openRequested(modelData.url)
                        }
                    }

                    Repeater {
                        model: catalog.officialLinks(catalog.selectedPlatformKey)
                               .filter(function(link) { return link.primary })

                        HardButton {
                            required property var modelData

                            text: modelData.label
                            primary: modelData.primary
                            icon.source: "qrc:/res/fluent/download.svg"
                            icon.color: Theme.ink
                            icon.width: 16
                            icon.height: 16
                            enabled: modelData.url !== ""
                            onClicked: catalog.openRequested(modelData.url)
                        }
                    }
                }
            }
        }
    }
}
