import QtQuick 2.9
// Overlay（下拉面板算可用高度时要用）需要 QtQuick.Controls 2.3 以上
import QtQuick.Controls
import QtQuick.Window 2.2

import SdlGamepadKeyNavigation 1.0

import "theme"

// https://stackoverflow.com/questions/45029968/how-do-i-set-the-combobox-width-to-fit-the-largest-item
ComboBox {
    id: control

    property int textWidth

    // 每一项文字之外还要留出：控件自身的左右 padding、下拉箭头，以及 contentItem
    // （FluentWinUI3 下是个 TextField）自己的左右 padding。最后这项以前没算，
    // 于是像「窗口化」这种刚好卡在边界上的选项会被截断成「窗[」。
    // 箭头这里一律预留：FluentWinUI3 只在 indicator.visible 为真时才把它折进
    // rightPadding，而这个状态在绑定求值过程中会反复翻转，读到的值不可靠。
    // 宁可多留十几像素，也不要把文字裁掉。
    property int desiredWidth: textWidth + leftPadding + rightPadding + indicator.width
                               + (contentItem ? contentItem.leftPadding + contentItem.rightPadding : 0)
    property int maximumWidth : parent.width

    implicitWidth: desiredWidth < maximumWidth ? desiredWidth : maximumWidth

    // 以前只在 onActivated 里量宽度，靠每个调用点在 Component.onCompleted 里补一次
    // activated()。设置页改成按分类切换之后，非当前分类的控件在创建时 visible 为假，
    // 有几处调用点会直接 return，宽度就永远停在 0。这里自己盯住模型和字体，
    // 调用点不用再记得手动触发。
    onCountChanged: recalculateWidth()
    onFontChanged: recalculateWidth()

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

    // 收起状态下上下键的去向。
    //
    // ComboBox 默认拿上下键换值，于是弹窗里的下拉会变成焦点陷阱（走不出去），
    // 而且一路上还把值静默改了。给任一方向设了目标，这个下拉的上下键就整体改为
    // 导航语义：有目标就移动焦点，没目标（到头了）就吃掉按键。
    //
    // 两个都不设则保持 ComboBox 默认的换值行为 —— 设置页里手柄的上下是
    // Tab / Shift+Tab，压根到不了这里，键盘用户的上下换值不该被一起砍掉。
    property Item navUpItem: null
    property Item navDownItem: null

    readonly property bool arrowNavigation: navUpItem !== null || navDownItem !== null

    // 展开状态下上下归列表（accepted = false 放行给 ComboBox 自己的键处理）
    Keys.onUpPressed: function(event) {
        if (popup.opened || !arrowNavigation) {
            event.accepted = false
            return
        }
        if (navUpItem) {
            navUpItem.forceActiveFocus(Qt.TabFocusReason)
        }
    }

    Keys.onDownPressed: function(event) {
        if (popup.opened || !arrowNavigation) {
            event.accepted = false
            return
        }
        if (navDownItem) {
            navDownItem.forceActiveFocus(Qt.TabFocusReason)
        }
    }

    // 左右键不再直接换选项。
    //
    // 以前左右就是「改值」，于是手柄用户在设置页里横着扫一下就把分辨率、编解码器
    // 这类关键项静默改掉了（issue #144 的另一半）。现在下拉统一走「A 打开列表 →
    // 上下选 → A 确认 / B 取消」：改值必须是一次明确的操作。
    // 键盘用户不受影响，ComboBox 自带的上下键换值还在。
    //
    // 展开状态下左右在纵向列表里没有意义，吃掉；收起状态下放行给上层做焦点移动
    // （QML 的按键处理器默认 accepted = true，不显式放行就成了死键）。
    Keys.onLeftPressed: function(event) {
        event.accepted = popup.opened
    }

    Keys.onRightPressed: function(event) {
        event.accepted = popup.opened
    }

    // 方角化。FluentWinUI3 的圆角来自它自己 __config 里的背景，只能整块替掉。
    // 这一处改动覆盖设置页全部 16 个下拉。
    //
    // 千万别顺手改 contentItem 或 indicator：上面 desiredWidth 的算式读的是
    // 它们的 padding 和宽度，动了它们测宽就会跟着错。
    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 34

        radius: 0
        color: control.pressed ? Theme.surface : Theme.surface2
        border.width: 1
        border.color: control.visualFocus || control.hovered ? Theme.accent : Theme.lineStrong

        Behavior on border.color {
            ColorAnimation { duration: Theme.durFast }
        }
    }

    // 下拉项的行高。面板高度直接按 count * 这个值算，不去读 ListView 的
    // contentHeight —— 那个值在 aboutToShow 的时刻还没排完版（代理是按需创建的），
    // 读到的是半成品，面板就会开得比内容矮，于是三项的列表也能挤出滚动条。
    readonly property int popupItemHeight: 34

    // 展开的面板。不覆盖的话是 FluentWinUI3 自己那一套：圆角、半透明灰渐变底、
    // 项也是圆角高亮块，选中项左边还有一颗胶囊小点 —— 和方角硬投影完全不搭。
    //
    // 面板本体用 Panel（方角 + 1px 描边 + 硬投影），描边走 accent：它是从控件里
    // 「弹出来」的东西，描边亮一档才分得清它压在页面上面。
    delegate: ItemDelegate {
        id: comboItem

        width: ListView.view ? ListView.view.width : control.width
        height: control.popupItemHeight
        highlighted: control.highlightedIndex === index

        // 交给 ComboBox 自己解析 textRole。这样 JS 数组、QVariantList 和 ListModel
        // 都走 Qt 的同一条取值路径，不依赖它们在 QML 里的运行时包装类型。
        readonly property string itemText: control.textAt(index)

        contentItem: Text {
            leftPadding: Theme.spaceSm
            rightPadding: Theme.spaceSm
            text: comboItem.itemText
            // 字体跟控件走：上面 recalculateWidth() 是按 control.font / popup.font 量的宽度，
            // 这里自己换字号就会量不准，长选项会被面板裁掉。
            font: control.font
            color: comboItem.highlighted ? Theme.text : Theme.textDim
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 0
            color: comboItem.highlighted ? Theme.surface2 : "transparent"

            // 当前值左边一条 accent 粗条。原来那颗圆点是 Fluent 的语言，
            // 这套风格里「当前项」一律用粗条表示（和设置页分类栏一致）。
            Rectangle {
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                width: control.currentIndex === index ? Theme.accentBar : 0
                visible: width > 0
                color: Theme.accent
            }
        }
    }

    popup: Popup {
        id: comboPopup

        // 面板允许的最大高度。每次弹出时按控件当前位置重算：设置页是能滚动的，
        // 写成绑定的话滚动之后不会重新求值，会拿着旧坐标算。
        property real maxHeight: 320
        // 内容想要的高度。按项数算，和排版进度无关；再和 ListView 实际的
        // contentHeight 取大值兜底，万一某个代理比 popupItemHeight 高也不会被切。
        readonly property real wantedHeight:
            Math.max(control.count * control.popupItemHeight,
                     contentItem ? contentItem.contentHeight : 0) + topPadding + bottomPadding

        y: control.height
        width: control.width
        implicitHeight: Math.min(wantedHeight, maxHeight)

        // FluentWinUI3 的 Popup 会从它自己的 __config 里逐个设置 topPadding /
        // bottomPadding / inset，而单项赋值优先于分组的 padding —— 只写 padding: 1
        // 压不住，内容区就比面板矮一截，看着像少了一项。四个方向逐个写死，
        // inset 也归零，否则 Panel 背景和面板本体对不齐。
        topPadding: 1
        bottomPadding: 1
        leftPadding: 1
        rightPadding: 1
        topInset: 0
        bottomInset: 0
        leftInset: 0
        rightInset: 0

        // 展开期间挂起 UI 导航模式，让上下键回到真正的方向键，在列表里逐项走
        // （UI 导航模式下上下发的是 Tab / Shift+Tab，驱动不了下拉列表）。
        //
        // 用挂起计数而不是「存旧值 - 还原旧值」：PcView 和 AppView 里也有下拉，
        // 那两页本来跑在普通模式，早先这里是硬置为 true，开合一次下拉就把它们的
        // 上下键永久变成 Tab，网格导航直接废掉。而存旧值同样不安全 —— 收起和
        // SettingsView 切页时的 setUiNavMode 谁先谁后不确定，还原会把页面刚设好的
        // 模式覆盖回去。计数只表达「我要临时借用方向键」，和页面级的开关正交。
        onAboutToShow: {
            SdlGamepadKeyNavigation.suspendUiNavMode()

            // 坐标和窗口高度都通过 Overlay 拿。别用 control.Window.height：
            // 附加属性没法这样从 JS 上取，那句会抛 TypeError，整个处理函数直接中断，
            // maxHeight 停在默认值上 —— 这就是之前三项的列表也开得很矮的原因。
            var overlay = Overlay.overlay
            var winHeight = overlay ? overlay.height : 720
            var scenePos = control.mapToItem(overlay, 0, 0)

            var below = winHeight - scenePos.y - control.height - Theme.spaceSm
            var above = scenePos.y - Theme.spaceSm

            // 下面装得下就往下开；装不下而上面更宽敞就整块往上翻。
            // 只有两边都不够装（语言、分辨率那种几十项的列表）才会真的出滚动条。
            if (wantedHeight <= below || below >= above) {
                maxHeight = Math.max(below, control.popupItemHeight * 3)
                y = control.height
            }
            else {
                maxHeight = Math.max(above, control.popupItemHeight * 3)
                y = -Math.min(wantedHeight, maxHeight)
            }
        }

        onAboutToHide: {
            SdlGamepadKeyNavigation.resumeUiNavMode()
        }

        // 面板自带硬投影，右下会溢出一点，这是刻意的：投影必须落在页面上才成立
        background: Panel {
            fill: Theme.surfaceLayer
            borderColor: Theme.accent
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
