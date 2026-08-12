#include "streaming/input/cursorshapeclassifier.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QTextStream>

namespace {

// 一张 BGRA8888 画布，只用来搭合成光标。所有不透明像素都画成白色——分类器只看
// alpha，颜色不参与判断。
class Canvas
{
public:
    Canvas(int width, int height)
        : m_width(width),
          m_height(height),
          m_data(width * height * 4, '\0')
    {
    }

    void plot(int x, int y)
    {
        if (x < 0 || y < 0 || x >= m_width || y >= m_height) {
            return;
        }
        const int index = (y * m_width + x) * 4;
        m_data[index + 0] = static_cast<char>(0xFF); // B
        m_data[index + 1] = static_cast<char>(0xFF); // G
        m_data[index + 2] = static_cast<char>(0xFF); // R
        m_data[index + 3] = static_cast<char>(0xFF); // A
    }

    void plotRow(int y, int x0, int width)
    {
        for (int x = x0; x < x0 + width; x++) {
            plot(x, y);
        }
    }

    int width() const { return m_width; }
    int height() const { return m_height; }
    const QByteArray& data() const { return m_data; }

private:
    int m_width;
    int m_height;
    QByteArray m_data;
};

// Windows 标准箭头的骨架：热点在左上角，三角头部自上而下变宽，接着一条偏右的尾巴
Canvas makeArrow(int s)
{
    Canvas canvas(32 * s, 32 * s);

    const int headRows = 12 * s;
    const int totalRows = 19 * s;
    const int maxWidth = 11 * s;

    for (int y = 0; y < totalRows; y++) {
        if (y < headRows) {
            const int width = 1 + (maxWidth - 1) * y / (headRows - 1);
            canvas.plotRow(y, 0, width);
        }
        else {
            const int tailRows = totalRows - headRows;
            const int width = qMax(1, 6 * s - (y - headRows) * (4 * s) / tailRows);
            canvas.plotRow(y, 3 * s, width);
        }
    }

    return canvas;
}

// I 型：上下两道齐平的衬线夹一条细竖杆。
//
// 比例照实测的真货来：Sunshine 推的 Windows 64×64 I 型，不透明包围盒 21×36、
// fill≈0.444、topWidthRatio≈4.12。衬线相对整体高度比直觉宽得多（比值只有 1.71），
// 早先按一个瘦长的 7×20 合成体来定阈值，真货全被挡在外面了。
Canvas makeIBeam(int s)
{
    Canvas canvas(64 * s, 64 * s);

    const int totalRows = 36 * s;
    const int serifWidth = 21 * s;
    const int serifRows = 5 * s;
    const int barWidth = 5 * s;
    const int barLeft = (serifWidth - barWidth) / 2;

    for (int y = 0; y < totalRows; y++) {
        if (y < serifRows || y >= totalRows - serifRows) {
            canvas.plotRow(y, 0, serifWidth);
        }
        else {
            canvas.plotRow(y, barLeft, barWidth);
        }
    }

    return canvas;
}

// 上下双箭头：两端是尖的，最宽处在箭头根部，中间一条细杆
Canvas makeSizeNS(int s)
{
    Canvas canvas(32 * s, 32 * s);

    const int totalRows = 20 * s;
    const int boxWidth = 9 * s;
    const int stemWidth = 3 * s;
    const int arrowRows = 4 * s;

    for (int y = 0; y < totalRows; y++) {
        const int d = qMin(y, totalRows - 1 - y);
        int width = stemWidth;
        if (d < arrowRows) {
            width = stemWidth +
                    (boxWidth - stemWidth) * d / (arrowRows - 1);
            // 保持奇数宽度，才能在 boxWidth 里精确居中
            if ((width % 2) != (boxWidth % 2)) {
                width++;
            }
        }
        canvas.plotRow(y, (boxWidth - width) / 2, width);
    }

    return canvas;
}

// 左右双箭头 = 上下双箭头转置
Canvas makeSizeWE(int s)
{
    const Canvas source = makeSizeNS(s);
    Canvas canvas(source.width(), source.height());

    const uchar* pixels = reinterpret_cast<const uchar*>(source.data().constData());
    for (int y = 0; y < source.height(); y++) {
        for (int x = 0; x < source.width(); x++) {
            const int index = (y * source.width() + x) * 4 + 3;
            if (pixels[index] != 0) {
                canvas.plot(y, x);
            }
        }
    }

    return canvas;
}

// 沿主对角线（左上-右下）的双箭头。按主对角转置对称地构造，两端各带一个 L 形箭头。
Canvas makeSizeNWSE(int s, bool mirrored)
{
    const int extent = 16 * s;
    const int headLength = 5 * s;
    Canvas canvas(32 * s, 32 * s);

    auto plot = [&](int x, int y) {
        canvas.plot(mirrored ? (extent - 1 - x) : x, y);
    };

    for (int i = 0; i < extent; i++) {
        plot(i, i);
        if (i + 1 < extent) {
            plot(i + 1, i);
            plot(i, i + 1);
        }
    }

    for (int k = 0; k < headLength; k++) {
        plot(k, 0);
        plot(k, 1);
        plot(0, k);
        plot(1, k);
        plot(extent - 1 - k, extent - 1);
        plot(extent - 1 - k, extent - 2);
        plot(extent - 1, extent - 1 - k);
        plot(extent - 2, extent - 1 - k);
    }

    return canvas;
}

// 手型：一根食指立在拳头上，指尖偏左于整体中线
Canvas makeHand(int s)
{
    Canvas canvas(32 * s, 32 * s);

    const int boxWidth = 20 * s;
    const int fingerWidth = 4 * s;
    const int fingerLeft = 6 * s;
    const int fingerRows = 8 * s;
    const int flareRows = 3 * s;
    const int totalRows = 22 * s;

    for (int y = 0; y < totalRows; y++) {
        if (y < fingerRows) {
            canvas.plotRow(y, fingerLeft, fingerWidth);
        }
        else if (y < fingerRows + flareRows) {
            // 手掌张开的过渡段
            const int step = y - fingerRows + 1;
            const int width = fingerWidth +
                              (boxWidth - fingerWidth) * step / (flareRows + 1);
            canvas.plotRow(y, (boxWidth - width) / 2, width);
        }
        else {
            canvas.plotRow(y, 0, boxWidth);
        }
    }

    return canvas;
}

// 箭头 + 右上角一个转圈（IDC_APPSTARTING）。
//
// 比例照实测：Windows 64×64 这只的包围盒是 44×53，热点归一化到 (0.00, 0.29)。热点
// 的纵向位置不是 0，是因为那个圈比箭头尖还高——包围盒的顶边是圈的顶，箭头尖在它
// 下面约 15 像素处。corr≈-0.54 也来自这个布局：质量分两坨，箭头在左侧竖着铺，圈在
// 右上角。
Canvas makeAppStarting(int s, int& hotspotX, int& hotspotY)
{
    Canvas canvas(64 * s, 64 * s);

    // 右上角的圈，顶边就是整体包围盒的顶边
    const int cx = 30 * s;
    const int cy = 13 * s;
    const int outer = 13 * s;
    const int inner = 5 * s;
    for (int y = cy - outer; y <= cy + outer; y++) {
        for (int x = cx - outer; x <= cx + outer; x++) {
            const int dx = x - cx;
            const int dy = y - cy;
            const int d2 = dx * dx + dy * dy;
            if (d2 <= outer * outer && d2 >= inner * inner) {
                canvas.plot(x, y);
            }
        }
    }

    // 左边那只箭头，比普通箭头粗一圈（64 像素档的实际观感），尖端压在圈的下方
    const int arrowTop = 15 * s;
    const int totalRows = 38 * s;
    const int headRows = 24 * s;
    const int maxWidth = 22 * s;
    for (int y = 0; y < totalRows; y++) {
        if (y < headRows) {
            const int width = 1 + (maxWidth - 1) * y / (headRows - 1);
            canvas.plotRow(arrowTop + y, 0, width);
        }
        else {
            const int tailRows = totalRows - headRows;
            const int width = qMax(1, 12 * s - (y - headRows) * (8 * s) / tailRows);
            canvas.plotRow(arrowTop + y, 7 * s, width);
        }
    }

    hotspotX = 0;
    hotspotY = arrowTop;
    return canvas;
}

// 等待光标：一个中空的圈。
//
// 比例照实测：Windows 64×64 这只的包围盒是 40×40、fill≈0.570，反推内圈半径约为外圈
// 的 0.52。
Canvas makeWait(int s)
{
    Canvas canvas(64 * s, 64 * s);

    const int size = 40 * s;
    const qreal center = (size - 1) / 2.0;
    const qreal outer = size / 2.0;
    const qreal inner = outer * 0.524;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            const qreal dx = x - center;
            const qreal dy = y - center;
            const qreal d2 = dx * dx + dy * dy;
            if (d2 <= outer * outer && d2 >= inner * inner) {
                canvas.plot(x, y);
            }
        }
    }

    return canvas;
}

