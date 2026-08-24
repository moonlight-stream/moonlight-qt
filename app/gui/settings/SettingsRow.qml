import QtQuick 2.9
import QtQuick.Layouts 1.3
import "../theme"

// 一行设置：左边标题 + 说明，右边控件。
// 用 FocusScope 是为了让 activeFocus 在内部控件获得焦点时为真，
// 这样手柄/键盘 Tab 过来时整行会亮起来，而不是只有控件本身有个小框。
FocusScope {
    id: row

    property string title: ""
    property string description: ""
    // 业务上是否该显示这一行（例如某功能在当前平台不可用）
    property bool applicable: true
    readonly property bool stacked: width < Theme.settingsRowStackBreakpoint
    readonly property bool hoverable: controlSlot.children.length > 0

    default property alias controlContent: controlSlot.data

    width: parent ? parent.width : 0
    visible: applicable
    height: visible ? implicitHeight : 0
    implicitHeight: contentLayout.implicitHeight + Theme.spaceMd * 2

    // 行背景：方角，hover 时填 surface2；焦点落进这一行时填 surface2 并在左边
    // 立一条 accent 粗条。
    //
    // 以前焦点是给整行描一圈 1px accent。问题是 Tab 进来时焦点其实落在行里的控件上，
    // 于是控件自己的焦点框和整行的框套在一起，成了两个同色方框嵌套，很脏。
    // 换成左侧粗条之后两者形态完全不同：粗条说「焦点在这一行」，控件的方框说
    // 「具体在这个控件上」，叠在一起也读得清。粗条也是这套设计里 Panel 现成的语汇。
    Rectangle {
        anchors.fill: parent
        radius: 0
        color: ((row.hoverable && hoverArea.containsMouse) || row.activeFocus)
               ? Theme.surface2 : "transparent"
        border.width: 0

        Rectangle {
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            width: row.activeFocus ? Theme.accentBar : 0
            visible: width > 0
            color: Theme.accent
        }

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 1
            color: Theme.line
            // 行与行之间的 1px 分隔（这套设计靠细线分栏，不靠间距）。
            // 同一个 Column 里最后一行不画，避免和卡片下沿贴出双线。
            visible: row.parent && row.parent.children
                     && row.parent.children[row.parent.children.length - 1] !== row
        }

        Behavior on color {
            ColorAnimation { duration: Theme.durFast }
        }
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    GridLayout {
        id: contentLayout

        anchors {
            fill: parent
            leftMargin: Theme.spaceMd
            rightMargin: Theme.spaceMd
            topMargin: Theme.spaceMd
            bottomMargin: Theme.spaceMd
        }
        columns: row.stacked ? 1 : 2
        columnSpacing: Theme.spaceLg
        rowSpacing: row.stacked ? Theme.spaceSm : 0

        Column {
            id: textColumn
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.spaceXs

            Text {
                width: parent.width
                text: row.title
                visible: text !== ""
                color: Theme.text
                font.family: Theme.fontSans
                font.pointSize: Theme.fontRowTitle
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: row.description
                visible: text !== ""
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pointSize: Theme.fontCaption
                wrapMode: Text.Wrap
            }
        }

        Item {
            id: controlSlot
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            Layout.preferredWidth: childrenRect.width
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
