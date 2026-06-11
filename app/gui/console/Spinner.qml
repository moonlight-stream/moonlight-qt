import QtQuick

// Indicateur d'activité circulaire : arc orange en rotation continue.
// Aucune dépendance — utilisable partout dans l'UI console.
Item {
    id: root

    property color color: "#F2802A"
    property real lineWidth: 3

    implicitWidth: 36
    implicitHeight: 36

    Canvas {
        id: arc
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.color
            ctx.lineWidth = root.lineWidth
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(width / 2, height / 2,
                    (Math.min(width, height) - root.lineWidth) / 2 - 1,
                    0, Math.PI * 1.5)
            ctx.stroke()
        }

        RotationAnimation on rotation {
            from: 0; to: 360
            duration: 900
            loops: Animation.Infinite
            running: root.visible
        }
    }
}
