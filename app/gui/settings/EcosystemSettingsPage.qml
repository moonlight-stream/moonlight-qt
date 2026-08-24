pragma ComponentBehavior: Bound
import QtQuick 2.9
import QtQuick.Controls
import "."
import "../theme"
import StreamingPreferences 1.0

// 基地生态独立承载串流主机、核心能力和各平台客户端。“关于”页只保留
// 社区与法律信息，避免两类内容继续堆在同一条长页面里。
Column {
    id: ecosystemPage

    signal aboutRequested()

    width: parent ? parent.width : 0
    spacing: Theme.spaceLg

    readonly property string paragraphIndent: "\u00A0\u00A0\u00A0\u00A0"
    readonly property string sunshineUrl: "https://github.com/AlkaidLab/foundation-sunshine"
    readonly property string sunshineDownloadUrl: sunshineUrl + "/releases/latest"
    readonly property string pcUrl: "https://github.com/qiin2333/moonlight-qt"
    readonly property string pcDownloadUrl: pcUrl + "/releases/latest"
    readonly property string androidUrl: "https://github.com/qiin2333/moonlight-vplus"
    readonly property string androidDownloadUrl: androidUrl + "/releases/latest"
    readonly property string harmonyUrl: "https://github.com/AlkaidLab/moonlight-harmony"
    readonly property string harmonyStoreUrl: "https://appgallery.huawei.com/app/detail?id=com.alkaidlab.sdream"
    readonly property string voidLinkUrl: useMainlandLinks
        ? "https://apps.apple.com/cn/app/voidlink/id6747717070"
        : "https://apps.apple.com/us/app/voidlink-extreme/id6755103808"
    readonly property string macosUrl: "https://github.com/skyhua0224/moonlight-macos-enhanced"
    readonly property string macosDownloadUrl: macosUrl + "/releases/latest"
    readonly property string crownUrl: "https://github.com/WACrown/moonlight-android"
    readonly property string ohosUrl: "https://gitee.com/smdsbz/moonlight-ohos"
    readonly property string natpierceUrl: "https://docs.qq.com/aio/DRFVhWERDaFhKd1ZE"

    readonly property bool useMainlandLinks:
        StreamingPreferences.language === StreamingPreferences.LANG_ZH_CN ||
        (StreamingPreferences.language === StreamingPreferences.LANG_AUTO &&
         isSimplifiedChineseLocale())

    function isSimplifiedChineseLocale() {
        var localeName = Qt.locale().name.replace("-", "_").toLowerCase()
        return localeName === "zh_cn" || localeName.indexOf("zh_hans") === 0
    }

    function openExternal(url) {
        if (!Qt.openUrlExternally(url)) {
            ToolTip.show(qsTranslate("AboutSettingsPage", "No external browser is available."), 3500)
        }
    }

    PlatformNavButton {
        width: Math.min(parent.width, 320)
        height: 44
        text: qsTr("About AlkaidLab Open Source Community")
        onClicked: ecosystemPage.aboutRequested()
    }

    SettingsCard {
        title: qsTranslate("AboutSettingsPage", "Server")

        SettingsRow {
            id: serverInfoRow

            title: "Foundation Sunshine"
            description: ecosystemPage.paragraphIndent + qsTranslate("AboutSettingsPage", "Foundation Sunshine is the core streaming server of the AlkaidLab ecosystem and is installed on the controlled device. It connects AlkaidLab clients with a wide range of devices to deliver stronger streaming capabilities for gaming, creation, media, remote control, and more.")
            descriptionFontPointSize: Theme.fontSettingsSubtitle + 1

            Flow {
                width: Math.min(260, Math.max(0, serverInfoRow.width - Theme.spaceMd * 2))
                spacing: Theme.spaceSm

                HardLink {
                    text: qsTranslate("AboutSettingsPage", "Visit Now")
                    onClicked: ecosystemPage.openExternal(ecosystemPage.sunshineUrl)
                }

                HardButton {
                    text: qsTranslate("AboutSettingsPage", "Download")
                    primary: true
                    icon.source: "qrc:/res/fluent/download.svg"
                    icon.color: Theme.ink
                    icon.width: 16
                    icon.height: 16
                    onClicked: ecosystemPage.openExternal(ecosystemPage.sunshineDownloadUrl)
                }
            }
        }

        EcosystemCapabilityGroup {
            width: parent.width
            title: qsTranslate("AboutSettingsPage", "Picture and Streaming")
            segments: [
                { text: qsTr("Built around"), highlight: false },
                { text: qsTranslate("AboutSettingsPage", "In-house Capture Technology"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Enhanced WGC"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Enhanced AMD Encoding"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Sliced Encoding"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Low-latency Mode"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "HDR10"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "HDR10+"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "HLG"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "HDR Vivid"), highlight: true },
                { text: qsTr("to combine low-latency streaming, precise synchronization, and comprehensive HDR presentation."), highlight: false }
            ]
        }

        EcosystemCapabilityGroup {
            width: parent.width
            title: qsTranslate("AboutSettingsPage", "Input and Interaction")
            segments: [
                { text: qsTr("Connect"), highlight: false },
                { text: qsTranslate("AboutSettingsPage", "Native Touch"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Native Pen"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Precision Touchpad"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Virtual Mouse"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "DualSense Controller"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "HD Haptics"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Super Menu"), highlight: true },
                { text: qsTr("into one input path for gaming, creation, and remote control."), highlight: false }
            ]
        }

        EcosystemCapabilityGroup {
            width: parent.width
            title: qsTranslate("AboutSettingsPage", "Audio and Collaboration")
            showDivider: false
            segments: [
                { text: qsTr("Bring together"), highlight: false },
                { text: qsTranslate("AboutSettingsPage", "Immersive Audio"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Audio Haptics"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Remote Microphone"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Clipboard Sync"), highlight: true },
                { text: qsTranslate("AboutSettingsPage", "Folder Sharing"), highlight: true },
                { text: qsTr("to extend streaming from entertainment to creation and collaboration."), highlight: false }
            ]
        }
    }

    SettingsCard {
        title: qsTranslate("AboutSettingsPage", "Clients")
        subtitle: qsTranslate("AboutSettingsPage", "Client applications act as the controller for the streaming server.")

        EcosystemPlatformCatalog {
            width: parent.width
            pcUrl: ecosystemPage.pcUrl
            pcDownloadUrl: ecosystemPage.pcDownloadUrl
            androidUrl: ecosystemPage.androidUrl
            androidDownloadUrl: ecosystemPage.androidDownloadUrl
            harmonyUrl: ecosystemPage.harmonyUrl
            harmonyStoreUrl: ecosystemPage.harmonyStoreUrl
            iosUrl: ecosystemPage.voidLinkUrl
            macosUrl: ecosystemPage.macosUrl
            macosDownloadUrl: ecosystemPage.macosDownloadUrl
            onOpenRequested: function(url) { ecosystemPage.openExternal(url) }
        }
    }

    SettingsCard {
        title: qsTranslate("AboutSettingsPage", "Friendly links")

        SettingsRow {
            title: qsTranslate("AboutSettingsPage", "Moonlight Android Crown")
            description: qsTranslate("AboutSettingsPage", "A community-enhanced Android client with an alternative take on mobile input, controller support, and interface features.")

            HardLink {
                text: qsTranslate("AboutSettingsPage", "Visit Now")
                onClicked: ecosystemPage.openExternal(ecosystemPage.crownUrl)
            }
        }

        SettingsRow {
            title: "moonlight-ohos"
            description: qsTranslate("AboutSettingsPage", "The first Moonlight client built for HarmonyOS NEXT, pioneering Moonlight on HarmonyOS and compatible with the AlkaidLab ecosystem.")

            HardLink {
                text: qsTranslate("AboutSettingsPage", "Visit Now")
                onClicked: ecosystemPage.openExternal(ecosystemPage.ohosUrl)
            }
        }

        SettingsRow {
            title: qsTranslate("AboutSettingsPage", "Natpierce")
            description: qsTranslate("AboutSettingsPage", "A virtual networking tool that connects Moonlight clients to Sunshine servers over the public Internet when they are not on the same LAN.")

            HardLink {
                text: qsTranslate("AboutSettingsPage", "Visit Now")
                onClicked: ecosystemPage.openExternal(ecosystemPage.natpierceUrl)
            }
        }
    }
}
