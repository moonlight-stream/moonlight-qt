#include "overlaymenubutton.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPainterPath>
#include <QStyleHints>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QInputDevice>
#endif

namespace {
QPoint globalMousePosition(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

bool mouseEventComesFromTouch(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->pointingDevice() &&
           event->pointingDevice()->type() == QInputDevice::DeviceType::TouchScreen;
#else
    return event->source() != Qt::MouseEventNotSynthesized;
#endif
}
}

OverlayMenuButton::OverlayMenuButton(QWindow* parent)
    : QRasterWindow(parent),
      m_Hovered(false),
      m_ButtonVisible(false),
      m_Dragging(false),
      m_InputSource(InputSource::None),
      m_TouchPointId(-1),
      m_NormalizedPosition(m_PositionStore.load())
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

void OverlayMenuButton::repositionTo(int parentX, int parentY, int parentW, int parentH)
{
    const QRect newParentGeometry(parentX, parentY, parentW, parentH);
    const QRect previousParentGeometry = m_ParentGeometry;
    const QPoint previousPosition = position();
    m_ParentGeometry = newParentGeometry;

    QPoint newPosition;
    if (m_InputSource != InputSource::None) {
        const QPoint parentTranslation = previousParentGeometry.isValid()
                ? m_ParentGeometry.topLeft() - previousParentGeometry.topLeft()
                : QPoint();
        newPosition = OverlayButtonPlacement::clamp(
                previousPosition + parentTranslation,
                m_ParentGeometry,
                QSize(kButtonSize, kButtonSize),
                kMargin);
        // Keep the active drag origin aligned with any parent movement or
        // resize so the next pointer event cannot jump back to stale bounds.
        m_WindowPositionAtPress += newPosition - previousPosition;
    }
    else if (m_NormalizedPosition) {
        newPosition = OverlayButtonPlacement::resolve(
                *m_NormalizedPosition,
                m_ParentGeometry,
                QSize(kButtonSize, kButtonSize),
                kMargin);
    }
    else {
        newPosition = OverlayButtonPlacement::defaultPosition(
                m_ParentGeometry, QSize(kButtonSize, kButtonSize), kMargin);
    }

    setGeometry(QRect(clampToParent(newPosition), QSize(kButtonSize, kButtonSize)));
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
    cancelInteraction();
    m_ButtonVisible = false;
    unsetCursor();
    hide();
}

QPoint OverlayMenuButton::clampToParent(const QPoint& position) const
{
    return OverlayButtonPlacement::clamp(
            position, m_ParentGeometry, QSize(kButtonSize, kButtonSize), kMargin);
}

void OverlayMenuButton::beginInteraction(InputSource source, const QPoint& globalPosition)
{
    if (m_InputSource != InputSource::None) {
        return;
    }

    m_InputSource = source;
    m_Dragging = false;
    m_PressGlobalPosition = globalPosition;
    m_WindowPositionAtPress = position();
}

void OverlayMenuButton::updateInteraction(const QPoint& globalPosition)
{
    if (m_InputSource == InputSource::None) {
        return;
    }

    const QPoint delta = globalPosition - m_PressGlobalPosition;
    if (!m_Dragging &&
            delta.manhattanLength() >= QGuiApplication::styleHints()->startDragDistance()) {
        m_Dragging = true;
        setCursor(Qt::ClosedHandCursor);
    }

    if (m_Dragging) {
        const QPoint targetPosition = clampToParent(m_WindowPositionAtPress + delta);
        if (targetPosition != position()) {
            setPosition(targetPosition);
        }
    }
}

void OverlayMenuButton::finishInteraction(const QPoint& globalPosition)
{
    if (m_InputSource == InputSource::None) {
        return;
    }

    const InputSource source = m_InputSource;
    const bool dragged = m_Dragging;
    const bool activate = !m_Dragging;

    if (dragged && m_ParentGeometry.isValid()) {
        m_NormalizedPosition = OverlayButtonPlacement::normalize(
                position(), m_ParentGeometry, QSize(kButtonSize, kButtonSize), kMargin);
        if (!m_PositionStore.save(*m_NormalizedPosition)) {
            qWarning("Failed to save the device-local overlay button position");
        }
    }

    cancelInteraction();

    if (activate && m_ClickCallback) {
        // Touch input has no reliable QCursor position, so it must not use
        // mouse-hover auto-close checks after opening the menu.
        m_ClickCallback(globalPosition, source == InputSource::Mouse);
    }
}

void OverlayMenuButton::cancelInteraction()
{
    const bool wasTouch = m_InputSource == InputSource::Touch;
    m_InputSource = InputSource::None;
    m_TouchPointId = -1;
    m_Dragging = false;

    if (wasTouch) {
        // Touch input has no hover state after the finger is released.
        m_Hovered = false;
        setOpacity(0.35);
        requestUpdate();
    }

    if (m_Hovered) {
        setCursor(Qt::OpenHandCursor);
    }
    else {
        unsetCursor();
    }
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
    if (event->button() == Qt::LeftButton && m_InputSource == InputSource::None) {
        beginInteraction(mouseEventComesFromTouch(event) ? InputSource::Touch
                                                        : InputSource::Mouse,
                         globalMousePosition(event));
        event->accept();
    }
}

void OverlayMenuButton::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_Hovered) {
        m_Hovered = true;
        setOpacity(0.95);
        requestUpdate();
    }

    if (m_InputSource == InputSource::None) {
        setCursor(Qt::OpenHandCursor);
        return;
    }

    if (!(event->buttons() & Qt::LeftButton)) {
        // A cancelled touch sequence may not produce a synthesized mouse
        // release. Clear the state as soon as the button mask says it ended.
        cancelInteraction();
        return;
    }

    updateInteraction(globalMousePosition(event));
    event->accept();
}

void OverlayMenuButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_InputSource == InputSource::None) {
        return;
    }

    finishInteraction(globalMousePosition(event));
    event->accept();
}

void OverlayMenuButton::touchEvent(QTouchEvent* event)
{
    if (event->type() == QEvent::TouchCancel) {
        cancelInteraction();
        event->accept();
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const auto points = event->points();
    if (points.isEmpty()) {
        if (event->type() == QEvent::TouchEnd && m_InputSource == InputSource::Touch) {
            cancelInteraction();
        }
        event->accept();
        return;
    }

    if (event->type() == QEvent::TouchBegin && m_InputSource == InputSource::None) {
        m_TouchPointId = points.first().id();
        beginInteraction(InputSource::Touch, points.first().globalPosition().toPoint());
    }

    bool foundTrackedPoint = false;
    for (const auto& point : points) {
        if (point.id() != m_TouchPointId) {
            continue;
        }

        foundTrackedPoint = true;
        const QPoint globalPosition = point.globalPosition().toPoint();
        if (point.state() == QEventPoint::Released) {
            finishInteraction(globalPosition);
        }
        else if (event->type() == QEvent::TouchUpdate) {
            updateInteraction(globalPosition);
        }
        else if (event->type() == QEvent::TouchEnd) {
            finishInteraction(globalPosition);
        }
        break;
    }
    if (event->type() == QEvent::TouchEnd &&
            !foundTrackedPoint && m_InputSource == InputSource::Touch) {
        cancelInteraction();
    }
#else
    const auto points = event->touchPoints();
    if (points.isEmpty()) {
        if (event->type() == QEvent::TouchEnd && m_InputSource == InputSource::Touch) {
            cancelInteraction();
        }
        event->accept();
        return;
    }

    if (event->type() == QEvent::TouchBegin && m_InputSource == InputSource::None) {
        m_TouchPointId = points.first().id();
        beginInteraction(InputSource::Touch, points.first().screenPos().toPoint());
    }

    bool foundTrackedPoint = false;
    for (const auto& point : points) {
        if (point.id() != m_TouchPointId) {
            continue;
        }

        foundTrackedPoint = true;
        const QPoint globalPosition = point.screenPos().toPoint();
        if (point.state() == Qt::TouchPointReleased) {
            finishInteraction(globalPosition);
        }
        else if (event->type() == QEvent::TouchUpdate) {
            updateInteraction(globalPosition);
        }
        else if (event->type() == QEvent::TouchEnd) {
            finishInteraction(globalPosition);
        }
        break;
    }
    if (event->type() == QEvent::TouchEnd &&
            !foundTrackedPoint && m_InputSource == InputSource::Touch) {
        cancelInteraction();
    }
#endif

    // Accepting the native touch sequence prevents Qt from also synthesizing
    // a duplicate mouse interaction for the same finger.
    event->accept();
}

bool OverlayMenuButton::event(QEvent* ev)
{
    if (ev->type() == QEvent::Leave) {
        m_Hovered = false;
        setOpacity(0.35);
        requestUpdate();
    }
    else if (ev->type() == QEvent::UngrabMouse && m_InputSource == InputSource::Mouse) {
        cancelInteraction();
    }
    return QRasterWindow::event(ev);
}
