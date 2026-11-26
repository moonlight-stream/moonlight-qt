import QtQuick 2.9
import QtQuick.Controls 2.5

Button {
    id: control
    hoverEnabled: true
    implicitWidth: Math.max(94, contentItem.implicitWidth + 28)
    implicitHeight: Math.max(26, contentItem.implicitHeight + 6)
    padding: 0
    topInset: 0
    bottomInset: 0
    font.pixelSize: 14
    font.capitalization: Font.MixedCase
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? "#20354c" : "#718092"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 4
        color: control.down ? "#dbe7f7" : control.hovered && control.enabled ? "#f1f6fd" : "white"
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? "#2468d5" : "#cbd5df"
    }
}
