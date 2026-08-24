import QtQuick 2.9
import QtQuick.Controls
import "."

// 方角硬投影按钮。只覆盖 background（FluentWinUI3 的圆角是从它自己的
// __config 背景来的），contentItem 交给基类，图标和文字的排布不受影响。
Button {
    id: control

    property bool primary: false
    // default / link。primary 保留为现有主按钮开关，并优先于 variant。
    property string variant: "default"
    readonly property bool linkVariant: !primary && variant === "link"

    font.family: Theme.fontSans
    font.bold: true
    palette.buttonText: primary ? Theme.ink : (linkVariant ? Theme.linkText : Theme.text)

    // 关掉 FluentWinUI3 那圈白色圆角双环，焦点由下面的 2px accent 边框表达。
    // 详见 FocusRing.qml 的注释。
    readonly property Item __focusFrameTarget: null

    background: Panel {
        implicitWidth: 96
        implicitHeight: 34

        fill: control.primary
              ? (control.down ? Theme.accentDim
                              : (control.hovered ? Theme.accentStrong : Theme.accent))
              : control.linkVariant
                ? (control.down ? Theme.linkSurfacePressed
                                : (control.hovered ? Theme.linkSurfaceHover : Theme.linkSurface))
              : (control.down ? Theme.accentDim
                              : (control.hovered ? Theme.surface2 : Theme.surface))
        // hover / 按下是 1px accent，focus 是 2px accent —— 靠粗细区分，
        // 免得「鼠标停在上面」和「焦点在这里」看起来一模一样。
        borderColor: control.primary
                     ? Theme.ink
                     : control.linkVariant
                       ? Theme.linkBorder
                     : (control.down || control.hovered || control.visualFocus
                        ? Theme.accent : Theme.lineStrong)
        borderWidth: control.visualFocus ? 2 : 1
        // 按钮比卡片小，投影跟着收一档，否则一堆小按钮会糊成黑块
        shadowDepth: control.down ? 2 : (control.hovered || control.visualFocus ? 6 : 4)
        liftShift: control.down ? 1 : 0
        opacity: control.enabled ? 1.0 : 0.45
    }
}
