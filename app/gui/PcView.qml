import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Dialogs 1.3
import QtQuick.Layouts 1.11

import ComputerModel 1.0

Frame {
    anchors.fill: parent

    GridView {
        anchors.fill: parent
        anchors.leftMargin: 5
        anchors.topMargin: 5
        anchors.rightMargin: 5
        anchors.bottomMargin: 5
        cellWidth: 350; cellHeight: 350;
        focus: true

        model: ComputerModel {}

        delegate: Item {
            width: 300; height: 300;

            Image {
                id: pcIcon
                anchors.horizontalCenter: parent.horizontalCenter;
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
                font.pointSize: 48
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
                visible: !model.addPc

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
                    if(model.addPc) {
                        // TODO: read the output of the dialog
                        inputDialog.on
                        inputDialog.open()

                    } else if(!model.paired && !model.busy) {

                        // TODO: generate pin
                        pairDialog.text = pairDialog.text.replace("XXXX", "1234")
                        // TODO: initiate pairing request
                        pairDialog.open()
                    } else if (model.online) {
                        // go to game view
                    }
                }
            }
        }
    }

    MessageDialog {
        id: pairDialog
        // don't allow edits to the rest of the window while open
        modality:Qt.WindowModal

        title:"pair to a PC"
        text:"please enter XXXX on your GeForce enabled PC"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            console.log("accepted")
        }
        onRejected: {
            console.log("rejected")
        }
    }


    Dialog {
        id: inputDialog
        property string label: "enter the address of your GeForce enabled PC"
        property string hint: "example.foo.local"
        property alias editText : editTextItem

        standardButtons: StandardButton.Ok | StandardButton.Cancel
        onVisibleChanged: {
            editTextItem.focus = true
            editTextItem.selectAll()
        }
        onAccepted: {
            console.log("accepted, with value: " + editText.text)
        }
        onRejected: {
            console.log("rejected")
        }

        ColumnLayout {
            Text {
                id: inputDialogLabel
                text: inputDialog.label
            }
            Rectangle {
                implicitWidth: parent.parent.width
                height: editTextItem.height

                TextInput {
                    // TODO: calculate this properly
                    height: 20
                    id: editTextItem
                    inputMethodHints: Qt.ImhPreferUppercase
                    text: inputDialog.hint
                }
            }
        }
    }
}
