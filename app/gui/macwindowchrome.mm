#include "macwindowchrome.h"

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

namespace
{

// 红绿灯距窗口左边的留白。main.qml 里工具栏的 windowButtonInsetLeft 是按
// 「这个值 + 整组 60pt 宽 + 一格间距」算出来的，两边要一起改。
const CGFloat kButtonLeftMargin = 20;
const void* kTitleBarObserverTokensKey = &kTitleBarObserverTokensKey;

void removeTitleBarObservers(NSWindow* window)
{
    NSArray* observerTokens = objc_getAssociatedObject(window, kTitleBarObserverTokensKey);
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    for (id token in observerTokens) {
        [center removeObserver:token];
    }
    objc_setAssociatedObject(window, kTitleBarObserverTokensKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

// 把系统标题栏那条带子拉高到 barHeight，并让红绿灯在新高度里垂直居中。
//
// 这里动的是 standardWindowButton 的 superview 链：
//   NSButton（红绿灯） → NSTitlebarView → NSTitlebarContainerView → 窗口的 frame view
// 这套层级从 10.10 一直稳定到现在，是各家做「高标题栏」的通用做法，但毕竟不是公开
// API，所以每一步都做了判空，拿不到就原样返回、什么都不改（最坏情况是红绿灯留在
// 最上面那条，功能不受影响）。
void applyTallTitleBar(NSWindow* window, CGFloat barHeight)
{
    if (window == nil) {
        return;
    }

    NSButton* buttons[] = {
        [window standardWindowButton:NSWindowCloseButton],
        [window standardWindowButton:NSWindowMiniaturizeButton],
        [window standardWindowButton:NSWindowZoomButton],
    };

    NSView* titleBarView = buttons[0].superview;
    if (titleBarView == nil) {
        return;
    }

    NSView* container = titleBarView.superview;
    if (container == nil) {
        return;
    }

    // 带子贴着窗口顶边，所以拉高的同时要把原点往下挪同样的量
    NSRect containerFrame = container.frame;
    if (containerFrame.size.height < barHeight) {
        CGFloat delta = barHeight - containerFrame.size.height;
        containerFrame.size.height = barHeight;
        containerFrame.origin.y -= delta;
        container.frame = containerFrame;
    }

    // 里面那层 NSTitlebarView 也铺满容器。实测只拉容器就已经能把按钮居中了，
    // 但两层高度对不上时 AppKit 在窗口尺寸变化后重新布局的结果不好预料，
    // 顺手对齐，省得留一个只在特定版本成立的巧合。
    NSRect titleBarFrame = titleBarView.frame;
    titleBarFrame.origin.y = 0;
    titleBarFrame.size.height = barHeight;
    titleBarView.frame = titleBarFrame;

    // 三颗按钮垂直居中，并整组右移到 kButtonLeftMargin。
    //
    // 系统默认把第一颗放在 x=9（实测三颗分别在 9 / 32 / 55，各 14×14，整组占 9..69）。
    // 那是给 28pt 高的窄标题栏配的边距，放到我们这条 56pt 的 bar 里显得贴边，
    // 所以整组往右挪，左右各留出一样的余量。
    //
    // NSView 默认不翻转，y 从下往上算。
    CGFloat shiftX = kButtonLeftMargin - buttons[0].frame.origin.x;

    for (NSButton* button : buttons) {
        if (button == nil) {
            continue;
        }

        NSRect buttonFrame = button.frame;
        buttonFrame.origin.x += shiftX;
        buttonFrame.origin.y = (barHeight - buttonFrame.size.height) / 2.0;
        button.frame = buttonFrame;
    }
}

} // namespace

void MacWindowChrome::useTallTitleBar(QWindow* window, int barHeight)
{
    if (window == nullptr) {
        return;
    }

    // Create the native window before accessing its AppKit view hierarchy.
    window->create();

    NSView* view = reinterpret_cast<NSView*>(window->winId());
    NSWindow* nsWindow = view.window;
    if (nsWindow == nil) {
        return;
    }

    applyTallTitleBar(nsWindow, barHeight);

    // AppKit resets the title-bar layout after resizing and full-screen
    // transitions, so reapply it at each of these points. Notification-center
    // block observers must be removed explicitly; keep their tokens associated
    // with this window and tear them down when it closes.
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    removeTitleBarObservers(nsWindow);

    NSMutableArray* observerTokens = [NSMutableArray array];
    NSArray<NSNotificationName>* names = @[
        NSWindowDidResizeNotification,
        NSWindowDidEndLiveResizeNotification,
        NSWindowDidEnterFullScreenNotification,
        NSWindowDidExitFullScreenNotification,
        NSWindowDidBecomeKeyNotification,
    ];

    for (NSNotificationName name in names) {
        id token = [center addObserverForName:name
                                      object:nsWindow
                                       queue:nil
                                  usingBlock:^(NSNotification* note) {
            applyTallTitleBar(static_cast<NSWindow*>(note.object), barHeight);
        }];
        [observerTokens addObject:token];
    }

    id closeToken = [center addObserverForName:NSWindowWillCloseNotification
                                        object:nsWindow
                                         queue:nil
                                    usingBlock:^(NSNotification* note) {
        removeTitleBarObservers(static_cast<NSWindow*>(note.object));
    }];
    [observerTokens addObject:closeToken];
    objc_setAssociatedObject(nsWindow,
                             kTitleBarObserverTokensKey,
                             observerTokens,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
