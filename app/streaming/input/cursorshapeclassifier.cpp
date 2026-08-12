#include "cursorshapeclassifier.h"

#include <QVector>

#include <algorithm>
#include <cmath>

namespace {

// alpha 高于这个值算不透明。抗锯齿的边缘像素 alpha 很低，一并当透明处理，
// 免得包围盒被一圈几乎看不见的羽化撑大。
constexpr int OpaqueAlphaThreshold = 32;

// 正常的光标不会比这更大。超过就当成不认识，走位图。
constexpr int MaxCursorDimension = 128;

qreal medianOf(QVector<int> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    const int mid = values.size() / 2;
    if (values.size() % 2 != 0) {
        return values[mid];
    }
    return (values[mid - 1] + values[mid]) / 2.0;
}

} // namespace

CursorShapeMetrics measureCursorShape(int width,
                                      int height,
                                      int hotspotX,
                                      int hotspotY,
                                      const QByteArray& bgra)
{
    CursorShapeMetrics metrics;

    if (width <= 0 || height <= 0 ||
        width > MaxCursorDimension || height > MaxCursorDimension) {
        return metrics;
    }

    if (bgra.size() != static_cast<qint64>(width) * height * 4) {
        return metrics;
    }

    const uchar* pixels = reinterpret_cast<const uchar*>(bgra.constData());

    int left = width;
    int top = height;
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const int alphaIndex = (y * width + x) * 4 + 3;
            if (pixels[alphaIndex] <= OpaqueAlphaThreshold) {
                continue;
            }
            left = qMin(left, x);
            top = qMin(top, y);
            right = qMax(right, x);
            bottom = qMax(bottom, y);
        }
    }

    if (right < 0) {
        // 整张全透明。主机偶尔会用它表示"光标不可见"。
        return metrics;
    }

    const int boxWidth = right - left + 1;
    const int boxHeight = bottom - top + 1;

    QVector<quint8> box(boxWidth * boxHeight, 0);
    int opaqueCount = 0;
    for (int y = 0; y < boxHeight; y++) {
        for (int x = 0; x < boxWidth; x++) {
            const int alphaIndex = ((y + top) * width + (x + left)) * 4 + 3;
            if (pixels[alphaIndex] > OpaqueAlphaThreshold) {
                box[y * boxWidth + x] = 1;
                opaqueCount++;
            }
        }
    }

    metrics.boxWidth = boxWidth;
    metrics.boxHeight = boxHeight;
    metrics.fill = static_cast<qreal>(opaqueCount) / (boxWidth * boxHeight);
    metrics.hotspotX = boxWidth > 1
                           ? qBound(0.0,
                                    static_cast<qreal>(hotspotX - left) / (boxWidth - 1),
                                    1.0)
                           : 0.0;
    metrics.hotspotY = boxHeight > 1
                           ? qBound(0.0,
                                    static_cast<qreal>(hotspotY - top) / (boxHeight - 1),
                                    1.0)
                           : 0.0;

    int matchV = 0;
    int matchH = 0;
    int matchRot180 = 0;
    for (int y = 0; y < boxHeight; y++) {
        for (int x = 0; x < boxWidth; x++) {
            if (box[y * boxWidth + x] == box[y * boxWidth + (boxWidth - 1 - x)]) {
                matchV++;
            }
            if (box[y * boxWidth + x] == box[(boxHeight - 1 - y) * boxWidth + x]) {
                matchH++;
            }
            if (box[y * boxWidth + x] ==
                box[(boxHeight - 1 - y) * boxWidth + (boxWidth - 1 - x)]) {
                matchRot180++;
            }
        }
    }
    metrics.symV = static_cast<qreal>(matchV) / (boxWidth * boxHeight);
    metrics.symH = static_cast<qreal>(matchH) / (boxWidth * boxHeight);
    metrics.symRot180 = static_cast<qreal>(matchRot180) / (boxWidth * boxHeight);

    qreal sumX = 0.0;
    qreal sumY = 0.0;
    qreal sumXX = 0.0;
    qreal sumYY = 0.0;
    qreal sumXY = 0.0;
    for (int y = 0; y < boxHeight; y++) {
        for (int x = 0; x < boxWidth; x++) {
            if (box[y * boxWidth + x] == 0) {
                continue;
            }
            sumX += x;
            sumY += y;
            sumXX += static_cast<qreal>(x) * x;
            sumYY += static_cast<qreal>(y) * y;
            sumXY += static_cast<qreal>(x) * y;
        }
    }
    const qreal meanX = sumX / opaqueCount;
    const qreal meanY = sumY / opaqueCount;
    const qreal varX = sumXX / opaqueCount - meanX * meanX;
    const qreal varY = sumYY / opaqueCount - meanY * meanY;
    if (varX > 0.0 && varY > 0.0) {
        const qreal covXY = sumXY / opaqueCount - meanX * meanY;
        metrics.diagonalCorrelation =
            qBound(-1.0, covXY / std::sqrt(varX * varY), 1.0);
    }

    // 正中那块的不透明占比。圆环中间是空的，四向箭头和十字的臂在中心交汇是实的。
    const int centerLeft = boxWidth * 3 / 8;
    const int centerTop = boxHeight * 3 / 8;
    const int centerWidth = qMax(1, boxWidth / 4);
    const int centerHeight = qMax(1, boxHeight / 4);
    int centerOpaque = 0;
    for (int y = centerTop; y < centerTop + centerHeight; y++) {
        for (int x = centerLeft; x < centerLeft + centerWidth; x++) {
            if (box[y * boxWidth + x] != 0) {
                centerOpaque++;
            }
        }
    }
    metrics.centerFill =
        static_cast<qreal>(centerOpaque) / (centerWidth * centerHeight);

    QVector<int> rowWidths(boxHeight, 0);
    for (int y = 0; y < boxHeight; y++) {
        for (int x = 0; x < boxWidth; x++) {
            if (box[y * boxWidth + x] != 0) {
                rowWidths[y]++;
            }
        }
    }

    const qreal medianRow = medianOf(rowWidths);
    const auto widestRow = std::max_element(rowWidths.cbegin(), rowWidths.cend());
    const int maxRow = *widestRow;
    const int widestRowIndex =
        static_cast<int>(std::distance(rowWidths.cbegin(), widestRow));

    const int topRows = qMax(1, boxHeight * 15 / 100);
    int topSum = 0;
    for (int y = 0; y < topRows; y++) {
        topSum += rowWidths[y];
    }
    if (medianRow > 0.0) {
        metrics.topWidthRatio = (static_cast<qreal>(topSum) / topRows) / medianRow;
    }
    if (maxRow > 0) {
        metrics.firstRowRatio = static_cast<qreal>(rowWidths[0]) / maxRow;
    }
    if (boxHeight > 1) {
        metrics.widestRowPosition =
            static_cast<qreal>(widestRowIndex) / (boxHeight - 1);
    }

    const int upperHalf = qMax(1, boxHeight / 2);
    for (int y = 0; y + 1 < upperHalf; y++) {
        if (rowWidths[y + 1] < rowWidths[y]) {
            metrics.topMonotoneViolations++;
        }
    }

    metrics.valid = true;
    return metrics;
}

