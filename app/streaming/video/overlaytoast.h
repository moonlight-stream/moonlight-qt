#pragma once

#include <QRasterWindow>
#include <QPainter>
#include <QFont>
#include <QTimer>
#include <QSurfaceFormat>
#include <QPropertyAnimation>

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

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void startFadeOut();
    void onFadeFinished();

private:
    QString m_Message;
    QFont   m_Font;
    QTimer  m_DismissTimer;
    QPropertyAnimation* m_FadeAnimation;
    int m_ToastHeight;
    int m_HorizPadding;
    int m_VertPadding;
};