// 四向箭头（IDC_SIZEALL）和十字（IDC_CROSS）：跟等待光标同样是近方形 + 四重对称 +
// 热点居中，故意不认。它们的臂在中心交汇，中心是实的，正是靠这一点跟圈分开的。
Canvas makeSizeAll(int s)
{
    Canvas canvas(64 * s, 64 * s);

    const int size = 32 * s;
    const int arm = 5 * s;
    const int head = 9 * s;
    const int mid = size / 2;

    for (int y = mid - arm / 2; y < mid - arm / 2 + arm; y++) {
        canvas.plotRow(y, 0, size);
    }
    for (int y = 0; y < size; y++) {
        canvas.plotRow(y, mid - arm / 2, arm);
    }
    // 四个箭头头部
    for (int k = 0; k < head / 2; k++) {
        canvas.plotRow(k, mid - k, 2 * k + 1);
        canvas.plotRow(size - 1 - k, mid - k, 2 * k + 1);
        for (int y = mid - k; y <= mid + k; y++) {
            canvas.plot(k, y);
            canvas.plot(size - 1 - k, y);
        }
    }

    return canvas;
}

Canvas makeCrosshair(int s)
{
    Canvas canvas(64 * s, 64 * s);

    const int size = 32 * s;
    const int thickness = 3 * s;
    const int mid = size / 2 - thickness / 2;

    for (int y = mid; y < mid + thickness; y++) {
        canvas.plotRow(y, 0, size);
    }
    for (int y = 0; y < size; y++) {
        canvas.plotRow(y, mid, thickness);
    }

    return canvas;
}

