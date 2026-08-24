pragma ComponentBehavior: Bound
import QtQuick 2.9
import QtQuick.Controls
import "."
import "../theme"
import SystemProperties 1.0
import StreamingPreferences 1.0

// “关于”只展示产品、基地生态和法律入口。
// 页面不保存偏好、不请求网络，也不把网页嵌入客户端。
Column {
    id: aboutPage

    signal scrollToEndRequested()

    width: parent ? parent.width : 0
    spacing: Theme.spaceLg

    function isSimplifiedChineseLocale() {
        var localeName = Qt.locale().name.replace("-", "_").toLowerCase()
        return localeName === "zh_cn" || localeName.indexOf("zh_hans") === 0
    }

    readonly property bool useMainlandLinks:
        StreamingPreferences.language === StreamingPreferences.LANG_ZH_CN ||
        (StreamingPreferences.language === StreamingPreferences.LANG_AUTO &&
         isSimplifiedChineseLocale())
    // 社区介绍与核心服务端说明统一保留四个可见前导空格。
    readonly property string paragraphIndent: "\u00A0\u00A0\u00A0\u00A0"

    readonly property string githubUrl: "https://github.com/qiin2333/moonlight-qt"
    readonly property string issueUrl: "https://github.com/qiin2333/moonlight-qt/issues/new/choose"
    readonly property string androidUrl: "https://github.com/qiin2333/moonlight-vplus"
    readonly property string qqUrl: "https://qm.qq.com/q/3tWBFVNZ"
    readonly property string officialSiteUrl: useMainlandLinks
        ? "https://www.alkaidlab.cn/"
        : "https://www.alkaidlab.com/"
    readonly property string bilibiliUrl: "https://space.bilibili.com/3690974838524514"
    readonly property string sunshineUrl: "https://github.com/AlkaidLab/foundation-sunshine"
    readonly property string sunshineDownloadUrl: sunshineUrl + "/releases/latest"
    readonly property string pcDownloadUrl: githubUrl + "/releases/latest"
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
    readonly property string licenseUrl: "https://github.com/AlkaidLab/foundation-sunshine/blob/master/LICENSE"
    readonly property string noticeUrl: "https://github.com/AlkaidLab/foundation-sunshine/blob/master/NOTICE"

    function openExternal(url) {
        if (!Qt.openUrlExternally(url)) {
            ToolTip.show(qsTr("No external browser is available."), 3500)
        }
    }

    function revealLegalNotice() {
        // 让按钮事件完成后再滚动，避免和当前点击的焦点滚动互相覆盖。
        Qt.callLater(function() { aboutPage.scrollToEndRequested() })
    }

    SettingsCard {
        title: qsTr("About")

        Column {
            width: parent.width
            spacing: Theme.spaceMd

            Row {
                width: parent.width
                spacing: Theme.spaceMd

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    // 窄窗口时标题收缩省略，版本号不被裁掉；极窄时钳到 0，
                    // 避免负宽度把版本行顶到负坐标
                    width: Math.max(0, parent.width - versionLine.implicitWidth - parent.spacing)
                    text: "Moonlight V+ for PC"
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pointSize: Theme.fontHeroTitle
                    font.weight: Font.ExtraBold
                    font.letterSpacing: Theme.trackingTight(Theme.fontHeroTitle)
                    elide: Text.ElideRight
                }

                SpecLine {
                    id: versionLine

                    anchors.verticalCenter: parent.verticalCenter
                    tags: [qsTr("Version %1").arg(SystemProperties.versionString)]
                }
            }

            Text {
                width: parent.width
                text: qsTr("AlkaidLab Open Source Community")
                color: Theme.accentStrong
                font.family: Theme.fontSans
                font.pointSize: Theme.fontCardTitle
                font.weight: Font.ExtraBold
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: aboutPage.paragraphIndent + qsTr("Founded in 2024, AlkaidLab has brought together tens of thousands of users and maintained in-depth dialogue with thousands of them. Guided by extensive real-world feedback, we have developed a systematic understanding of networking and streaming as a whole and built a complete solution spanning video, audio, touch, controllers, networking, and multi-device experiences, with continuous improvements ahead. The ecosystem covers desktop PCs, Android handhelds, Windows handhelds, TVs and set-top boxes, projectors, VR devices, and more, aiming to deliver a more elegant and uncompromising streaming experience for gaming, media, creation, and remote collaboration.")
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
            }

        }

        Flow {
            width: parent.width
            spacing: Theme.spaceSm

            HardLink {
                text: qsTr("Website")
                onClicked: aboutPage.openExternal(aboutPage.officialSiteUrl)
            }

            HardLink {
                text: qsTr("Bilibili")
                onClicked: aboutPage.openExternal(aboutPage.bilibiliUrl)
            }

            HardLink {
                text: qsTr("QQ Group")
                onClicked: aboutPage.openExternal(aboutPage.qqUrl)
            }

            HardLink {
                text: qsTr("Submit Issue")
                onClicked: aboutPage.openExternal(aboutPage.issueUrl)
            }

            HardLink {
                text: qsTr("Legal notice")
                onClicked: aboutPage.revealLegalNotice()
            }
        }
    }

    SettingsCard {
        title: qsTr("Server")

        SettingsRow {
            id: serverInfoRow

            title: "Foundation Sunshine"
            description: aboutPage.paragraphIndent + qsTr("Foundation Sunshine is the core streaming server of the AlkaidLab ecosystem and is installed on the controlled device. It connects AlkaidLab clients with a wide range of devices to deliver stronger streaming capabilities for gaming, creation, media, remote control, and more.")

            Flow {
                width: Math.min(260, Math.max(0, serverInfoRow.width - Theme.spaceMd * 2))
                spacing: Theme.spaceSm

                HardLink {
                    text: qsTr("Visit Now")
                    onClicked: aboutPage.openExternal(aboutPage.sunshineUrl)
                }

                HardButton {
                    text: qsTr("Download")
                    primary: true
                    icon.source: "qrc:/res/fluent/download.svg"
                    icon.color: Theme.ink
                    icon.width: 16
                    icon.height: 16
                    onClicked: aboutPage.openExternal(aboutPage.sunshineDownloadUrl)
                }
            }
        }

        EcosystemCapabilityGroup {
            width: parent.width
            title: qsTr("Picture and Streaming")
            description: qsTr("Our exclusive end-to-end HDR technology brings together HDR10, HDR10+, HLG, HDR Vivid, and SDR-to-HDR, alongside in-house capture technology that delivers a smoother experience than WGC, enhanced hardware encoding, and precise frame synchronization.")
            tags: [
                qsTr("In-house Capture Technology"),
                qsTr("Enhanced WGC"),
                qsTr("Enhanced AMD Encoding"),
                qsTr("Sliced Encoding"),
                qsTr("Low-latency Mode"),
                qsTr("HDR10"),
                qsTr("HDR10+"),
                qsTr("HLG"),
                qsTr("HDR Vivid")
            ]
        }

        EcosystemCapabilityGroup {
            width: parent.width
            title: qsTr("Input and Interaction")
            description: qsTr("A stronger input system covers touch, pen, touchpad, mouse, controllers, and stream-side assistance for both gaming and productivity.")
            tags: [
                qsTr("Native Touch"),
                qsTr("Native Pen"),
                qsTr("Precision Touchpad"),
                qsTr("Virtual Mouse"),
                qsTr("DualSense Controller"),
                qsTr("HD Haptics"),
                qsTr("Super Menu")
            ]
        }

        EcosystemCapabilityGroup {
            width: parent.width
            title: qsTr("Audio and Collaboration")
            showDivider: false
            description: qsTr("Immersive audio, audio haptics, remote microphone, clipboard, and folder sharing extend streaming into entertainment, creation, and collaboration.")
            tags: [
                qsTr("Immersive Audio"),
                qsTr("Audio Haptics"),
                qsTr("Remote Microphone"),
                qsTr("Clipboard Sync"),
                qsTr("Folder Sharing")
            ]
        }
    }

    SettingsCard {
        title: qsTr("Clients")
        subtitle: qsTr("Client applications act as the controller for the streaming server.")

        EcosystemPlatformCatalog {
            width: parent.width
            pcUrl: aboutPage.githubUrl
            pcDownloadUrl: aboutPage.pcDownloadUrl
            androidUrl: aboutPage.androidUrl
            androidDownloadUrl: aboutPage.androidDownloadUrl
            harmonyUrl: aboutPage.harmonyUrl
            harmonyStoreUrl: aboutPage.harmonyStoreUrl
            iosUrl: aboutPage.voidLinkUrl
            macosUrl: aboutPage.macosUrl
            macosDownloadUrl: aboutPage.macosDownloadUrl
            onOpenRequested: function(url) { aboutPage.openExternal(url) }
        }
    }

    SettingsCard {
        title: qsTr("Friendly links")

        SettingsRow {
            title: qsTr("Moonlight Android Crown")
            description: qsTr("A community-enhanced Android client with an alternative take on mobile input, controller support, and interface features.")

            HardLink {
                text: qsTr("Visit Now")
                onClicked: aboutPage.openExternal(aboutPage.crownUrl)
            }
        }

        SettingsRow {
            title: "moonlight-ohos"
            description: qsTr("The first Moonlight client built for HarmonyOS NEXT, pioneering Moonlight on HarmonyOS and compatible with the AlkaidLab ecosystem.")

            HardLink {
                text: qsTr("Visit Now")
                onClicked: aboutPage.openExternal(aboutPage.ohosUrl)
            }
        }

        SettingsRow {
            title: qsTr("Natpierce")
            description: qsTr("A virtual networking tool that connects Moonlight clients to Sunshine servers over the public Internet when they are not on the same LAN.")

            HardLink {
                text: qsTr("Visit Now")
                onClicked: aboutPage.openExternal(aboutPage.natpierceUrl)
            }
        }
    }

    SettingsCard {
        title: qsTr("Legal notice")
        subtitle: qsTr("This software is released under GNU GPLv3. The full license and third-party component information are provided below.")

        SettingsRow {
            id: legalInfoRow

            title: "GNU GPL v3.0"

            Flow {
                width: Math.min(300, Math.max(0, legalInfoRow.width - Theme.spaceMd * 2))
                spacing: Theme.spaceSm

                HardLink {
                    text: qsTr("License agreement")
                    onClicked: aboutPage.openExternal(aboutPage.licenseUrl)
                }

                HardLink {
                    text: qsTr("Third-party notice")
                    onClicked: aboutPage.openExternal(aboutPage.noticeUrl)
                }
            }
        }

        CommunityLicenseNotice {
            width: parent.width
        }
    }

}
