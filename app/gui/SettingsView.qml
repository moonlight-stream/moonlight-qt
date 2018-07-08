import QtQuick 2.9
import QtQuick.Controls 2.2

Page {
    id: settingsPage
    objectName: "Settings"

    Column {
        id: column
        x: 10
        y: 10
        width: settingsPage.width
        height: 400

        GroupBox {
            // TODO save the settings
            id: streamingSettingsGroupBox
            width: (parent.width - 20)
            //height: 200
            padding: 12
            title: qsTr("Basic Settings")

            Column {
                id: column1
                anchors.fill: parent
                spacing: 5

                Label {
                    width: parent.width
                    id: resFPStitle
                    text: qsTr("Resolution and FPS target")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                Label {
                    width: parent.width
                    id: resFPSdesc
                    text: qsTr("Setting values too high for your device may cause lag or crashing")
                    font.pointSize: 9
                    wrapMode: Text.Wrap
                }

                ComboBox {
                    id: resolutionComboBox
                    currentIndex : 4
                    model: ListModel {
                        id: resolutionListModel
                        // TODO have values associated with the text.
                        ListElement { text: "360p 30 FPS" }
                        ListElement { text: "360p 60 FPS" }
                        ListElement { text: "720p 30 FPS" }
                        ListElement { text: "720p 60 FPS" }
                        ListElement { text: "1080p 30 FPS" }
                        ListElement { text: "1080p 60 FPS" }
                        ListElement { text: "4K 30 FPS" }
                        ListElement { text: "4K 60 FPS" }
                    }
                    onCurrentIndexChanged: console.debug(resolutionListModel.get(currentIndex).text + " selected resolution")
                }

                Label {
                    width: parent.width
                    id: bitrateTitle
                    text: qsTr("Video bitrate target")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                }

                Label {
                    width: parent.width
                    id: bitrateDesc
                    text: qsTr("Lower bitrate to reduce stuttering. Raise bitrate to increase image quality.")
                    font.pointSize: 9
                    wrapMode: Text.Wrap
                }

                Slider {
                    id: slider
                    wheelEnabled: true

                    // TODO value should be loaded as the current value.
                    value: 500
                    stepSize: 500
                    from : 500
                    to: 10000
                    snapMode: "SnapOnRelease"
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)

                    // TODO store the value
                    // TODO display the current value to the user
                    onValueChanged:
                    {
                        console.debug(slider.value + " Slider moved")
                    }
                }
            }
        }
    }



    // TODO - add settings


}
