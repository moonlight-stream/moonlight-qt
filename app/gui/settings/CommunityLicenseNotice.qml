pragma ComponentBehavior: Bound
import QtQuick 2.9
import "../theme"

// 社区许可立场公示。正文始终完整显示，用分节、列表和警示块建立清晰层级。
Column {
    id: notice

    width: parent ? parent.width : 0
    spacing: 0

    component NoticeList: Column {
        id: noticeList

        required property var items
        property color bulletColor: Theme.accent
        property color textColor: Theme.textDim

        width: parent ? parent.width : 0
        spacing: Theme.spaceSm

        Repeater {
            model: noticeList.items

            Row {
                required property string modelData

                width: noticeList.width
                height: Math.max(bullet.height, itemText.implicitHeight)
                spacing: Theme.spaceSm

                Rectangle {
                    id: bullet

                    anchors.verticalCenter: parent.verticalCenter
                    width: 5
                    height: 5
                    color: noticeList.bulletColor
                }

                Text {
                    id: itemText

                    width: parent.width - bullet.width - parent.spacing
                    text: parent.modelData
                    color: noticeList.textColor
                    font.family: Theme.fontMono
                    font.pointSize: Theme.fontCaption
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    Rectangle {
        width: parent.width
        height: contentColumn.implicitHeight + Theme.spaceLg * 2
        radius: 0
        color: Theme.surface2Layer
        border.width: 1
        border.color: Theme.line

        Column {
            id: contentColumn

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: Theme.spaceLg
            }
            spacing: Theme.spaceMd

            Text {
                width: parent.width
                text: qsTr("Open Source and Compatibility Notice")
                color: Theme.accentStrong
                font.family: Theme.fontSans
                font.pointSize: Theme.fontCardTitle
                font.weight: Font.ExtraBold
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: qsTr("We know open source is not easy. Every feature, test, answer, and maintenance update represents real time and effort from real people. AlkaidLab follows GPLv3 so that every user can access the source, understand the software, modify it freely, and pass those freedoms on.")
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: qsTr("Freedom and Responsibility")
                color: Theme.accentStrong
                font.family: Theme.fontSans
                font.pointSize: Theme.fontRowTitle
                font.weight: Font.ExtraBold
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: qsTr("GPLv3 gives everyone the freedom to run this software. When you copy, modify, or distribute it, please help preserve that freedom:")
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
            }

            NoticeList {
                items: [
                    qsTr("Keep the original copyright and license notices."),
                    qsTr("Clearly state what you changed."),
                    qsTr("When distributing modified or binary versions, provide the corresponding source as required by GPLv3."),
                    qsTr("Do not impose additional terms that restrict downstream use, study, modification, or redistribution.")
                ]
            }

            Text {
                width: parent.width
                text: qsTr("We Will Not Endorse Violations")
                color: Theme.danger
                font.family: Theme.fontSans
                font.pointSize: Theme.fontRowTitle
                font.weight: Font.ExtraBold
                wrapMode: Text.Wrap
            }

            Rectangle {
                width: parent.width
                height: warningText.implicitHeight + Theme.spaceMd * 2
                radius: 0
                color: Theme.surface2Layer
                border.width: 1
                border.color: Theme.danger

                Rectangle {
                    anchors {
                        left: parent.left
                        top: parent.top
                        bottom: parent.bottom
                    }
                    width: Theme.accentBar
                    color: Theme.danger
                }

                Text {
                    id: warningText

                    anchors {
                        fill: parent
                        topMargin: Theme.spaceMd
                        rightMargin: Theme.spaceMd
                        bottomMargin: Theme.spaceMd
                        leftMargin: Theme.spaceMd + Theme.accentBar
                    }
                    text: qsTr("For software confirmed to violate the GPL and whose distributors refuse to correct the violation, AlkaidLab will not provide dedicated compatibility work, recommendations, promotion, or bundling.")
                    color: Theme.danger
                    font.family: Theme.fontSans
                    font.pointSize: Theme.fontCaption
                    font.weight: Font.ExtraBold
                    wrapMode: Text.Wrap
                }
            }

            Text {
                width: parent.width
                text: qsTr("This is not about creating conflict. It protects the work of contributors and each user's right to obtain, inspect, and modify the source.")
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: qsTr("If a project's authorization, source availability, or license status cannot yet be verified, the community will remain cautious and welcomes clarification from its developers. We will not label a project as infringing without evidence.")
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: qsTr("Community Agreement")
                color: Theme.accentStrong
                font.family: Theme.fontSans
                font.pointSize: Theme.fontRowTitle
                font.weight: Font.ExtraBold
                wrapMode: Text.Wrap
            }

            NoticeList {
                bulletColor: Theme.danger
                textColor: Theme.danger
                items: [
                    qsTr("Within AlkaidLab community spaces, using this software for illegal activities, game cheating, the development or distribution of cheats, or conduct that infringes the rights of others is prohibited and will not receive community support.")
                ]
            }

            NoticeList {
                items: [
                    qsTr("Respect open-source licenses and the copyrights of original authors."),
                    qsTr("Do not promote software confirmed to have unresolved license violations in the community."),
                    qsTr("When sharing software, include its project source and license information whenever possible."),
                    qsTr("Use, improve, and contribute to genuinely free and open-source alternatives."),
                    qsTr("If a program's license status is unclear, ask the community before promoting it.")
                ]
            }

            Text {
                width: parent.width
                text: qsTr("A Final Note")
                color: Theme.text
                font.family: Theme.fontSans
                font.pointSize: Theme.fontRowTitle
                font.weight: Font.ExtraBold
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: qsTr("Software freedom does not sustain itself. It depends on every developer's persistence and every user's choice to respect and protect it. Thank you for helping us keep the open-source community free, transparent, and welcoming, and for giving the people who build it the respect they deserve.")
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: qsTr("— AlkaidLab Open Source Community Maintainers")
                color: Theme.accentStrong
                font.family: Theme.fontSans
                font.pointSize: Theme.fontCaption
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: qsTr("This notice describes AlkaidLab's maintenance and compatibility policy and is not legal advice. Specific rights and obligations are governed by the GNU GPLv3 license text.")
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
            }
        }
    }
}
