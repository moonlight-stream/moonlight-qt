import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Material 2.1
import QtQuick.Controls.Universal 2.1
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
        Material.foreground: "black"

        RowLayout {
            spacing: 20
            anchors.fill: parent

            ToolButton {
                icon.source: "qrc:/res/settings.png"
                onClicked: stackView.push("qrc:/gui/SettingsView.qml")

                Menu {
                    id: optionsMenu
                    x: parent.width - width
                    transformOrigin: Menu.TopRight
                }
            }

            ToolButton {
                icon.source: "qrc:/res/question_mark.png"

                // TODO need to bring browser to foreground.
                onClicked: Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide");

                Menu {
                    id: helpButton
                    x: parent.width - width
                    transformOrigin: Menu.TopRight
                }
            }

            Label {
                id: titleLabel
                text: "TODO - need the page name as a string"
                font.pixelSize: 20
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }
        }

    }
    Drawer {
        id: drawer
        width: Math.min(window.width, window.height) / 3 * 2
        height: window.height
        interactive: stackView.depth === 1

        ListView {
            id: listView

            focus: true
            currentIndex: -1
            anchors.fill: parent

            delegate: ItemDelegate {
                width: parent.width
                text: model.title
                highlighted: ListView.isCurrentItem
                onClicked: {
                    listView.currentIndex = index
                    stackView.push(model.source)
                    drawer.close()
                }
            }

            model: ListModel {
                ListElement { title: "Settings"; source: "qrc:/gui/SettingsView.qml" }
                ListElement { title: "PCs"; source: "qrc:/gui/PcView.qml" }
            }

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }

}
