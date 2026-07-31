import QtQuick 2.9
import QtQuick.Controls
import "."

// 方角分组框：标题在面板上方（宽字距大写强调色），下面是 surface 面板
// + 1px 描边 + 硬投影 + 左侧粗条。
//
// 标题保持 GroupBox 默认的「在框线之上」布局，不硬塞进面板里 ——
// GroupBox 自己管 label 的 y，跟它抢反而会在不同样式下错位；而且「小节标签在
// 硬边面板上方」本来就是参考站的排版方式。
// title 走 StyledText：旧设置页的 title 里带 <b><font color="#39C5BB"> 这类标签。
GroupBox {
    id: control

    topPadding: labelText.implicitHeight + Theme.spaceLg
    leftPadding: Theme.spaceLg
    rightPadding: Theme.spaceLg
    bottomPadding: Theme.spaceLg

    label: Text {
        id: labelText

        x: control.leftPadding
        width: control.availableWidth
        text: control.title
        textFormat: Text.StyledText
        color: Theme.accent
        // 字族和字号跟着控件自己的 font 走：旧设置页会把 groupBoxTitleFont 设成
        // 「幼圆」来给中文标题换脸，写死 Theme.fontSans 就把那个功能弄丢了。
        font.family: control.font.family
        font.pointSize: control.font.pointSize
        font.bold: true
        font.capitalization: Font.AllUppercase
        font.letterSpacing: Theme.tracking(control.font.pointSize, 0.08)
        elide: Text.ElideRight
    }

    background: Panel {
        fill: Theme.surfaceLayer
        y: control.topPadding - control.bottomPadding
        width: control.width
        height: control.height - control.topPadding + control.bottomPadding
        accentBarColor: Theme.accent
        accentBarWidth: Theme.accentBar
    }
}