// 一块确定性的噪声斑，代表游戏自绘光标：必须落到 Unknown
Canvas makeNoiseBlob()
{
    Canvas canvas(32, 32);

    quint32 state = 0x1234567u;
    for (int y = 4; y < 26; y++) {
        for (int x = 5; x < 24; x++) {
            state = state * 1664525u + 1013904223u;
            if (((state >> 16) & 0xFF) > 96) {
                canvas.plot(x, y);
            }
        }
    }

    return canvas;
}

Canvas makeSolidBlock()
{
    Canvas canvas(32, 32);
    for (int y = 8; y < 24; y++) {
        canvas.plotRow(y, 8, 16);
    }
    return canvas;
}

void printMetrics(QTextStream& out, const QString& label, const CursorShapeMetrics& m)
{
    out << "  " << label << ": box=" << m.boxWidth << 'x' << m.boxHeight
        << " fill=" << QString::number(m.fill, 'f', 3)
        << " hotspot=(" << QString::number(m.hotspotX, 'f', 2) << ','
        << QString::number(m.hotspotY, 'f', 2) << ')'
        << " symV=" << QString::number(m.symV, 'f', 3)
        << " symH=" << QString::number(m.symH, 'f', 3)
        << " symRot180=" << QString::number(m.symRot180, 'f', 3)
        << " centerFill=" << QString::number(m.centerFill, 'f', 3)
        << " corr=" << QString::number(m.diagonalCorrelation, 'f', 3)
        << " firstRow=" << QString::number(m.firstRowRatio, 'f', 3)
        << " widestRow=" << QString::number(m.widestRowPosition, 'f', 3)
        << " topWidth=" << QString::number(m.topWidthRatio, 'f', 3)
        << " violations=" << m.topMonotoneViolations << '\n';
}

