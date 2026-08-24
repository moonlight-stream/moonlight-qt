import QtQuick 2.9
import QtQuick.Controls
import "../theme"

// 客户端生态中的平台选择项。选中态表示当前详情，焦点态只表示键盘/手柄位置。
Button {
    id: control

    property bool selected: false
    property string markerText: ">"

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    leftPadding: Theme.spaceMd
    rightPadding: Theme.spaceMd

    readonly property Item __focusFrameTarget: null

    onSelectedChanged: plate.requestPaint()
    onHoveredChanged: plate.requestPaint()
    onDownChanged: plate.requestPaint()
    onVisualFocusChanged: plate.requestPaint()

    contentItem: Item {
        implicitWidth: 140
        implicitHeight: 22

        Rectangle {
            id: node
            anchors {
                left: parent.left
                verticalCenter: parent.verticalCenter
            }
            width: 5
            height: 5
            color: control.selected ? Theme.accentStrong : Theme.textFaint
        }

        Text {
            anchors {
                left: node.right
                right: marker.left
                leftMargin: Theme.spaceSm
                rightMargin: Theme.spaceSm
                verticalCenter: parent.verticalCenter
            }
            text: control.text
            color: control.selected ? Theme.accentStrong : Theme.text
            font.family: Theme.fontMono
            font.pointSize: Theme.fontBody
            font.weight: Font.Medium
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Text {
            id: marker
            anchors {
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            text: control.markerText
            color: control.selected || control.hovered || control.visualFocus
                   ? Theme.accentStrong : Theme.textFaint
            font.family: Theme.fontMono
            font.pointSize: Theme.fontBody
            font.weight: Font.DemiBold
        }
    }

    background: Canvas {
        id: plate

        implicitWidth: 164
        implicitHeight: 44

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            var cut = 6
            var active = control.selected || control.down
            var highlighted = active || control.hovered || control.visualFocus

            ctx.clearRect(0, 0, w, h)
            ctx.beginPath()
            ctx.moveTo(cut, 0.75)
            ctx.lineTo(w - cut, 0.75)
            ctx.lineTo(w - 0.75, cut)
            ctx.lineTo(w - 0.75, h - cut)
            ctx.lineTo(w - cut, h - 0.75)
            ctx.lineTo(cut, h - 0.75)
            ctx.lineTo(0.75, h - cut)
            ctx.lineTo(0.75, cut)
            ctx.closePath()
            ctx.fillStyle = active ? Theme.accentSoft
                                   : (highlighted ? Theme.surface2 : "transparent")
            ctx.fill()
            ctx.strokeStyle = highlighted ? Theme.accent : Theme.line
            ctx.lineWidth = control.visualFocus ? 2 : 1
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(cut + 3, 1.25)
            ctx.lineTo(Math.min(w - cut - 3, 42), 1.25)
            ctx.strokeStyle = highlighted ? Theme.accentStrong : Theme.accentDim
            ctx.lineWidth = 2
            ctx.stroke()

            if (control.selected) {
                ctx.fillStyle = Theme.accent
                ctx.fillRect(0, cut, Theme.accentBar, h - cut * 2)
            }
        }
    }

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
}
