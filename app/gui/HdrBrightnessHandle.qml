import QtQuick 2.12
import QtQuick.Controls

import "theme"

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

    FocusRing {
        visible: handle.activeFocus
        inset: 1
    }

    // 三个把手全部方角，配色跟着刻度条的三段：暗青 / 青 / 酸性绿。
    //
    // 焦点不画在把手自己的描边上：三个把手的填充分别是 accentDim / accent / acid，
    // 描边已经被「这是哪一段」占住了（中间那个还正好是 accent，accent 描 accent
    // 等于看不见）。所以统一挂全应用那个方角 FocusRing，跟 CheckBox / Switch /
    // Slider 把手一个规矩。
    Rectangle {
        anchors.centerIn: parent
        visible: handle.handleStyle === handle.minimumStyle
        width: 10
        height: 10
        radius: 0
        color: Theme.accentDim
        border.width: 2
        border.color: Theme.ink
    }

    Rectangle {
        anchors.centerIn: parent
        visible: handle.handleStyle === handle.averageStyle
        width: 5
        height: handle.scaleItem ? handle.scaleItem.height + 12 : 26
        radius: 0
        color: Theme.accent
        border.width: 0
    }

    Rectangle {
        anchors.centerIn: parent
        visible: handle.handleStyle === handle.peakStyle
        width: 14
        height: 14
        radius: 0
        color: Theme.acid
        border.width: 2
        border.color: Theme.ink

        // 峰值把手带一圈酸性绿光晕，替代参考站的 box-shadow: 0 0 12px
        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 8
            height: width
            radius: 0
            color: "transparent"
            border.width: 2
            border.color: Theme.acidGlow
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
