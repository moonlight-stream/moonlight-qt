#pragma once

#include <QRasterWindow>
#include <QPainter>
#include <QMouseEvent>
#include <QSurfaceFormat>
#include <QTouchEvent>
#include <functional>
#include <memory>
#include <optional>

#include "overlaybuttonposition.h"
#include "overlayeventwakestate.h"

#ifdef Q_OS_DARWIN
class MacOverlayEventMonitor;
#endif

/**
 * OverlayMenuButton - A small floating button rendered by the OS compositor,
 * positioned inside the streaming window. It defaults to the top-right and
 * remembers a device-local relative position after the user drags it.
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
    using EventWakeCallback = std::function<void()>;

    explicit OverlayMenuButton(QWindow* parent = nullptr);
    ~OverlayMenuButton() override;

    void setClickCallback(ClickCallback cb) { m_ClickCallback = std::move(cb); }
    void setEventWakeCallback(EventWakeCallback cb) { m_EventWakeCallback = std::move(cb); }

    /**
     * Reposition the button relative to the given Qt logical parent rect,
     * resolving a remembered relative position against the latest bounds.
     */
    void repositionTo(int parentX, int parentY, int parentW, int parentH);

    /**
     * Show the button inside the given parent rect.
     */
    void showButton(int parentX, int parentY, int parentW, int parentH);

    /**
     * Hide the button.
     */
    void hideButton();

    bool isButtonVisible() const { return m_ButtonVisible; }

    // Windows and macOS wake the SDL loop from native pointer events, so an
    // idle visible button does not need a continuous Qt event pump.
    bool needsEventProcessing() const;
    void beginEventProcessing();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void touchEvent(QTouchEvent* event) override;
    bool event(QEvent* event) override;

private:
#ifdef Q_OS_WIN32
    struct NativeEventMonitor;
#endif

    enum class InputSource { None, Mouse, Touch };

    void drawCrescentMoon(QPainter& p, qreal cx, qreal cy, qreal radius);
    QPoint clampToParent(const QPoint& position) const;
    void beginInteraction(InputSource source, const QPoint& globalPosition);
    void updateInteraction(const QPoint& globalPosition);
    void finishInteraction(const QPoint& globalPosition);
    void cancelInteraction();
    void requestEventProcessing();
    void requestButtonUpdate();
#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN)
    void ensureNativeEventMonitor();
#endif

    ClickCallback m_ClickCallback;
    EventWakeCallback m_EventWakeCallback;
    OverlayEventWakeState m_EventWakeState;
    bool m_Hovered;
    bool m_ButtonVisible;
    bool m_Dragging;
    InputSource m_InputSource;
    int m_TouchPointId;
    QPoint m_PressGlobalPosition;
    QPoint m_WindowPositionAtPress;
    QRect m_ParentGeometry;
    OverlayButtonPositionStore m_PositionStore;
    std::optional<QPointF> m_NormalizedPosition;
#ifdef Q_OS_WIN32
    std::unique_ptr<NativeEventMonitor> m_NativeEventMonitor;
#elif defined(Q_OS_DARWIN)
    std::unique_ptr<MacOverlayEventMonitor> m_NativeEventMonitor;
#endif

    // Button size (logical pixels)
    static constexpr int kButtonSize = 36;
    static constexpr int kMargin = 10;  // margin from window edge
};
