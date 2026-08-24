import QtQuick 2.9
import "../theme"

// 只读 HUD 芯片标签：深色切角底板、青色细边和状态点。
// 始终水平显示，不承担点击或状态交互。
Item {
    id: tag

    property alias text: label.text
    property bool prominent: false
    property bool showIndicator: true

    implicitWidth: content.implicitWidth + Theme.spaceLg * 2
    implicitHeight: prominent ? 40 : 34

    Canvas {
        id: plate
        anchors.fill: parent
        antialiasing: true

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            var cut = 5

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
            ctx.fillStyle = Theme.surface2
            ctx.fill()
            ctx.strokeStyle = Theme.accentDim
            ctx.lineWidth = 1.25
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(cut + 3, 1.25)
            ctx.lineTo(Math.min(w - cut - 3, 38), 1.25)
            ctx.strokeStyle = Theme.accentStrong
            ctx.lineWidth = 2
            ctx.stroke()
        }
    }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: Theme.spaceSm

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            visible: tag.showIndicator
            width: 5
            height: 5
            color: Theme.accent
        }

        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.text
            font.family: Theme.fontMono
            font.pointSize: tag.prominent ? Theme.fontCardTitle : Theme.fontBody
            font.weight: tag.prominent ? Font.DemiBold : Font.Medium
        }
    }
}
