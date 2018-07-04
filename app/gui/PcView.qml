import QtQuick 2.9
import QtQuick.Controls 2.2

import ComputerModel 1.0

Frame {
    GridView {
        anchors.fill: parent
        cellWidth: 200; cellHeight: 300;
        focus: true

        model: ComputerModel {}

        delegate: Item {
            width: 300; height: 300;

            Image {
                id: pcIcon
                y: 10; anchors.horizontalCenter: parent.horizontalCenter;
                source: {
                    model.addPc ? "ic_add_to_queue_white_48px.svg" : "ic_tv_white_48px.svg"
                }
                sourceSize {
                    width: 200
                    height: 200
                }
            }

            Text {
                id: pcNameText
                text: model.name
                color: "white"

                width: parent.width
                anchors.top: pcIcon.bottom
                minimumPointSize: 12
                font.pointSize: 36
                horizontalAlignment: Text.AlignHCenter
                fontSizeMode: Text.HorizontalFit
            }

            Text {
                function getStatusText(model)
                {
                    if (model.online) {
                        var text = "<font color=\"green\">Online</font>"
                        text += "<font color=\"white\"> - </font>"
                        if (model.paired) {
                            text += "<font color=\"skyblue\">Paired</font>"
                        }
                        else if (model.busy) {
                            text += "<font color=\"red\">Busy</font>"
                        }
                        else {
                            text += "<font color=\"red\">Not Paired</font>"
                        }
                        return text
                    }
                    else {
                        return "<font color=\"red\">Offline</font>";
                    }
                }

                id: pcPairedText
                text: getStatusText(model)

                width: parent.width
                anchors.top: pcNameText.bottom
                minimumPointSize: 12
                font.pointSize: 36
                horizontalAlignment: Text.AlignHCenter
                fontSizeMode: Text.HorizontalFit
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    parent.GridView.view.currentIndex = index
                }
            }
        }
    }
}
