import QtQuick 2.11
import QtQuick.Controls 2.2

// https://stackoverflow.com/questions/45029968/how-do-i-set-the-combobox-width-to-fit-the-largest-item
ComboBox {
    property int modelWidth

    width: modelWidth + 2*leftPadding + 2*rightPadding

    TextMetrics {
        id: textMetrics
        font: parent.font
    }

    onFontChanged: {
        textMetrics.font = font
    }

    // We call this every time the options change (and init)
    // so we can adjust the combo box width here too
    onActivated: {
        for (var i = 0; i < count; i++){
            textMetrics.text = textAt(i)
            modelWidth = Math.max(textMetrics.width, modelWidth)
        }
    }
}
