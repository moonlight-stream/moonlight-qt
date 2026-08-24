pragma Singleton
import QtQuick 2.9

// 全应用的设计 token。风格是 neo-brutalism（新粗野主义）深色工业变体，
// 数值对齐参考站 https://client.cloud.procriva.com/ 的 --cend-* 变量。
//
// 配方只有六件事：方角、零模糊硬投影、极简中性色、单一荧光强调色、
// 几何变宽字体 + 等宽数字、宽字距大写微标签。想改风格只动这一个文件。
QtObject {
    // ---- 表面 ----
    // 一律不透明。壁纸只从卡片之间的缝隙里透出来，卡片本身要读起来像硬物件，
    // 不是叠在图上的一层玻璃 —— 这是和之前 glassmorphism 最根本的区别。
    readonly property color ink: "#0F1115"       // --cend-ink，最底层
    readonly property color surface: "#171A20"   // --cend-surface，卡片/面板
    readonly property color surface2: "#1F232B"  // --cend-surface-2，hover / 输入框
    readonly property color surfaceLayer: "#E0171A20"  // 菜单/卡片层，约 88% 不透明
    readonly property color surface2Layer: "#E01F232B" // 抬高一层的半透明表面
    readonly property color line: "#2B3038"      // --cend-border，1px 描边
    readonly property color lineStrong: "#3C434E" // 控件轮廓，比描边亮一档

    // ---- 文字 ----
    readonly property color text: "#EEF0EC"      // --cend-text，带一点暖调的灰白
    readonly property color textDim: "#AEB3AB"   // --cend-text-dim，副标题/说明
    readonly property color textFaint: "#7E858E" // 禁用态、极次要信息
    // 长说明比微标签更亮，避免在高 DPI 和复杂背景上退成一层灰雾。
    readonly property color textSettingsSubtitle: "#D7DBD5"

    // ---- 强调色 ----
    // 主色保留品牌青；酸性绿只留给「正在运行 / LIVE」这类抢眼状态标记，
    // 用得越少越有效，一旦到处都是就退化成普通的绿色装饰。
    readonly property color accent: "#39C5BB"
    readonly property color accentStrong: "#5DD9D0"
    readonly property color accentDim: "#2BA39A"
    readonly property color accentSoft: "#2639C5BB"  // 15% 主色，选中态填充
    readonly property color acid: "#C8FF4D"          // --cend-acid
    readonly property color acidGlow: "#66C8FF4D"    // --cend-glow-acid
    readonly property color danger: "#FF876F"        // --cend-coral

    // ---- 形状 ----
    // 全方角。参考站是 --cend-radius: 0 配上一堆 !important 去压 Naive UI 的圆角，
    // 我们这边对应的是逐控件覆盖 FluentWinUI3 的 background。
    readonly property int radiusCard: 0
    readonly property int radiusControl: 0

    // 零模糊、纯偏移的实心投影，对应 box-shadow: 6px 6px #0000008c。
    // QML 没有 box-shadow，Panel.qml 用一个偏移的实心矩形垫在本体后面模拟。
    readonly property int shadowOffset: 6
    readonly property int shadowOffsetLift: 9     // hover/focus 抬起时的投影
    readonly property color shadowColor: "#8C000000"

    // border-left 粗条，参考站用 5px/9px 做强调，我们卡片用 4、状态标记用 5
    readonly property int accentBar: 4
    readonly property int accentBarStrong: 5

    // ---- 字体 ----
    // 打包在 app/res/fonts/，由 main.cpp 注册；中文靠系统字体回退。
    readonly property string fontSans: "Manrope"
    readonly property string fontMono: "DM Mono"

    // QML 的 font.letterSpacing 单位是像素，不是 em，所以要按字号折算。
    // 参考站的宽字距微标签是 .18em~.24em，标题是 -.03em。
    function tracking(pointSize, em) { return pointSize * 1.333 * em }
    function trackingWide(pointSize) { return tracking(pointSize, 0.2) }
    function trackingTight(pointSize) { return tracking(pointSize, -0.03) }
    // 常用字号的预折算值，绑定里直接用，省得到处调函数
    readonly property real trackingCaption: trackingWide(fontCaption)
    readonly property real trackingLabel: trackingWide(fontBody)

    // ---- 8pt 间距栅格 ----
    readonly property int spaceXs: 4
    readonly property int spaceSm: 8
    readonly property int spaceMd: 12
    readonly property int spaceLg: 16
    readonly property int spaceXl: 24

    // ---- 字号 ----
    readonly property int fontCardTitle: 13
    readonly property int fontHeroTitle: 18
    readonly property int fontRowTitle: 11
    readonly property int fontBody: 10
    readonly property int fontCaption: 9
    readonly property int fontSettingsSubtitle: fontBody

    // ---- 动效 ----
    // neo-brutalism 的动效更短更机械：不要弹、不要缓慢收尾，OutQuad 就够了。
    readonly property int durFast: 120
    readonly property int durNormal: 150
    readonly property int easing: Easing.OutQuad

    // ---- 布局 ----
    readonly property int railWidth: 208
    // 低于这个宽度时 rail 塌缩成顶部横向 tab
    readonly property int compactBreakpoint: 860
    // 低于这个内容宽度时，设置行把右侧控件移到说明文字下方。
    readonly property int settingsRowStackBreakpoint: 520
}
