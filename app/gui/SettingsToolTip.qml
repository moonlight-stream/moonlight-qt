import QtQuick 2.9
import QtQuick.Window 2.2

ToolbarToolTip {
    id: tip
    visible: parent.visible && parent.hovered
    delay: 800
    timeout: 5000
    margins: 8
    width: Math.min(implicitWidth, 420, Math.max(1, parent.Window.width - 2 * margins))
    x: Math.min(12, (parent.width - width) / 2)
    y: -height - 5
}
