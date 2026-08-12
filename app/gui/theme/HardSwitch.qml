import QtQuick 2.9
import QtQuick.Controls
import "."

// 方角开关：方轨 + 方滑块，滑块位移不带回弹。
Switch {
    id: control

    font.family: Theme.fontSans
    implicitWidth: 44
    implicitHeight: 22
    padding: 0
    spacing: 0

    function commitPointerToggle() {
        if (!control.enabled) {
            return
        }

        var previousChecked = control.checked
        control.toggle()
        if (control.checked !== previousChecked) {
            control.toggled()
        }
    }

    indicator: Rectangle {
        id: track
        implicitWidth: 44
        implicitHeight: 22
        x: control.text ? control.leftPadding
                        : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        radius: 0
        color: control.checked ? Theme.accent : Theme.surface2
        border.width: 1
        border.color: !control.enabled ? Theme.line
                    : control.checked ? Theme.accent
                    : (control.hovered || control.visualFocus ? Theme.accent : Theme.lineStrong)
        opacity: control.enabled ? 1.0 : 0.45

        Behavior on color {
            ColorAnimation { duration: Theme.durFast }
        }

        Rectangle {
            id: handle
            y: 3
            x: 3 + (pointerArea.dragging ? pointerArea.dragPosition
                                         : control.visualPosition) * (parent.width - width - 6)
            width: 16
            height: parent.height - 6
            radius: 0
            color: control.checked ? Theme.ink : Theme.textDim

            Behavior on x {
                enabled: !pointerArea.pressed && !control.down
                NumberAnimation { duration: Theme.durFast; easing.type: Theme.easing }
            }
        }

        // Own the pointer gesture on the painted track. This avoids the
        // FluentWinUI3 Switch regression where a stationary release is ignored,
        // while retaining both tap-to-toggle and drag-to-select behavior.
        //
        // 这条自管的指针路径必须同时切换状态并发出 toggled()：ToggleRow 靠它把值
        // 写回偏好设置，而 checkedChanged 在初始化恢复绑定值时也会触发，不能代替。
        // 不使用 AbstractButton.click()，因为它从 Qt 6.8 才有；Steam Link 和部分
        // Linux 构建仍使用更早的 Qt。
        MouseArea {
            id: pointerArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.PointingHandCursor

            property real pressStartX: 0
            property real dragPosition: control.visualPosition
            property real dragOffset: 0
            property bool dragging: false

            onPressed: function(mouse) {
                pressStartX = mouse.x
                dragPosition = control.visualPosition
                var travel = width - handle.width - 6
                var handleCenter = 3 + handle.width / 2 + dragPosition * travel
                dragOffset = mouse.x - handleCenter
                dragging = false
                control.forceActiveFocus(Qt.MouseFocusReason)
            }
            onPositionChanged: function(mouse) {
                if (!pressed) {
                    return
                }
                if (Math.abs(mouse.x - pressStartX) >= 4) {
                    dragging = true
                }
                if (dragging) {
                    var travel = width - handle.width - 6
                    dragPosition = Math.max(0, Math.min(1,
                        (mouse.x - dragOffset - 3 - handle.width / 2) / travel))
                }
            }
            onReleased: function() {
                if (dragging) {
                    // 拖到哪一侧就是哪一侧：只有真的换了边才提交，
                    // 拖回原位松手等于什么都没做。
                    var targetChecked = dragPosition >= 0.5
                    if (targetChecked !== control.checked) {
                        control.commitPointerToggle()
                    }
                } else {
                    control.commitPointerToggle()
                }
                dragging = false
            }
            onCanceled: dragging = false
        }
    }
}
