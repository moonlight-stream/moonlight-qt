import QtQuick 2.12
import QtQuick.Controls 2.2

Item {
    id: handle

    readonly property int minimumStyle: 0
    readonly property int averageStyle: 1
    readonly property int peakStyle: 2

    property Item scaleItem
    property int handleStyle: minimumStyle
    property real brightnessValue: 0
    readonly property real value: brightnessValue
    property real minimumValue: 0
    property real maximumValue: 10000
    property int decimalPlaces: 3
    property string valueLabel
    property string unitLabel
    property int restingZ: 0
    property int draggedZ: restingZ

    signal dragMoved(real scaleX)
    signal nudgeRequested(int direction, bool accelerated)

    width: handleStyle === peakStyle ? 30 : 26
    height: handleStyle === averageStyle ? 48 :
            handleStyle === peakStyle ? 40 : 34
    activeFocusOnTab: true
    z: dragHandler.active ? draggedZ : restingZ

    Accessible.role: Accessible.Slider
    Accessible.name: valueLabel

    Rectangle {
        anchors.centerIn: parent
        visible: handle.handleStyle === handle.minimumStyle
        width: 10
        height: 10
        radius: width / 2
        color: "#64B5F6"
        border.width: 2
        border.color: handle.activeFocus ? "#FFFFFF" : "#D9FFFFFF"
    }

    Rectangle {
        anchors.centerIn: parent
        visible: handle.handleStyle === handle.averageStyle
        width: 5
        height: handle.scaleItem ? handle.scaleItem.height + 12 : 26
        radius: 2
        color: "#A8D3EC"
        border.width: handle.activeFocus ? 1 : 0
        border.color: "#FFFFFF"
    }

    Rectangle {
        anchors.centerIn: parent
        visible: handle.handleStyle === handle.peakStyle
        width: 14
        height: 14
        radius: width / 2
        color: "#F4FAFF"
        border.width: 2
        border.color: handle.activeFocus ? "#FFFFFF" : "#FFB86B"

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 8
            height: width
            radius: width / 2
            color: "transparent"
            border.width: 2
            border.color: "#45FFB86B"
            z: -1
        }
    }

    DragHandler {
        id: dragHandler
        target: null
        xAxis.enabled: true
        yAxis.enabled: false
        dragThreshold: 6
        cursorShape: Qt.SizeHorCursor
        property real startScaleX: 0

        onActiveChanged: {
            if (active) {
                handle.forceActiveFocus()
                var handleCenter = handle.mapToItem(handle.scaleItem,
                                                    handle.width / 2,
                                                    handle.height / 2)
                startScaleX = handleCenter.x
                handle.dragMoved(startScaleX +
                                 centroid.scenePosition.x -
                                 centroid.scenePressPosition.x)
            }
        }

        onCentroidChanged: {
            if (active && handle.scaleItem) {
                handle.dragMoved(startScaleX +
                                 centroid.scenePosition.x -
                                 centroid.scenePressPosition.x)
            }
        }
    }

    HoverHandler {
        id: hoverHandler
        cursorShape: Qt.SizeHorCursor
    }

    ToolTip.visible: hoverHandler.hovered || dragHandler.active || handle.activeFocus
    ToolTip.delay: dragHandler.active ? 0 : 350
    ToolTip.text: handle.valueLabel + "\n" +
                  Number(Number(handle.brightnessValue).toFixed(
                             handle.decimalPlaces)).toString() +
                  " " + handle.unitLabel

    Keys.onLeftPressed: function(event) {
        nudgeRequested(-1, (event.modifiers & Qt.ShiftModifier) !== 0)
        event.accepted = true
    }
    Keys.onRightPressed: function(event) {
        nudgeRequested(1, (event.modifiers & Qt.ShiftModifier) !== 0)
        event.accepted = true
    }
}