NativeCursorShape classifyCursorShape(const CursorShapeMetrics& metrics)
{
    if (!metrics.valid) {
        return NativeCursorShape::Unknown;
    }

    // >1 表示竖着长
    const qreal tallness =
        static_cast<qreal>(metrics.boxHeight) / metrics.boxWidth;
    const bool centeredHotspot =
        metrics.hotspotX >= 0.30 && metrics.hotspotX <= 0.70 &&
        metrics.hotspotY >= 0.30 && metrics.hotspotY <= 0.70;
    const bool nearSquare = tallness >= 0.80 && tallness <= 1.25;

    // I 型：细长、左右对称、热点居中，而且第一行就是最宽的那道衬线。
    // 最后那条不能松——上下双箭头同样细长、左右对称、热点也居中，两者只能靠
    // "顶端是齐平的衬线"还是"顶端是尖的、最宽处在箭头根部"分开。
    //
    // tallness 这一档是照实测定的：Windows 64×64 的 I 型包围盒是 21×36，比值只有
    // 1.71 —— 衬线相对整体高度比想象的宽。原先按 1.8 卡，真货全被挡在外面。
    if (tallness >= 1.5 && metrics.fill <= 0.60 && metrics.symV >= 0.85 &&
        centeredHotspot &&
        metrics.firstRowRatio >= 0.90 && metrics.topWidthRatio >= 1.5) {
        return NativeCursorShape::IBeam;
    }

    // 箭头：热点钉在左上角，顶端是尖的、三角头部自上而下变宽，左右明显不对称
    if (metrics.hotspotX <= 0.20 && metrics.hotspotY <= 0.20 &&
        tallness >= 1.15 && tallness <= 2.6 &&
        metrics.symV <= 0.65 &&
        metrics.firstRowRatio <= 0.35 &&
        metrics.topMonotoneViolations <= 2) {
        return NativeCursorShape::Arrow;
    }

    // 「后台忙」的箭头 + 转圈（IDC_APPSTARTING）。热点还钉在箭头尖上（包围盒最左），
    // 但右上角多出来的圆环把盒子撑宽、也把热点在盒内的纵向位置压过了箭头那条
    // hotspotY ≤ 0.20，所以跟上面的箭头天然互斥。
    //
    // 实测（Windows 64×64）：box=44x53 fill=0.446 hotspot=(0.00,0.29) symV=0.409
    // corr=-0.540。corr 是负的，因为质量分成两坨——箭头在左侧竖着铺，圆环在右上角。
    // 圈是转的，逐帧会抖，所以卡得紧的都是不随转动变化的量（包围盒、热点），corr 这
    // 类会抖的只当个方向性的粗筛。
    if (metrics.hotspotX <= 0.15 &&
        metrics.hotspotY > 0.20 && metrics.hotspotY <= 0.45 &&
        tallness >= 0.90 && tallness <= 1.60 &&
        metrics.fill >= 0.25 && metrics.fill <= 0.65 &&
        metrics.symV <= 0.70 &&
        metrics.diagonalCorrelation <= -0.25) {
        return NativeCursorShape::AppStarting;
    }

    // 手型：热点是食指指尖，在顶端偏中；从指尖往下张开成拳头，所以顶端窄、上半部分
    // 基本单调变宽、最宽处落在下半部分。这是几条里最容易误判的一条，阈值卡得比别的
    // 紧——没有后三项，任何一块顶部带热点的不规则色块都会被认成手型。
    // symV 只用来挡掉左右完全对称的形状；真实的手型有拇指，本来就不对称。
    if (tallness >= 0.85 && tallness <= 1.45 &&
        metrics.hotspotY <= 0.20 &&
        metrics.hotspotX >= 0.15 && metrics.hotspotX <= 0.60 &&
        metrics.fill >= 0.35 && metrics.fill <= 0.80 &&
        metrics.symV <= 0.90 &&
        metrics.firstRowRatio <= 0.50 &&
        metrics.widestRowPosition >= 0.40 &&
        metrics.topMonotoneViolations <= 2) {
        return NativeCursorShape::Hand;
    }

    // 剩下几种缩放光标的热点都在正中
    if (!centeredHotspot) {
        return NativeCursorShape::Unknown;
    }

    // 等待光标（IDC_WAIT）：一个中空的圈。
    //
    // 实测（Windows 64×64）：box=40x40 fill=0.570 hotspot=(0.51,0.51)
    // symV=symH=symRot180=1.000 corr=0.000 widestRow=0.205。转的是亮度、alpha 掩码
    // 是静止的整圈，所以逐帧度量完全一致，不用担心动画抖动。
    //
    // 十字和四向箭头同样是"近方形 + 四重对称 + 热点居中"，跟这条挤在一起，靠两点
    // 分开：圈的填充率高得多，而且中心是空的（centerFill≈0，它们的臂在中心交汇）。
    if (nearSquare &&
        metrics.symV >= 0.90 && metrics.symH >= 0.90 &&
        metrics.symRot180 >= 0.90 &&
        metrics.fill >= 0.40 && metrics.fill <= 0.80 &&
        metrics.centerFill <= 0.15) {
        return NativeCursorShape::Wait;
    }

    // 左右双箭头
    if (tallness <= 1.0 / 1.8 && metrics.symV >= 0.85 && metrics.symH >= 0.80) {
        return NativeCursorShape::SizeWE;
    }

    // 上下双箭头。firstRowRatio 小说明顶端是尖的，把 I 型排除在外。
    if (tallness >= 1.8 && metrics.symH >= 0.85 && metrics.symV >= 0.80 &&
        metrics.firstRowRatio <= 0.60) {
        return NativeCursorShape::SizeNS;
    }

    // 对角双箭头：整块质量沿某一条对角线铺开，用带符号的相关系数定方向。
    // 十字、四向箭头、实心块这类没有方向性的形状相关系数接近 0，会落到
    // Unknown 去画位图——正是我们要的保守行为。
    if (nearSquare && metrics.fill <= 0.45 && metrics.symRot180 >= 0.85) {
        if (metrics.diagonalCorrelation >= 0.70) {
            return NativeCursorShape::SizeNWSE;
        }
        if (metrics.diagonalCorrelation <= -0.70) {
            return NativeCursorShape::SizeNESW;
        }
    }

    return NativeCursorShape::Unknown;
}

NativeCursorShape classifyCursorShape(int width,
                                      int height,
                                      int hotspotX,
                                      int hotspotY,
                                      const QByteArray& bgra)
{
    return classifyCursorShape(
        measureCursorShape(width, height, hotspotX, hotspotY, bgra));
}

const char* nativeCursorShapeName(NativeCursorShape shape)
{
    switch (shape) {
    case NativeCursorShape::Arrow:    return "arrow";
    case NativeCursorShape::AppStarting: return "app-starting";
    case NativeCursorShape::Wait:     return "wait";
    case NativeCursorShape::IBeam:    return "ibeam";
    case NativeCursorShape::Hand:     return "hand";
    case NativeCursorShape::SizeWE:   return "size-we";
    case NativeCursorShape::SizeNS:   return "size-ns";
    case NativeCursorShape::SizeNWSE: return "size-nwse";
    case NativeCursorShape::SizeNESW: return "size-nesw";
    case NativeCursorShape::Unknown:  break;
    }
    return "unknown";
}