bool expectShape(QTextStream& out,
                 const QString& label,
                 const Canvas& canvas,
                 int hotspotX,
                 int hotspotY,
                 NativeCursorShape expected)
{
    const CursorShapeMetrics metrics = measureCursorShape(
        canvas.width(), canvas.height(), hotspotX, hotspotY, canvas.data());
    const NativeCursorShape actual = classifyCursorShape(metrics);

    printMetrics(out, label, metrics);

    if (actual != expected) {
        out << "FAIL: " << label << " expected "
            << nativeCursorShapeName(expected) << ", got "
            << nativeCursorShapeName(actual) << '\n';
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    bool ok = true;

    // 同一形状的 1x 与 2x 都要认出来——主机的 DPI 缩放会在这几档之间变
    for (int s = 1; s <= 2; s++) {
        const QString suffix = QStringLiteral(" @%1x").arg(s);

        ok &= expectShape(out, QStringLiteral("arrow") + suffix,
                          makeArrow(s), 0, 0, NativeCursorShape::Arrow);

        int appStartingHx = 0;
        int appStartingHy = 0;
        const Canvas appStarting = makeAppStarting(s, appStartingHx, appStartingHy);
        ok &= expectShape(out, QStringLiteral("app-starting") + suffix,
                          appStarting, appStartingHx, appStartingHy,
                          NativeCursorShape::AppStarting);
        ok &= expectShape(out, QStringLiteral("wait") + suffix,
                          makeWait(s), 20 * s, 20 * s, NativeCursorShape::Wait);

        ok &= expectShape(out, QStringLiteral("ibeam") + suffix,
                          makeIBeam(s), 10 * s, 18 * s, NativeCursorShape::IBeam);

        ok &= expectShape(out, QStringLiteral("hand") + suffix,
                          makeHand(s), 7 * s, 0, NativeCursorShape::Hand);

        ok &= expectShape(out, QStringLiteral("size-ns") + suffix,
                          makeSizeNS(s), 4 * s, 10 * s, NativeCursorShape::SizeNS);

        ok &= expectShape(out, QStringLiteral("size-we") + suffix,
                          makeSizeWE(s), 10 * s, 4 * s, NativeCursorShape::SizeWE);

        ok &= expectShape(out, QStringLiteral("size-nwse") + suffix,
                          makeSizeNWSE(s, false), 8 * s, 8 * s,
                          NativeCursorShape::SizeNWSE);

        ok &= expectShape(out, QStringLiteral("size-nesw") + suffix,
                          makeSizeNWSE(s, true), 8 * s, 8 * s,
                          NativeCursorShape::SizeNESW);
    }

    // 不认识的一律回退位图
    // 四向箭头和十字是故意不认的：它们跟等待光标同属"近方形 + 四重对称 + 热点居中"
    ok &= expectShape(out, QStringLiteral("size-all"),
                      makeSizeAll(1), 16, 16, NativeCursorShape::Unknown);
    ok &= expectShape(out, QStringLiteral("crosshair"),
                      makeCrosshair(1), 16, 16, NativeCursorShape::Unknown);
    ok &= expectShape(out, QStringLiteral("noise blob"),
                      makeNoiseBlob(), 12, 8, NativeCursorShape::Unknown);
    ok &= expectShape(out, QStringLiteral("solid block"),
                      makeSolidBlock(), 16, 16, NativeCursorShape::Unknown);

    // 全透明（主机用来表示光标不可见）不能被当成任何形状
    const Canvas blank(32, 32);
    ok &= expectShape(out, QStringLiteral("fully transparent"),
                      blank, 0, 0, NativeCursorShape::Unknown);

    // 输入长度不符时必须安全地判为不认识
    if (classifyCursorShape(32, 32, 0, 0, QByteArray(16, '\0')) !=
        NativeCursorShape::Unknown) {
        out << "FAIL: short pixel buffer was not rejected\n";
        ok = false;
    }
    if (classifyCursorShape(0, 0, 0, 0, QByteArray()) !=
        NativeCursorShape::Unknown) {
        out << "FAIL: zero-sized shape was not rejected\n";
        ok = false;
    }

    out << (ok ? "PASS\n" : "FAILED\n");
    out.flush();
    return ok ? 0 : 1;
}
