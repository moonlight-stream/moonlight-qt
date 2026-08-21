import QtQuick 2.9
import "."
import ".."
import "../theme"

// 枚举选择行。恢复索引和用户写回严格分离：selectedValue 变化只更新
// currentIndex，只有下拉框的 activated 信号才向外发送 valueActivated。
SettingsRow {
    id: row

    property alias model: control.model
    property alias currentIndex: control.currentIndex
    property string valueRole: "val"
    property var selectedValue
    property bool controlEnabled: true
    property int maximumControlWidth: 320

    signal valueActivated(var value)

    function syncSelection() {
        if (!control.model || control.model.count === undefined) {
            return
        }

        for (var i = 0; i < control.model.count; i++) {
            if (control.model.get(i)[valueRole] === selectedValue) {
                control.currentIndex = i
                return
            }
        }

        control.currentIndex = control.model.count > 0 ? 0 : -1
    }

    onSelectedValueChanged: syncSelection()

    AutoResizingComboBox {
        id: control
        maximumWidth: Math.min(row.maximumControlWidth,
                               Math.max(120, row.width - Theme.spaceMd * 2))
        textRole: "text"
        enabled: row.controlEnabled
        hoverEnabled: true

        Component.onCompleted: row.syncSelection()

        onActivated: {
            if (currentIndex >= 0) {
                row.valueActivated(model.get(currentIndex)[row.valueRole])
            }
        }
    }
}
