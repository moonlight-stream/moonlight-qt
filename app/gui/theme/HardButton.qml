import QtQuick 2.9
import QtQuick.Controls
import "."

// 方角硬投影按钮。只覆盖 background（FluentWinUI3 的圆角是从它自己的
// __config 背景来的），contentItem 交给基类，图标和文字的排布不受影响。
Button {
    id: control

    font.family: Theme.fontSans
    font.bold: true

    background: Panel {
        implicitWidth: 96
        implicitHeight: 34

        fill: control.down ? Theme.accentDim
                           : (control.hovered ? Theme.surface2 : Theme.surface)
        borderColor: control.down || control.hovered || control.visualFocus
                     ? Theme.accent : Theme.lineStrong
        // 按钮比卡片小，投影跟着收一档，否则一堆小按钮会糊成黑块
        shadowDepth: control.down ? 2 : (control.hovered || control.visualFocus ? 6 : 4)
        liftShift: control.down ? 1 : 0
        opacity: control.enabled ? 1.0 : 0.45
    }
}
