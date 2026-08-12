#pragma once

#include <QByteArray>

// 主机推来的光标只有裸 BGRA 位图和一个不透明的 shapeId，协议里没有"这是箭头还是
// I 型"的语义。为了在本地能换成系统原生光标（风格跟客户端一致、Retina 下天然清晰），
// 我们只能从位图本身把标准形状认出来。
//
// 识别用的是尺度无关的几何特征——包围盒长宽比、填充率、热点在包围盒里的相对位置、
// 各轴对称性、行宽剖面——而不是逐像素指纹。主机的光标会随 Windows 主题和 DPI 缩放
// 在 32/48/64 像素几档之间变化，指纹匹配在这几档之间不成立，几何特征则基本恒定。
//
// 只认几何签名足够分得开的那几种：箭头、"箭头 + 转圈"、转圈、I 型、手型、上下/左右/
// 两个对角的缩放箭头。十字（IDC_CROSS）和四向箭头（IDC_SIZEALL）都是"近方形 + 四重
// 对称 + 细线"，几何上挤在一起分不干净，故意不认，让它们走位图。判不准就返回
// Unknown，调用方回退到画主机位图——宁可回退，不要给错形状。

enum class NativeCursorShape {
    Unknown,
    Arrow,
    // IDC_APPSTARTING：箭头右上角挂个转圈。macOS 有对应的原生货（HIServices 里的
    // busybutclickable），SDL 的 SDL_SYSTEM_CURSOR_WAITARROW 就是它。
    AppStarting,
    // IDC_WAIT：只有一个转圈，没有箭头
    Wait,
    IBeam,
    Hand,
    SizeWE,
    SizeNS,
    SizeNWSE,
    SizeNESW,
};

// 分类用到的中间量。单独暴露出来是为了能在单测里断言，以及在流里按日志打出来——
// 现场遇到误判时，照着日志调阈值比重新猜一遍要快。
struct CursorShapeMetrics {
    bool valid = false;

    // 不透明像素的包围盒尺寸
    int boxWidth = 0;
    int boxHeight = 0;
    // 包围盒内不透明像素占比
    qreal fill = 0.0;
    // 热点在包围盒内的归一化坐标，[0,1]；落在盒外时会被裁到边界
    qreal hotspotX = 0.0;
    qreal hotspotY = 0.0;
    // 对称度，[0,1]：竖轴镜像、横轴镜像、绕中心旋转 180°
    qreal symV = 0.0;
    qreal symH = 0.0;
    qreal symRot180 = 0.0;
    // 不透明像素坐标的皮尔逊相关系数，[-1,1]。沿左上-右下方向的形状为正，
    // 右上-左下为负，十字/方块/四向箭头这类无方向性的接近 0。
    //
    // 注意不能用"对某条对角线转置是否不变"来区分两个对角光标：绕任一条对角线
    // 做镜像，两条对角线都映射回自身，两者的转置对称度都是 1。方向性只能靠
    // 相关系数这种带符号的量来测。
    qreal diagonalCorrelation = 0.0;
    // 包围盒正中那块（各边取 1/4 宽高）的不透明占比。用来分辨"中空"和"中心是实的"：
    // 圆环（等待光标）中间是空的，而四向箭头和十字的臂在中心交汇，是实的。
    qreal centerFill = 0.0;
    // 顶部 15% 的平均行宽 / 中位行宽
    qreal topWidthRatio = 0.0;
    // 第一行行宽 / 最大行宽。I 型的顶端就是最宽的那道衬线（≈1），而上下双箭头的
    // 顶端是尖的、最宽处在箭头根部（明显 <1）。这两种都是细长 + 左右对称 + 热点
    // 居中，全靠这一项分开。手型和箭头的顶端也都是尖的。
    qreal firstRowRatio = 0.0;
    // 最宽那一行的位置，归一化到 [0,1]。手型是"食指压在拳头上"，最宽处在下半部分；
    // I 型最宽处在第 0 行，上下双箭头在靠近顶端的箭头根部。
    qreal widestRowPosition = 0.0;
    // 上半部分行宽出现"变窄"的次数。箭头的三角头部应该接近单调变宽
    int topMonotoneViolations = 0;
};

CursorShapeMetrics measureCursorShape(int width,
                                      int height,
                                      int hotspotX,
                                      int hotspotY,
                                      const QByteArray& bgra);

NativeCursorShape classifyCursorShape(const CursorShapeMetrics& metrics);

// bgra 为紧打包的 BGRA8888，长度须等于 width * height * 4
NativeCursorShape classifyCursorShape(int width,
                                      int height,
                                      int hotspotX,
                                      int hotspotY,
                                      const QByteArray& bgra);

const char* nativeCursorShapeName(NativeCursorShape shape);
