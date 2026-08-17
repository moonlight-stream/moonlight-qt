import QtQuick 2.9

import StreamingPreferences 1.0

import "theme"

Canvas {
    id: preview

    property int modeValue: StreamingPreferences.SCM_FOLLOW_HOST
    property bool selected: false

    antialiasing: true

    onModeValueChanged: requestPaint()
    onSelectedChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function drawDisplay(ctx, x, y, width, height, fill, stroke) {
        ctx.fillStyle = fill
        ctx.fillRect(x, y, width, height)
        ctx.strokeStyle = stroke
        ctx.lineWidth = 2
        ctx.strokeRect(x, y, width, height)

        ctx.fillStyle = stroke
        ctx.fillRect(x + width * 0.32, y + height + 5, width * 0.36, 3)
    }

    function drawConnection(ctx, fromX, toX, y, color) {
        ctx.strokeStyle = color
        ctx.lineWidth = 2
        ctx.beginPath()
        ctx.moveTo(fromX, y)
        ctx.lineTo(toX, y)
        ctx.stroke()
    }

    function drawArrow(ctx, fromX, toX, y, color) {
        drawConnection(ctx, fromX, toX, y, color)
        ctx.beginPath()
        ctx.moveTo(toX, y)
        ctx.lineTo(toX - 7, y - 5)
        ctx.moveTo(toX, y)
        ctx.lineTo(toX - 7, y + 5)
        ctx.stroke()
    }

    function drawPrimaryBadge(ctx, x, y, color) {
        ctx.fillStyle = color
        ctx.beginPath()
        ctx.arc(x, y, 6, 0, Math.PI * 2)
        ctx.fill()
        ctx.fillStyle = Theme.ink
        ctx.beginPath()
        ctx.arc(x, y, 2, 0, Math.PI * 2)
        ctx.fill()
    }

    function drawPause(ctx, x, y, color) {
        ctx.fillStyle = color
        ctx.fillRect(x - 7, y - 9, 5, 18)
        ctx.fillRect(x + 2, y - 9, 5, 18)
    }

    function drawGear(ctx, x, y, color) {
        var radius = 7
        ctx.strokeStyle = color
        ctx.lineWidth = 2
        ctx.beginPath()
        ctx.arc(x, y, radius * 0.55, 0, Math.PI * 2)
        ctx.stroke()

        for (var i = 0; i < 8; i++) {
            var angle = Math.PI * 2 * i / 8
            ctx.beginPath()
            ctx.moveTo(x + Math.cos(angle) * radius * 0.72,
                       y + Math.sin(angle) * radius * 0.72)
            ctx.lineTo(x + Math.cos(angle) * radius,
                       y + Math.sin(angle) * radius)
            ctx.stroke()
        }
    }

    function drawDisabledSlash(ctx, x, y, width, height, color) {
        ctx.strokeStyle = color
        ctx.lineWidth = 2
        ctx.beginPath()
        ctx.moveTo(x + 4, y + height - 4)
        ctx.lineTo(x + width - 4, y + 4)
        ctx.stroke()
    }

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)

        var activeFill = selected ? Theme.accent : Theme.accentDim
        var activeStroke = Theme.accentStrong
        var idleFill = Qt.rgba(0.93, 0.94, 0.92, 0.12)
        var idleStroke = Qt.rgba(0.93, 0.94, 0.92, 0.58)
        var mutedStroke = Qt.rgba(0.93, 0.94, 0.92, 0.30)
        var screenWidth = width * 0.36
        var screenHeight = height * 0.44
        var screenY = height * 0.20
        var leftX = width * 0.06
        var rightX = width * 0.58
        var centerX = width * 0.32
        var middleY = screenY + screenHeight * 0.5

        switch (modeValue) {
        case StreamingPreferences.SCM_FOLLOW_HOST:
            drawDisplay(ctx, leftX, screenY, screenWidth, screenHeight, idleFill, idleStroke)
            drawDisplay(ctx, rightX, screenY, screenWidth, screenHeight, idleFill, idleStroke)
            drawConnection(ctx, leftX + screenWidth + 5, rightX - 5, middleY, idleStroke)
            drawGear(ctx, leftX + screenWidth * 0.5, middleY, idleStroke)
            break
        case StreamingPreferences.SCM_NO_OPERATION:
            drawDisplay(ctx, leftX, screenY, screenWidth, screenHeight, "transparent", mutedStroke)
            drawDisplay(ctx, rightX, screenY, screenWidth, screenHeight, "transparent", mutedStroke)
            drawPause(ctx, width * 0.5, middleY, idleStroke)
            break
        case StreamingPreferences.SCM_ENSURE_ACTIVE:
            drawDisplay(ctx, leftX, screenY, screenWidth, screenHeight, idleFill, idleStroke)
            drawArrow(ctx, leftX + screenWidth + 5, rightX - 5, middleY, Theme.accent)
            drawDisplay(ctx, rightX, screenY, screenWidth, screenHeight, activeFill, activeStroke)
            break
        case StreamingPreferences.SCM_ENSURE_PRIMARY:
            drawDisplay(ctx, leftX, screenY, screenWidth, screenHeight, idleFill, idleStroke)
            drawDisplay(ctx, rightX, screenY, screenWidth, screenHeight, activeFill, activeStroke)
            drawPrimaryBadge(ctx, rightX + screenWidth * 0.5, screenY - 2, activeStroke)
            break
        case StreamingPreferences.SCM_ENSURE_SECONDARY:
            drawDisplay(ctx, leftX, screenY, screenWidth, screenHeight,
                        Qt.rgba(0.93, 0.94, 0.92, 0.20), Theme.text)
            drawPrimaryBadge(ctx, leftX + screenWidth * 0.5, screenY - 2, Theme.text)
            drawConnection(ctx, leftX + screenWidth + 5, rightX - 5, middleY, Theme.accent)
            drawDisplay(ctx, rightX, screenY, screenWidth, screenHeight,
                        Qt.rgba(0.22, 0.77, 0.73, 0.22), activeStroke)
            break
        case StreamingPreferences.SCM_ENSURE_ONLY_DISPLAY:
            var sideWidth = width * 0.22
            var sideHeight = height * 0.34
            var sideY = height * 0.29
            drawDisplay(ctx, width * 0.02, sideY, sideWidth, sideHeight, "transparent", mutedStroke)
            drawDisplay(ctx, width * 0.76, sideY, sideWidth, sideHeight, "transparent", mutedStroke)
            drawDisabledSlash(ctx, width * 0.02, sideY, sideWidth, sideHeight, Theme.accentDim)
            drawDisabledSlash(ctx, width * 0.76, sideY, sideWidth, sideHeight, Theme.accentDim)
            drawDisplay(ctx, centerX, height * 0.17, screenWidth, screenHeight, activeFill, activeStroke)
            break
        default:
            drawDisplay(ctx, leftX, screenY, screenWidth, screenHeight, idleFill, idleStroke)
            drawDisplay(ctx, rightX, screenY, screenWidth, screenHeight, activeFill, activeStroke)
            break
        }
    }
}
