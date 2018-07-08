import QtQuick 2.9
import QtQuick.Controls 2.2

ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 600
    color: "#333333"

    StackView {
        id: stackView
        initialItem: "PreferencesView.qml"
        anchors.fill: parent
    }
}
