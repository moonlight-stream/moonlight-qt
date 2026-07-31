#pragma once

#include <QWindow>

// macOS 专用的窗口外壳调整。
//
// 主界面用 Qt.ExpandedClientAreaHint + Qt.NoTitleBarBackgroundHint 去掉了系统
// 标题栏的底色，内容顶到窗口最上沿。但系统那条标题栏带子（NSTitlebarContainerView）
// 还在，高度固定 28~32pt：红绿灯挤在最上面一小条里，而且 AppKit 只在那条带子里
// 提供窗口拖动和双击缩放，我们那条 56px 的 bar 下半部分是拖不动的。
//
// 这里把那条带子拉高到和我们的 bar 一样，并把三颗按钮在新高度里垂直居中。
// 于是红绿灯落到 bar 的中线上，AppKit 的拖动 / 双击缩放也自然覆盖整条 bar ——
// 不需要自己去接 startSystemMove()。
namespace MacWindowChrome
{
    // barHeight 要和 main.qml 里那条 ToolBar 的 height 保持一致。
    // AppKit 会在窗口尺寸变化、进出全屏时把带子的布局改回去，所以内部装了通知
    // 观察者自动重新应用，调用方只需要在窗口创建后调一次。
    void useTallTitleBar(QWindow* window, int barHeight);
}
