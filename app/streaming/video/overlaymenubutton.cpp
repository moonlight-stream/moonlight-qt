#include "overlaymenubutton.h"

#include <QScreen>
#include <QPainterPath>

OverlayMenuButton::OverlayMenuButton(QWindow* parent)
    : QRasterWindow(parent),
      m_Hovered(false),
      m_ButtonVisible(false)
{
    setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
             | Qt::WindowDoesNotAcceptFocus);

    QSurfaceFormat fmt;
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);

    setOpacity(0.35);
}

OverlayMenuButton::~OverlayMenuButton()
{
}

void OverlayMenuButton::repositionTo(int parentX, int parentY, int parentW, int /*parentH*/)
{
    int qpX = parentX;
    int qpY = parentY;
    int qpW = parentW;

    // Position at top-right corner of the streaming window
    int x = qpX + qpW - kButtonSize - kMargin;
    int y = qpY + kMargin;

    setGeometry(x, y, kButtonSize, kButtonSize);
}

void OverlayMenuButton::showButton(int parentX, int parentY, int parentW, int parentH)
{
    repositionTo(parentX, parentY, parentW, parentH);
    m_ButtonVisible = true;
    show();
    raise();
    requestUpdate();
}

void OverlayMenuButton::hideButton()
{
    m_ButtonVisible = false;
    hide();
}

void OverlayMenuButton::drawCrescentMoon(QPainter& p, qreal cx, qreal cy, qreal radius)
{
    // Crescent moon: full circle minus an offset circle
    QPainterPath moonPath;
    moonPath.addEllipse(QPointF(cx, cy), radius, radius);

    QPainterPath cutout;
    cutout.addEllipse(QPointF(cx + radius * 0.5, cy - radius * 0.25), radius * 0.78, radius * 0.82);

    QPainterPath crescent = moonPath.subtracted(cutout);

    // Soft golden glow
    QColor moonColor = m_Hovered ? QColor(255, 235, 140) : QColor(230, 215, 150);
    p.fillPath(crescent, moonColor);
}

void OverlayMenuButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // Clear to transparent
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(0, 0, w, h, Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    qreal cx = w / 2.0;
    qreal cy = h / 2.0;
    qreal bgRadius = qMin(w, h) / 2.0 - 1.0;

    // Circular dark background
    QPainterPath bgCircle;
    bgCircle.addEllipse(QPointF(cx, cy), bgRadius, bgRadius);

    QColor bgColor = m_Hovered ? QColor(35, 40, 75, 230) : QColor(20, 24, 50, 200);
    p.fillPath(bgCircle, bgColor);

    // Subtle border
    QColor borderColor = m_Hovered ? QColor(120, 150, 230, 150) : QColor(70, 85, 150, 80);
    p.setPen(QPen(borderColor, 1.0));
    p.drawPath(bgCircle);

    // Draw crescent moon centered in the background
    qreal moonR = bgRadius * 0.55;
    drawCrescentMoon(p, cx, cy, moonR);
}

void OverlayMenuButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_ClickCallback) {
            m_ClickCallback();
        }
    }
}

void OverlayMenuButton::mouseMoveEvent(QMouseEvent*)
{
    if (!m_Hovered) {
        m_Hovered = true;
        setOpacity(0.95);
        requestUpdate();
    }
}

bool OverlayMenuButton::event(QEvent* ev)
{
    if (ev->type() == QEvent::Leave) {
        m_Hovered = false;
        setOpacity(0.35);
        requestUpdate();
    }
    return QRasterWindow::event(ev);
}
