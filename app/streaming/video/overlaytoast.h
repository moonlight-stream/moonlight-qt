#pragma once

#include <QRasterWindow>
#include <QPainter>
#include <QFont>
#include <QElapsedTimer>
#include <QSurfaceFormat>
#include <QPropertyAnimation>

#include "overlaytoasteventstate.h"

/**
 * OverlayToast - lightweight, auto-dismissing toast notification
 * rendered by the OS compositor (DWM), independent of D3D11 pipeline.
 *
 * Shows a brief message at the bottom-center of the streaming window,
 * then fades out and hides itself after a configurable duration.
 */
class OverlayToast : public QRasterWindow {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    explicit OverlayToast(QWindow* parent = nullptr);
    ~OverlayToast() override;

    /**
     * Show a toast message centered at the bottom of the given parent rect.
     * @param parentX/Y/W/H  Qt global logical geometry of the streaming window
     * @param message         Text to display
     * @param durationMs      How long to show before fading out (default 2000ms)
     */
    void showToast(int parentX, int parentY, int parentW, int parentH,
                   const QString& message, int durationMs = 2000);

    // Static toast contents do not require a continuously running Qt event
    // loop. Session uses these methods to wake only for initial paint,
    // dismissal deadline, and the short fade animation.
    bool needsEventProcessing() const;
    int nextEventDelayMs() const;
    void beginEventProcessing();
    void dismissImmediately();

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void startFadeOut();
    void onFadeFinished();

private:
    QString m_Message;
    QFont   m_Font;
    QElapsedTimer m_Clock;
    OverlayToastEventState m_EventState;
    QPropertyAnimation* m_FadeAnimation;
    int m_ToastHeight;
    int m_HorizPadding;
    int m_VertPadding;
};
