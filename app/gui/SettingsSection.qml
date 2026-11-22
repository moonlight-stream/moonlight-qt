import QtQuick 2.9
import QtQuick.Controls 2.2

GroupBox {
    id: section
    topPadding: heading.height + padding + 9
    label: Label {
        id: heading
        x: section.leftPadding
        y: section.padding
        width: section.availableWidth
        text: section.title
        color: "#244655"; font.pixelSize: 16; font.bold: true
        wrapMode: Text.Wrap
    }
    background: Rectangle {
        color: "white"; radius: 9; border.color: "#d4e1e5"
    }
}
