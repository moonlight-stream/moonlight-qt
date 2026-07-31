import QtQuick

import "."

// 不定长进度条：斜条纹推进。
//
// 一整槽等距斜条从左往右匀速推。之前试过来回晃的滑块（没有方向感，读起来像卡住了
// 在反复重试）和分段方块（太像电量格），斜条是这几个里唯一一眼就是「在走」的。
//
// 刻意做得克制：细条、半透明、只有一条 1px 基线兜底，不套边框也不用满色。
// 试过满色粗斜条（工业危险胶带那种），气质是对的，但味太重 —— 一条 620px 宽的
// 满色酸性绿胶带会把整个加载页的注意力全吸走，而加载只是个过渡态。
//
// 斜条不是转出来的，是错切出来的：Matrix4x4 把 x 按 y 线性偏移，得到严格的
// 平行四边形 —— 旋转会让两端出现斜切的尖角，还得额外补偿宽度。
Item {
    id: root

    property color barColor: Theme.acid
    property color trackColor: Theme.line

    // 停下来时斜条留在原地转暗，像一台没通电的仪表 —— 比整条消失更说明状态
    // （主机搜索页关掉 mDNS 时就是这个状态）
    property bool running: true

    property int stripeWidth: 10
    property int stripeGap: 8
    // 斜度：顶边相对底边的水平偏移量 = slant * height
    property real slant: 0.9

    readonly property int pitch: stripeWidth + stripeGap

    implicitHeight: 12

    // 底槽
    Rectangle {
        anchors.fill: parent
        color: root.trackColor
        opacity: 0.35
    }

    Item {
        id: viewport

        anchors.fill: parent
        clip: true

        // 相位只跑一个节距就归零。斜条等距，跑完一个节距的画面和起点完全一致，
        // 所以循环点看不出来 —— 不需要让它跑完全程。
        property real phase: 0

        NumberAnimation on phase {
            running: root.running && root.visible
            loops: Animation.Infinite
            from: 0
            to: root.pitch
            duration: 520
        }

        Repeater {
            // 两侧各多铺一根：错切之后顶边会往右探出 slant * height，左右都要有条子
            // 顶上，否则推进时两端会露出空槽。
            //
            // 这里必须写 root.width / root.height：Repeater 自己也是个 Item，而且是
            // 零尺寸的，在 model 里裸写 width 拿到的是它自己的 0，结果只会铺出两根条子。
            model: Math.ceil((root.width + root.slant * root.height) / root.pitch) + 2

            Rectangle {
                x: (index - 1) * root.pitch + viewport.phase
                width: root.stripeWidth
                height: viewport.height
                color: root.running ? root.barColor : root.trackColor
                opacity: root.running ? 0.9 : 0.45

                transform: Matrix4x4 {
                    // x' = x + slant * (height - y)：底边不动，顶边右移，
                    // 斜条向右上倾斜，和推进方向一致。
                    matrix: Qt.matrix4x4(1, -root.slant, 0, root.slant * viewport.height,
                                         0,  1,          0, 0,
                                         0,  0,          1, 0,
                                         0,  0,          0, 1)
                }
            }
        }
    }

    // 基线：读条停着的时候也要有东西告诉你这里是一条轨道
    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: 1
        color: root.trackColor
    }
}
