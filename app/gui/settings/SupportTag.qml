import QtQuick 2.9
import "../theme"

// 只读的平台标签，不进入焦点链，也不承担跳转。
Rectangle {
    id: tag

    property alias text: label.text
    // neutral / accent / input / audio / platform
    property string tone: "neutral"

    readonly property color resolvedSurface: tone === "accent" ? Theme.tagAccentSurface :
                                                      tone === "input" ? Theme.tagInputSurface :
                                                      tone === "audio" ? Theme.tagAudioSurface :
                                                      tone === "platform" ? Theme.tagPlatformSurface :
                                                      Theme.surface2
    readonly property color resolvedBorder: tone === "accent" ? Theme.tagAccentBorder :
                                                     tone === "input" ? Theme.tagInputBorder :
                                                     tone === "audio" ? Theme.tagAudioBorder :
                                                     tone === "platform" ? Theme.tagPlatformBorder :
                                                     Theme.lineStrong
    readonly property color resolvedText: tone === "accent" ? Theme.tagAccentText :
                                                   tone === "input" ? Theme.tagInputText :
                                                   tone === "audio" ? Theme.tagAudioText :
                                                   tone === "platform" ? Theme.tagPlatformText :
                                                   Theme.textSettingsSubtitle

    implicitWidth: label.implicitWidth + Theme.spaceMd * 2
    implicitHeight: 30
    radius: 0
    color: resolvedSurface
    border.width: 1
    border.color: resolvedBorder

    Text {
        id: label
        anchors.centerIn: parent
        color: tag.resolvedText
        font.family: Theme.fontSans
        font.pointSize: Theme.fontBody
        font.weight: Font.Medium
    }
}
