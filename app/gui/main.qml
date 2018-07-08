import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 600
    color: "#333333"

    StackView {
        id: stackView
        initialItem: "PcView.qml"
        anchors.fill: parent
    }

    header: ToolBar {

        RowLayout {
            spacing: 20
            anchors.fill: parent

            ToolButton {
                // Only make the button visible if the user has navigated somewhere.
                visible: stackView.depth > 1

                icon.source: "qrc:/res/arrow_left.png"
                onClicked: stackView.pop()

                Menu {
                    id: backButton
                    x: parent.width - width
                    transformOrigin: Menu.TopRight
                }
            }

            Label {
                id: titleLabel
                text: stackView.currentItem.objectName
                font.pointSize: 15
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }

            ToolButton {
                icon.source: "qrc:/res/settings.png"
                onClicked: stackView.push("qrc:/gui/SettingsView.qml")

                Menu {
                    id: optionsMenu
                    x: parent.width
                    transformOrigin: Menu.TopRight
                }
            }

            ToolButton {
                icon.source: "qrc:/res/question_mark.png"

                // TODO need to make sure browser is brought to foreground.
                onClicked: Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide");

                Menu {
                    id: helpButton
                    x: parent.width
                    transformOrigin: Menu.TopRight
                }
            }
        }
    }
}
