import QtQuick 2.9
import QtQuick.Controls 2.2

Page {
    id: settingsPage
    objectName: "Settings"

    Column {
        x: 10
        y: 10
        width: settingsPage.width
        height: 400

        GroupBox {
            // TODO save the settings
            id: basicSettingsGroupBox
            width: (parent.width - 20)
            padding: 12
            title: qsTr("Basic Settings")
            font.pointSize: 12

            Column {
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
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)
                    font.pointSize: 9
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

                CheckBox {
                    id: fullScreenCheck
                    text: qsTr("Stretch video to full-screen")
                    font.pointSize:  12
                }

                CheckBox {
                    id: pipObserverCheck
                    text: qsTr("Enable Picture-in-Picture observer mode")
                    font.pointSize: 12
                }
            }
        }

        GroupBox {
            id: audioSettingsGroupBox
            width: (parent.width - 20)
            padding: 12
            title: qsTr("Audio Settings")
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: surroundSoundCheck
                    text: qsTr("Enable 5.1 surround sound")
                    font.pointSize:  12
                }
            }
        }

        GroupBox {
            id: gamepadSettingsGroupBox
            width: (parent.width - 20)
            padding: 12
            title: qsTr("Gamepad Settings")
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: multiControllerCheck
                    text: qsTr("Multiple controller support")
                    font.pointSize:  12
                }
                CheckBox {
                    id: mouseEmulationCheck
                    text: qsTr("Mouse emulation via gamepad")
                    font.pointSize:  12
                }
            }
        }

        GroupBox {
            id: onScreenControlsGroupBox
            width: (parent.width - 20)
            padding: 12
            title: qsTr("On-screen Controls Settings")
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: onScreenControlsCheck
                    text: qsTr("Show on-screen controls")
                    font.pointSize:  12
                }
            }
        }

        GroupBox {
            id: hostSettingsGroupBox
            width: (parent.width - 20)
            padding: 12
            title: qsTr("Host Settings")
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: optimizeGameSettingsCheck
                    text: qsTr("Optimize Game Settings")
                    font.pointSize:  12
                }

                CheckBox {
                    id: audioPcCheck
                    text: qsTr("Play audio on PC")
                    font.pointSize:  12
                }
            }
        }

        GroupBox {
            id: advancedSettingsGroupBox
            width: (parent.width - 20)
            padding: 12
            title: qsTr("Advanced Settings")
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: neverDropFramesCheck
                    text: qsTr("Never Drop Frames")
                    font.pointSize:  12
                }
            }
        }
    }
}
