import QtQuick 2.9
import QtQuick.Controls 2.2

ApplicationWindow {
    id: window
    visible: true
    width: 640
    height: 480
    title: qsTr("Stack")
    color: "#333333"

    StackView {
        id: stackView
        initialItem: "PcView.qml"
        anchors.fill: parent
    }
}
