import QtQuick 2.9
import QtQuick.Controls 2.2

Page {
    id: settingsPage
    objectName: "Settings"

    Column {
        id: column
        x: 10
        y: 10
        width: 200
        height: 400

        GroupBox {
            // TODO save the settings
            id: streamingSettingsGroupBox
            width: 200
            height: 200
            padding: 12
            title: qsTr("Streaming Settings")

            Column {
                id: column1
                anchors.fill: parent
                spacing: 5

                Label {
                    id: label
                    text: qsTr("Resolution")
                }

                ComboBox {
                    id: resolutionComboBox
                    model: ListModel {
                        id: resolutionListModel
                        ListElement { text: "Resolution1" }
                        ListElement { text: "Resolution2" }
                        ListElement { text: "Resolution3" }
                    }
                    onCurrentIndexChanged: console.debug(resolutionListModel.get(currentIndex).text + " selected resolution")
                }

                Label {
                    id: fpsLabel
                    text: qsTr("FPS")
                }

                ComboBox {
                    id: fpsComboBox
                    model: ListModel {
                        id: fpsListModel
                        ListElement { text: "FPS1" }
                        ListElement { text: "FPS2" }
                        ListElement { text: "FPS3" }
                    }
                    onCurrentIndexChanged: console.debug(resolutionListModel.get(currentIndex).text + " selected FPS")
                }
            }
        }
    }



    // TODO - add settings


}
