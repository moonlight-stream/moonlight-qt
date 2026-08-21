import QtQuick 2.9
import QtQuick.Controls
import "../theme"

// 左侧分类导航。窄窗口时 compact 置真，变成横向 tab 条。
//
// 焦点模型：每一项都在 Tab 焦点链里（设置页的手柄上下键发的就是 Tab / Shift+Tab），
// 所以「上下走分类」是白送的。移动焦点本身不切分类 —— 切分类要按 A（Space）或
// 朝内容区的方向键，这样浏览分类不会把右边的内容翻来翻去。
Item {
    id: rail

    property var categories: []
    property string currentCategory: ""
    property bool compact: false

    // 焦点是否落在分类栏里。ListView 本身是焦点域，所以它的 activeFocus
    // 在任意一个代理持焦时都为真。
    readonly property bool railFocused: list.activeFocus

    signal categoryPicked(string category)
    // 用户在分类栏里确认了选择，希望焦点进到右边的内容区
    signal contentRequested()

    function step(delta) {
        var index = indexOf(currentCategory)
        if (index < 0) {
            return
        }
        var next = index + delta
        if (next < 0) {
            next = categories.length - 1
        }
        else if (next >= categories.length) {
            next = 0
        }
        categoryPicked(categories[next].key)
    }

    function indexOf(key) {
        for (var i = 0; i < categories.length; i++) {
            if (categories[i].key === key) {
                return i
            }
        }
        return -1
    }

    function ensureCurrentVisible() {
        var index = indexOf(currentCategory)
        if (index < 0) {
            return
        }

        list.currentIndex = index
        list.positionViewAtIndex(index, ListView.Contain)
    }

    onCurrentCategoryChanged: Qt.callLater(ensureCurrentVisible)
    onCompactChanged: Qt.callLater(ensureCurrentVisible)
    onCategoriesChanged: Qt.callLater(ensureCurrentVisible)

    // 把焦点放到当前分类那一项上。进入设置页、以及从内容区按 B 退回来时用。
    function focusCurrent() {
        var index = indexOf(currentCategory)
        if (index < 0) {
            index = 0
        }
        list.currentIndex = index
        list.positionViewAtIndex(index, ListView.Contain)

        if (!focusIndex(index)) {
            // 刚进页面时代理还没排版出来，下一帧再试一次
            Qt.callLater(focusIndex, index)
        }
    }

    // 聚焦第 index 项，返回是否成功（代理可能还没实例化）
    function focusIndex(index) {
        var item = list.itemAtIndex(index)
        if (item) {
            item.forceActiveFocus(Qt.TabFocusReason)
        }
        return !!item
    }

    implicitHeight: compact ? 46 : 0

    ListView {
        id: list
        anchors.fill: parent
        orientation: rail.compact ? ListView.Horizontal : ListView.Vertical
        spacing: Theme.spaceXs
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: rail.categories

        onWidthChanged: {
            if (rail.compact) {
                Qt.callLater(rail.ensureCurrentVisible)
            }
        }

        // 方向键由代理自己处理（要区分横竖两种排布），ListView 别再抢一遍
        keyNavigationEnabled: false

        delegate: ItemDelegate {
            id: item

            readonly property bool current: modelData.key === rail.currentCategory

            // 关掉 FluentWinUI3 那圈白色圆角双环，焦点由下面背景的 2px accent
            // 描边表达。详见 theme/FocusRing.qml 的注释。
            readonly property Item __focusFrameTarget: null

            width: rail.compact ? Math.max(96, label.implicitWidth + Theme.spaceXl + Theme.spaceLg) : list.width
            height: rail.compact ? list.height : 44

            // 进焦点链。设置页的手柄上下键 = Tab / Shift+Tab，所以这一行就等于
            // 「上下能走到分类栏」。ItemDelegate 默认是 NoFocus，改成 StrongFocus
            // 之后 Tab 和鼠标点击都会把焦点带过来（Control 会顺带打开
            // activeFocusOnTab）。
            focusPolicy: Qt.StrongFocus

            onClicked: rail.categoryPicked(modelData.key)

            // 焦点落到哪一项，ListView 的 currentIndex 就跟到哪 —— 只是同步高亮，
            // 不切分类。
            onActiveFocusChanged: {
                if (activeFocus) {
                    list.currentIndex = index
                }
            }

            // 确认这一项并把焦点交给内容区
            function activate() {
                rail.categoryPicked(modelData.key)
                rail.contentRequested()
            }

            function moveFocus(forward) {
                nextItemInFocusChain(forward).forceActiveFocus(Qt.TabFocusReason)
            }

            Keys.onReturnPressed: activate()
            Keys.onEnterPressed: activate()
            // 手柄 A 在 UI 导航模式下发的是 Space
            Keys.onSpacePressed: activate()

            // 方向键按实际排布走：竖着的 rail 上下换项、右进内容；
            // 横着的 tab 条左右换项、下进内容。
            Keys.onUpPressed:    if (!rail.compact) moveFocus(false)
            Keys.onLeftPressed:  if (rail.compact)  moveFocus(false)
            Keys.onDownPressed:  rail.compact ? activate() : moveFocus(true)
            Keys.onRightPressed: rail.compact ? moveFocus(true) : activate()

            // 方角 + 当前项左侧一条 accent 粗条（横向 tab 条时改成底部一条）。
            // 原来靠一圈描边表示当前项，方角化之后描边和卡片边框太像，粗条读起来更明确。
            //
            // 「当前分类」和「焦点在哪」是两件事，必须分开表达：粗条 + 软填充表示
            // 当前分类，一圈亮描边表示焦点。只用键盘/手柄来的焦点才画描边
            // （visualFocus），鼠标点一下不该冒出个框。
            //
            // 描边用 2px accent，和全应用的焦点表达统一（以前这里是 1px 白，
            // 各处焦点框粗细和颜色都不一样，看着像三套设计）。
            background: Rectangle {
                radius: 0
                color: item.current ? Theme.accentSoft
                                    : (item.hovered || item.visualFocus ? Theme.surface2 : "transparent")
                border.width: item.visualFocus ? 2 : 0
                border.color: Theme.accent

                Rectangle {
                    anchors {
                        left: parent.left
                        top: parent.top
                        bottom: parent.bottom
                    }
                    width: (item.current && !rail.compact) ? Theme.accentBar : 0
                    visible: width > 0
                    color: Theme.accent
                }

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                    }
                    height: (item.current && rail.compact) ? Theme.accentBar : 0
                    visible: height > 0
                    color: Theme.accent
                }

                Behavior on color {
                    ColorAnimation { duration: Theme.durFast }
                }
            }

            contentItem: Row {
                id: label
                spacing: Theme.spaceSm
                leftPadding: Theme.spaceMd

                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    source: modelData.icon
                    // 按 2x 栅格化，Retina 上才不会糊
                    sourceSize.width: 18
                    sourceSize.height: 18
                    width: 18
                    height: 18
                    // 图标本身是白的，靠透明度区分选中与否
                    opacity: item.current ? 1.0 : 0.6

                    Behavior on opacity {
                        NumberAnimation { duration: Theme.durFast }
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.title
                    color: item.current ? Theme.text : Theme.textDim
                    font.family: Theme.fontSans
                    font.pointSize: Theme.fontRowTitle
                    font.weight: item.current ? Font.ExtraBold : Font.Medium
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: Theme.tracking(Theme.fontRowTitle, 0.06)
                    elide: Text.ElideRight
                }
            }
        }
    }
}
