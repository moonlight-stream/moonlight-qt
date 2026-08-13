import QtQuick 2.9
import "."

// 统一的焦点环：方角、2px、accent，套在目标外面 3px。
//
// 为什么要自己画一个：FluentWinUI3 会在获得焦点的控件外面挂一圈自己的
// focus frame（`FluentWinUI3/impl/FocusFrame.qml`）—— 3px 外环 + 1px 内环、
// 圆角半径 7、深色主题下外白内黑。那圈白色圆角双环在我们这套方角硬投影里
// 非常突兀，而且它自己就是「各个控件的焦点框长得都不一样」的主要来源。
//
// 关掉它的办法是在派生组件上把 style 用来定位的钩子属性影子覆盖成 null：
//
//     readonly property Item __focusFrameTarget: null
//
// 这不是我们发明的偏方，Qt 自己的 `FluentWinUI3/SearchField.qml:47` 就是这么
// 干的。关掉之后焦点表达全部由我们自己给：
//
//   - 本身就有一圈边框可用的控件（Button / ToolButton / ComboBox / TextField）：
//     边框直接转 2px accent，hover 维持 1px lineStrong，这样 hover 和 focus
//     在颜色和粗细两个维度上都能区分开。
//   - 边框已经被状态占用的小控件（CheckBox / Switch 勾选态是 accent 填充 +
//     accent 描边，Slider 把手同理）：边框腾不出来表达焦点，改用这个外挂环。
Rectangle {
    // 环相对目标向外扩出的距离
    property int inset: 3

    anchors.fill: parent
    anchors.margins: -inset

    z: 10
    radius: 0
    color: "transparent"
    border.width: 2
    border.color: Theme.accent

    // 紧贴目标边缘的 1px 暗色分隔。勾选态的 CheckBox / 打开的 Switch / 选中的
    // DisplayChip 本身就是 accent（或 acid）实心填充，accent 环直接贴上去几乎读不出来；
    // 垫一条 ink 把两者分开，环在任何填充色上都是清晰的一圈。
    Rectangle {
        anchors.fill: parent
        anchors.margins: parent.border.width
        radius: 0
        color: "transparent"
        border.width: 1
        border.color: Theme.ink
    }
}
