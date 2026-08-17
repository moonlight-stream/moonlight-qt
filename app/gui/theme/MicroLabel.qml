import QtQuick 2.9
import "."

// 宽字距大写微标签。等宽字体 + 0.2em 字距 + 次级文字颜色，参考站里的
// letter-spacing: .18em~.24em 的小标签就是这个东西 —— 风格的另一半靠它，
// 没有这些标签只有方角和投影，看起来只是「没做圆角」而不是刻意的工业感。
Text {
    id: root

    // 数字用等宽表格数字，避免跳动
    font.family: Theme.fontMono
    font.pointSize: Theme.fontCaption
    font.capitalization: Font.AllUppercase
    font.letterSpacing: Theme.trackingCaption
    color: Theme.textDim
    elide: Text.ElideRight
    verticalAlignment: Text.AlignVCenter
}
