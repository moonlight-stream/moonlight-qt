import QtQuick 2.9
import QtQuick.Controls 2.2

import SdlGamepadKeyNavigation 1.0

ComboBox {
    id: control
    property int textWidth
    property int maximumWidth: parent.width
    font.pixelSize: 13
    leftPadding: 10; rightPadding: 25
    topPadding: 2; bottomPadding: 2
    implicitHeight: 30
    implicitWidth: Math.min(maximumWidth, Math.max(74, selected.implicitWidth + leftPadding + rightPadding))
    hoverEnabled: true

    contentItem: Label {
        id: selected
        text: control.displayText
        font: control.font
        color: control.enabled ? "#233d50" : "#697c88"
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 5
        color: control.enabled && control.hovered ? "#eaf4f5" : "#f5f8f9"
        border.color: control.activeFocus ? "#167384" : "#c6d5db"
    }
    indicator: Item {
        width: 12; height: 8
        x: control.width - width - 9
        anchors.verticalCenter: parent.verticalCenter
        Repeater {
            model: 2
            Rectangle {
                width: 6; height: 1.5; radius: 1
                x: index ? 5 : 1; y: 3
                rotation: index ? -45 : 45
                color: "#486571"
            }
        }
    }
    popup.width: Math.min(window.width - 16, Math.max(control.width, textWidth + 36))

    TextMetrics {
        id: popupMetrics
    }

    TextMetrics {
        id: textMetrics
    }

    function recalculateWidth() {
        textMetrics.font = font
        popupMetrics.font = popup.font
        textWidth = 0
        for (var i = 0; i < count; i++){
            textMetrics.text = textAt(i)
            popupMetrics.text = textAt(i)
            textWidth = Math.max(textMetrics.width, textWidth)
            textWidth = Math.max(popupMetrics.width, textWidth)
        }
    }

    // We call this every time the options change (and init)
    // so we can adjust the combo box width here too
    onActivated: recalculateWidth()

    popup.onAboutToShow: {
        // Switch to normal navigation for combo boxes
        recalculateWidth()
        SdlGamepadKeyNavigation.setUiNavMode(false)

    }

    popup.onAboutToHide: {
        SdlGamepadKeyNavigation.setUiNavMode(true)
    }

    Keys.onLeftPressed: {
        decrementCurrentIndex()
    }

    Keys.onRightPressed: {
        incrementCurrentIndex()
    }
}
