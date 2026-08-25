#include "overlaytoast.h"
#include "uifont.h"

#include <QScreen>
#include <QFontMetrics>

namespace {

// app/gui/theme/Theme.qml 里的 token，这一层是 QPainter 手绘的，进不到 QML 单例，
// 只能在这里对着抄一份。改配色时两边要一起改。
const QColor kSurface(0x17, 0x1A, 0x20);   // Theme.surface
const QColor kLine(0x2B, 0x30, 0x38);      // Theme.line
const QColor kAccent(0x39, 0xC5, 0xBB);    // Theme.accent
const QColor kText(0xEE, 0xF0, 0xEC);      // Theme.text
const QColor kShadow(0, 0, 0, 0x8C);       // Theme.shadowColor

const int kShadowOffset = 6;               // Theme.shadowOffset
const int kAccentBar = 4;                  // Theme.accentBar

}

OverlayToast::OverlayToast(QWindow* parent)
    : QRasterWindow(parent),
      m_FadeAnimation(nullptr),
      m_ToastHeight(40),
      m_HorizPadding(16),
      m_VertPadding(10)
{
    setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
             | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);

    QSurfaceFormat fmt;
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);

    // 字体跟应用界面走。以前这里写死 "Segoe UI"，那是个 Windows 字体 ——
    // macOS 和 Linux 上根本没有，实际渲染出来是系统回退字体，三个平台长得不一样。
    // Manrope 由 main.cpp 注册进 QFontDatabase，进程内哪儿都能用；它没有中文字形，
    // 所以后面按平台补 CJK 回退（提示文案是会被翻译的）。
    m_Font.setFamilies(UiFont::familyChain(QStringLiteral("Manrope")));
    m_Font.setPointSize(11);
    m_Font.setWeight(QFont::DemiBold);
    m_Font.setStyleHint(QFont::SansSerif);

    m_FadeAnimation = new QPropertyAnimation(this, "opacity", this);
    // 淡出跟着 Theme 的动效时长走：这套风格的动效更短更机械
    m_FadeAnimation->setDuration(240);
    m_FadeAnimation->setStartValue(1.0);
    m_FadeAnimation->setEndValue(0.0);
    connect(m_FadeAnimation, &QPropertyAnimation::finished,
            this, &OverlayToast::onFadeFinished);

    m_Clock.start();
}

OverlayToast::~OverlayToast()
{
    dismissImmediately();
}

bool OverlayToast::needsEventProcessing() const
{
    return m_EventState.needsEventProcessing(m_Clock.elapsed());
}

int OverlayToast::nextEventDelayMs() const
{
    return m_EventState.nextEventDelayMs(m_Clock.elapsed());
}

void OverlayToast::beginEventProcessing()
{
    if (m_EventState.beginEventProcessing(m_Clock.elapsed())) {
        startFadeOut();
    }
}

void OverlayToast::dismissImmediately()
{
    m_FadeAnimation->stop();
    m_EventState.cancel();
    hide();
    setOpacity(1.0);
}

void OverlayToast::showToast(int parentX, int parentY, int parentW, int parentH,
                             const QString& message, int durationMs)
{
    m_Message = message;

    // Stop any ongoing fade before replacing the toast. The state deadline is
    // reset below, so an expired older toast cannot dismiss the new message.
    m_FadeAnimation->stop();
    m_EventState.show(m_Clock.elapsed(), durationMs);
    setOpacity(1.0);

    // Calculate dimensions
    QFontMetrics fm(m_Font);
    int textWidth = fm.horizontalAdvance(m_Message) + m_HorizPadding * 2 + kAccentBar;
    int toastWidth = qMin(textWidth, 560);
    if (toastWidth < 160) toastWidth = 160;

    QRect textRect = fm.boundingRect(QRect(0, 0, toastWidth - m_HorizPadding * 2 - kAccentBar, 200),
                                     Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                                     m_Message);
    m_ToastHeight = qMin(qMax(textRect.height() + m_VertPadding * 2, 36), 140);

    int qpX = parentX;
    int qpY = parentY;
    int qpW = parentW;
    int qpH = parentH;

    // Position at bottom-center, 60px above the bottom.
    // 窗口比本体大出一圈硬投影的宽度，投影画在右下。
    int x = qpX + (qpW - toastWidth) / 2;
    int y = qpY + qpH - m_ToastHeight - 60;

    setGeometry(x, y, toastWidth + kShadowOffset, m_ToastHeight + kShadowOffset);
    show();
    raise();
    requestUpdate();
}

void OverlayToast::startFadeOut()
{
    if (!m_EventState.isFading()) {
        return;
    }
    m_FadeAnimation->start();
}

void OverlayToast::onFadeFinished()
{
    m_EventState.finishFade();
    hide();
    setOpacity(1.0);
}

void OverlayToast::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    // 反锯齿只为文字开。方角矩形都落在整数坐标上，开不开都一样，
    // 但这套风格的边必须是硬的，别让抗锯齿在边缘糊出半透明像素。
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    int w = width();
    int h = height();
    int bodyW = w - kShadowOffset;
    int bodyH = h - kShadowOffset;

    // Clear to transparent
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(0, 0, w, h, Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // 零模糊硬投影：就是一块偏移的实心矩形垫在本体后面，和 QML 那边的 Panel 同一个做法
    p.fillRect(QRect(kShadowOffset, kShadowOffset, bodyW, bodyH), kShadow);

    // 本体：方角、不透明。以前是 8px 圆角 + 200/255 半透明，
    // 这两样都是和新粗野主义直接冲突的（没有圆角，也没有半透明表面）。
    QRect body(0, 0, bodyW, bodyH);
    p.fillRect(body, kSurface);

    p.setPen(QPen(kLine, 1.0));
    p.drawRect(QRect(body.left(), body.top(), body.width() - 1, body.height() - 1));

    // 左侧强调粗条
    p.fillRect(QRect(1, 1, kAccentBar, bodyH - 2), kAccent);

    // 文字左对齐，和加载页的阶段文字一致
    p.setFont(m_Font);
    p.setPen(kText);
    p.drawText(QRect(kAccentBar + m_HorizPadding, m_VertPadding,
                     bodyW - kAccentBar - m_HorizPadding * 2, bodyH - m_VertPadding * 2),
               Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, m_Message);
}
