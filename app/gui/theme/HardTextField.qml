import QtQuick 2.9
import QtQuick.Controls

import "."

// 方角输入框。FluentWinUI3 的 TextField 背景是圆角 + 底部一条粗下划线，
// 换成 1px 方框，聚焦时描边转 accent。
//
// 里面填的基本都是数字和 IP，所以正文走等宽字体 —— 等宽 + tabular-nums 是这套风格
// 表达「这是数据」的方式，边输边跳的比例字宽在这里读起来很脏。
TextField {
    id: control

    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectByMouse: true

    font.family: Theme.fontMono
    font.pointSize: Theme.fontBody

    leftPadding: Theme.spaceSm
    rightPadding: Theme.spaceSm
    topPadding: Theme.spaceXs
    bottomPadding: Theme.spaceXs

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 32

        radius: 0
        color: Theme.ink
        border.width: 1
        border.color: control.activeFocus ? Theme.accent
                                          : (control.hovered ? Theme.lineStrong : Theme.line)

        Behavior on border.color {
            ColorAnimation { duration: Theme.durFast }
        }
    }
}
