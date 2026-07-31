import QtQuick 2.0
import QtQuick.Controls

import "theme"

Menu {
    id: control

    property var initiator

    padding: Theme.spaceXs

    // 方角 + 1px 描边 + 硬投影。菜单不用左侧粗条：它本身已经是一块小硬板，
    // 再加粗条会和菜单项的 hover 抢注意力。
    background: Panel {
        implicitWidth: 200
        fill: Theme.surfaceLayer
    }

    onOpened: {
        // If the initiating object currently has keyboard focus,
        // give focus to the first visible and enabled menu item
        if (initiator && initiator.focus) {
            for (var i = 0; i < count; i++) {
                var item = itemAt(i)
                if (item.visible && item.enabled) {
                    item.forceActiveFocus(Qt.TabFocusReason)
                    break
                }
            }
        }
    }
}
