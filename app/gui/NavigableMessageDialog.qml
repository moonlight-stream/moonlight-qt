import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

NavigableDialog {
    id: dialog

    property alias text: dialogLabel.dialogText
    property alias showSpinner: dialogSpinner.visible
    property alias imageSrc: dialogImage.source
    property bool showIcon: true

    property string helpText
    property string helpUrl : "https://github.com/moonlight-stream/moonlight-docs/wiki/Troubleshooting"
    property string helpTextSeparator : " "
    property string discardText

    onOpened: {
        if ((standardButtons & Dialog.Discard) && discardText) {
            dialogButtonBox.standardButton(Dialog.Discard).text = discardText
        }
        // Force keyboard focus on the label so keyboard navigation works
        if ((standardButtons & Dialog.Discard) && (standardButtons & Dialog.Ok)) {
            dialogButtonBox.standardButton(Dialog.Ok).forceActiveFocus(Qt.TabFocus)
        }
        else if (dialogButtonBox.count > 0) {
            dialogButtonBox.itemAt(dialogButtonBox.count - 1).forceActiveFocus(Qt.TabFocus)
        }
    }

    RowLayout {
        spacing: 10

        BusyIndicator {
            id: dialogSpinner
            visible: false
            running: visible
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
            visible: showIcon && !showSpinner
        }

        Label {
            property string dialogText

            id: dialogLabel
            text: dialogText + ((helpText && (standardButtons & (Dialog.Help | Dialog.Discard))) ? (helpTextSeparator + helpText) : "")
            wrapMode: Text.Wrap
            elide: Label.ElideRight

            // Cap the width so the dialog doesn't grow horizontally forever. This
            // will cause word wrap to kick in.
            Layout.maximumWidth: 400
            Layout.maximumHeight: 400
        }
    }

    footer: DialogButtonBox {
        id: dialogButtonBox
        standardButtons: dialog.standardButtons

        delegate: Button {
            flat: true

            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
            Keys.onRightPressed: nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocus)
            Keys.onLeftPressed: nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
        }

        onHelpRequested: {
            Qt.openUrlExternally(helpUrl)
            close()
        }
    }
}
