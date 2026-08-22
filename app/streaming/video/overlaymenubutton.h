#pragma once

#include <QRasterWindow>
#include <QPainter>
#include <QMouseEvent>
#include <QSurfaceFormat>
#include <QTouchEvent>
#include <functional>

/**
 * OverlayMenuButton - A small floating button rendered by the OS compositor,
 * positioned at the top-right corner of the streaming window.
 *
 * When clicked, triggers a callback to open the overlay menu.
 * Semi-transparent when idle, fully opaque on hover.
 * Independent of D3D11/SDL rendering pipeline.
 */
class OverlayMenuButton : public QRasterWindow {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    using ClickCallback = std::function<void(const QPoint& globalPosition,
                                             bool closeWhenPointerOutside)>;

    explicit OverlayMenuButton(QWindow* parent = nullptr);
    ~OverlayMenuButton() override;

    void setClickCallback(ClickCallback cb) { m_ClickCallback = std::move(cb); }

    /**
     * Reposition the button relative to the given Qt logical parent rect.
     * Places the button at the top-right corner of the streaming window.
     */
    void repositionTo(int parentX, int parentY, int parentW, int parentH);

    /**
     * Show the button at the top-right corner of the given parent rect.
     */
    void showButton(int parentX, int parentY, int parentW, int parentH);

    /**
     * Hide the button.
     */
    void hideButton();

    bool isButtonVisible() const { return m_ButtonVisible; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void touchEvent(QTouchEvent* event) override;
    bool event(QEvent* event) override;

private:
    enum class InputSource { None, Mouse, Touch };

    void drawCrescentMoon(QPainter& p, qreal cx, qreal cy, qreal radius);
    QPoint clampToParent(const QPoint& position) const;
    void beginInteraction(InputSource source, const QPoint& globalPosition);
    void updateInteraction(const QPoint& globalPosition);
    void finishInteraction(const QPoint& globalPosition);
    void cancelInteraction();

    ClickCallback m_ClickCallback;
    bool m_Hovered;
    bool m_ButtonVisible;
    bool m_Dragging;
    InputSource m_InputSource;
    int m_TouchPointId;
    QPoint m_PressGlobalPosition;
    QPoint m_WindowPositionAtPress;
    QRect m_ParentGeometry;

    // Button size (logical pixels)
    static constexpr int kButtonSize = 36;
    static constexpr int kMargin = 10;  // margin from window edge
};
