import QtQuick 2.0
import QtQuick.Controls 6.3
import QtQuick.Dialogs 6.3
import QtQuick.Layouts 1.2

NavigableDialog {
    id: dialog

    property alias text: dialogLabel.dialogText
    property alias showSpinner: dialogSpinner.visible
    property alias imageSrc: dialogImage.source

    property string helpText
    property string helpUrl : "https://github.com/moonlight-stream/moonlight-docs/wiki/Troubleshooting"
    property string helpTextSeparator : " "

    onOpened: {
        // Force keyboard focus on the label so keyboard navigation works
        dialogLabel.forceActiveFocus()
    }

    background: Rectangle {
        color: Qt.rgba(0.1, 0.1, 0.1, 0.85)  // 深色半透明背景
        border.color: Qt.rgba(0.3, 0.3, 0.3, 0.6)
        border.width: 1
        radius: 5
    }

    RowLayout {
        spacing: 10

        BusyIndicator {
            id: dialogSpinner
            visible: false
        }

        Image {
            id: dialogImage
            source: (standardButtons & Dialog.Yes) ?
                        "qrc:/res/baseline-help_outline-24px.svg" :
                        "qrc:/res/baseline-error_outline-24px.svg"
            sourceSize {
                // The icon should be square so use the height as the width too
                width: 50
                height: 50
            }
            visible: !showSpinner
        }

        Label {
            property string dialogText

            id: dialogLabel
            text: dialogText + ((helpText && (standardButtons & Dialog.Help)) ? (helpTextSeparator + helpText) : "")
            wrapMode: Text.Wrap
            elide: Label.ElideRight

            // Cap the width so the dialog doesn't grow horizontally forever. This
            // will cause word wrap to kick in.
            Layout.maximumWidth: 400
            Layout.maximumHeight: 400

            Keys.onReturnPressed: {
                accept()
            }

            Keys.onEnterPressed: {
                accept()
            }

            Keys.onEscapePressed: {
                reject()
            }
        }
    }

    footer: DialogButtonBox {
        id: dialogButtonBox
        standardButtons: dialog.standardButtons

        onHelpRequested: {
            Qt.openUrlExternally(helpUrl)
            close()
        }
    }
}
