import QtQuick 2.9
import QtQuick.Controls 2.2

import StreamingPreferences 1.0

ScrollView {
    id: settingsPage
    objectName: "Settings"

    StreamingPreferences {
        id: prefs
    }

    Component.onDestruction: {
        prefs.save()
    }

    Column {
        x: 10
        y: 10
        width: settingsPage.width
        height: 400

        GroupBox {
            id: basicSettingsGroupBox
            width: (parent.width - 20)
            padding: 12
            title: "<font color=\"skyblue\">Basic Settings</font>"
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
                    color: "white"
                }

                Label {
                    width: parent.width
                    id: resFPSdesc
                    text: qsTr("Setting values too high for your device may cause lag or crashes")
                    font.pointSize: 9
                    wrapMode: Text.Wrap
                    color: "white"
                }

                ComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        // load the saved width/height/fps, and iterate through the ComboBox until a match is found
                        // set it to that index.
                        var saved_width = prefs.width
                        var saved_height = prefs.height
                        var saved_fps = prefs.fps
                        currentIndex = 0
                        for(var i = 0; i < resolutionComboBox.count; i++) {
                            var el_width = parseInt(resolutionListModel.get(i).video_width);
                            var el_height = parseInt(resolutionListModel.get(i).video_height);
                            var el_fps = parseInt(resolutionListModel.get(i).video_fps);
                          if(saved_width === el_width &&
                                  saved_height === el_height &&
                                  saved_fps === el_fps) {
                              currentIndex = i
                          }
                        }
                    }

                    id: resolutionComboBox
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)
                    font.pointSize: 9
                    textRole: "text"
                    model: ListModel {
                        id: resolutionListModel
                        ListElement {
                            text: "720p 30 FPS"
                            video_width: "1280"
                            video_height: "720"
                            video_fps: "30"
                        }
                        ListElement {
                            text: "720p 60 FPS"
                            video_width: "1280"
                            video_height: "720"
                            video_fps: "60"
                        }
                        ListElement {
                            text: "1080p 30 FPS"
                            video_width: "1920"
                            video_height: "1080"
                            video_fps: "30"
                        }
                        ListElement {
                            text: "1080p 60 FPS"
                            video_width: "1920"
                            video_height: "1080"
                            video_fps: "60"
                        }
                        ListElement {
                            text: "4K 30 FPS"
                            video_width: "3840"
                            video_height: "2160"
                            video_fps: "30"
                        }
                        ListElement {
                            text: "4K 60 FPS"
                            video_width: "3840"
                            video_height: "2160"
                            video_fps: "60"
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        prefs.width = parseInt(resolutionListModel.get(currentIndex).video_width)
                        prefs.height = parseInt(resolutionListModel.get(currentIndex).video_height)
                        prefs.fps = parseInt(resolutionListModel.get(currentIndex).video_fps)

                        prefs.bitrateKbps = prefs.getDefaultBitrate(prefs.width, prefs.height, prefs.fps);
                        slider.value = prefs.bitrateKbps
                    }
                }

                Label {
                    width: parent.width
                    id: bitrateTitle
                    text: qsTr("Video bitrate: ")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                    color: "white"
                }

                Label {
                    width: parent.width
                    id: bitrateDesc
                    text: qsTr("Lower bitrate to reduce lag and stuttering. Raise bitrate to increase image quality.")
                    font.pointSize: 9
                    wrapMode: Text.Wrap
                    color: "white"
                }

                Slider {
                    id: slider
                    wheelEnabled: true

                    value: prefs.bitrateKbps

                    stepSize: 500
                    from : 500
                    to: 100000

                    snapMode: "SnapOnRelease"
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)

                    onValueChanged: {
                        bitrateTitle.text = "Video bitrate: " + (value / 1000.0) + " Mbps"
                        prefs.bitrateKbps = value
                    }
                }

                CheckBox {
                    id: fullScreenCheck
                    text: "<font color=\"white\">Full-screen</font>"
                    font.pointSize:  12
                    checked: prefs.fullScreen
                    onCheckedChanged: {
                        prefs.fullScreen = checked
                    }
                }
            }
        }

        GroupBox {

            id: audioSettingsGroupBox
            width: (parent.width - 20)
            padding: 12
            title: "<font color=\"skyblue\">Audio Settings</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                Label {
                    width: parent.width
                    id: resAudioTitle
                    text: qsTr("Audio configuration")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                    color: "white"
                }

                ComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_audio = prefs.audioConfig
                        currentIndex = 0
                        for(var i = 0; i < audioListModel.count; i++) {
                            var el_audio= audioListModel.get(i).val;
                          if(saved_audio === el_audio) {
                              currentIndex = i
                          }
                        }
                    }

                    id: audioComboBox
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)
                    font.pointSize: 9
                    textRole: "text"
                    model: ListModel {
                        id: audioListModel
                        ListElement {
                            text: "Auto"
                            val: StreamingPreferences.AC_AUTO
                        }
                        ListElement {
                            text: "Force stereo"
                            val: StreamingPreferences.AC_FORCE_STEREO
                        }
                        ListElement {
                            text: "Force surround"
                            val: StreamingPreferences.AC_FORCE_SURROUND
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        prefs.audioConfig = audioListModel.get(currentIndex).val
                    }
                }

            }
        }

        GroupBox {
            id: gamepadSettingsGroupBox
            width: (parent.width - 20)
            padding: 12
            title: "<font color=\"skyblue\">Gamepad Settings</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: multiControllerCheck
                    text: "<font color=\"white\">Multiple controller support</font>"
                    font.pointSize:  12
                    checked: prefs.multiController
                    onCheckedChanged: {
                        prefs.multiController = checked
                    }
                }
            }
        }

        GroupBox {
            id: hostSettingsGroupBox
            width: (parent.width - 20)
            padding: 12
            title: "<font color=\"skyblue\">Host Settings</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                CheckBox {
                    id: optimizeGameSettingsCheck
                    text: "<font color=\"white\">Optimize game settings</font>"
                    font.pointSize:  12
                    checked: prefs.gameOptimizations
                    onCheckedChanged: {
                        prefs.gameOptimizations = checked
                    }
                }

                CheckBox {
                    id: audioPcCheck
                    text: "<font color=\"white\">Play audio on host PC</font>"
                    font.pointSize:  12
                    checked: prefs.playAudioOnHost
                    onCheckedChanged: {
                        prefs.playAudioOnHost = checked
                    }
                }
            }
        }

        GroupBox {
            id: advancedSettingsGroupBox
            width: (parent.width - 20)
            padding: 12
            title: "<font color=\"skyblue\">Advanced Settings</font>"
            font.pointSize: 12

            Column {
                anchors.fill: parent
                spacing: 5

                Label {
                    width: parent.width
                    id: resVDSTitle
                    text: qsTr("Video decoder")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                    color: "white"
                }

                ComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_vds = prefs.videoDecoderSelection
                        currentIndex = 0
                        for(var i = 0; i < decoderListModel.count; i++) {
                            var el_vds = decoderListModel.get(i).val;
                          if(saved_vds === el_vds) {
                              currentIndex = i
                          }
                        }
                    }

                    id: decoderComboBox
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)
                    font.pointSize: 9
                    textRole: "text"
                    model: ListModel {
                        id: decoderListModel
                        ListElement {
                            text: "Auto"
                            val: StreamingPreferences.VDS_AUTO
                        }
                        ListElement {
                            text: "Force software decoding"
                            val: StreamingPreferences.VDS_FORCE_SOFTWARE
                        }
                        ListElement {
                            text: "Force hardware decoding"
                            val: StreamingPreferences.VDS_FORCE_HARDWARE
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        prefs.videoDecoderSelection = decoderListModel.get(currentIndex).val
                    }
                }

                Label {
                    width: parent.width
                    id: resVCCTitle
                    text: qsTr("Video codec")
                    font.pointSize: 12
                    wrapMode: Text.Wrap
                    color: "white"
                }

                ComboBox {
                    // ignore setting the index at first, and actually set it when the component is loaded
                    Component.onCompleted: {
                        var saved_vcc = prefs.videoCodecConfig
                        currentIndex = 0
                        for(var i = 0; i < codecListModel.count; i++) {
                            var el_vcc = codecListModel.get(i).val;
                          if(saved_vcc === el_vcc) {
                              currentIndex = i
                          }
                        }
                    }

                    id: codecComboBox
                    width: Math.min(bitrateDesc.implicitWidth, parent.width)
                    font.pointSize: 9
                    textRole: "text"
                    model: ListModel {
                        id: codecListModel
                        ListElement {
                            text: "Auto"
                            val: StreamingPreferences.VCC_AUTO
                        }
                        ListElement {
                            text: "Force H264"
                            val: StreamingPreferences.VCC_FORCE_H264
                        }
                        ListElement {
                            text: "Force HEVC"
                            val: StreamingPreferences.VCC_FORCE_HEVC
                        }
                        ListElement {
                            text: "Force HEVC HDR"
                            val: StreamingPreferences.VCC_FORCE_HEVC_HDR
                        }
                    }
                    // ::onActivated must be used, as it only listens for when the index is changed by a human
                    onActivated : {
                        prefs.videoCodecConfig = codecListModel.get(currentIndex).val
                    }
                }

            }
        }
    }
}
