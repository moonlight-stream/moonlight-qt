pragma ComponentBehavior: Bound
import QtQuick 2.9
import QtQuick.Controls
import "."
import "../theme"
import StreamingPreferences 1.0

// “关于”只展示社区入口和法律声明。
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
    // 社区介绍保留四个可见前导空格。
    readonly property string paragraphIndent: "\u00A0\u00A0\u00A0\u00A0"

    readonly property string issueUrl: "https://github.com/qiin2333/moonlight-qt/issues/new/choose"
    readonly property string qqUrl: "https://qm.qq.com/q/3tWBFVNZ"
    readonly property string officialSiteUrl: useMainlandLinks
        ? "https://www.alkaidlab.cn/"
        : "https://www.alkaidlab.com/"
    readonly property string bilibiliUrl: "https://space.bilibili.com/3690974838524514"
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

            TechTag {
                text: qsTr("AlkaidLab Open Source Community")
                prominent: true
            }

            Text {
                width: parent.width
                text: aboutPage.paragraphIndent + qsTr("Founded in 2024, AlkaidLab has brought together tens of thousands of users and maintained in-depth dialogue with thousands of them. Guided by extensive real-world feedback, we have developed a systematic understanding of networking and streaming as a whole and built a complete solution spanning video, audio, touch, controllers, networking, and multi-device experiences, with continuous improvements ahead. The ecosystem covers desktop PCs, Android handhelds, Windows handhelds, TVs and set-top boxes, projectors, VR devices, and more, aiming to deliver a more elegant and uncompromising streaming experience for gaming, media, creation, and remote collaboration.")
                color: Theme.textSettingsSubtitle
                font.family: Theme.fontSans
                font.pointSize: Theme.fontSettingsSubtitle + 1
                font.weight: Font.Medium
                wrapMode: Text.Wrap
            }

        }

        Flow {
            width: parent.width
            spacing: Theme.spaceSm

            HardLink {
                text: qsTr("Website")
                prominent: true
                onClicked: aboutPage.openExternal(aboutPage.officialSiteUrl)
            }

            HardLink {
                text: qsTr("Bilibili")
                prominent: true
                onClicked: aboutPage.openExternal(aboutPage.bilibiliUrl)
            }

            HardLink {
                text: qsTr("QQ Group")
                prominent: true
                onClicked: aboutPage.openExternal(aboutPage.qqUrl)
            }

            HardLink {
                text: qsTr("Submit Issue")
                prominent: true
                onClicked: aboutPage.openExternal(aboutPage.issueUrl)
            }

            HardLink {
                text: qsTr("Legal notice")
                prominent: true
                onClicked: aboutPage.revealLegalNotice()
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
