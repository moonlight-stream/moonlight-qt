import QtQuick 2.9
import QtQuick.Controls
import "."
import "../theme"

// 开关行。checked 不做双向绑定，由使用方在 onToggled 里写回偏好设置，
// 避免初始化阶段的写回把已保存的值覆盖掉。
SettingsRow {
    id: toggleRow

    property alias checked: control.checked
    property alias controlEnabled: control.enabled
    property alias tooltip: tip.text

    signal toggled(bool value)

    HardSwitch {
        id: control
        hoverEnabled: true
        // 只在用户操作时上报。checked 是别名，从偏好设置恢复初值也会改动它，
        // 用 onCheckedChanged 的话初始化阶段就会触发一次写回 —— 正是上面注释里
        // 说要避免的那件事。Switch 自带的 toggled() 只由用户交互触发。
        onToggled: toggleRow.toggled(checked)

        ToolTip {
            id: tip
            visible: text !== "" && control.hovered
            delay: 800
            timeout: 8000
        }
    }
}
