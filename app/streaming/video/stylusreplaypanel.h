#pragma once

#include <QRasterWindow>
#include <QFont>
#include <QRect>
#include <QString>

#include <functional>
#include <utility>

class QCloseEvent;
class QKeyEvent;
class QMouseEvent;

/**
 * Fixed-size developer panel for controlling a stylus recording replay.
 *
 * StylusReplayTest owns exactly one instance. Hiding and showing the panel
 * reuses that instance, so opening the function test repeatedly cannot create
 * duplicate panels or replay controllers.
 */
class StylusReplayPanel final : public QRasterWindow
{
    Q_OBJECT

public:
    enum class Action {
        ChooseRecording,
        StartReplay,
        StopReplay,
        SetSpeed1,
        SetSpeed2,
        SetSpeed4,
        ToggleMouseFilter,
    };

    using ActionCallback = std::function<void(Action)>;
    using CloseCallback = std::function<void()>;

    explicit StylusReplayPanel(QWindow* parent = nullptr);

    void setActionCallback(ActionCallback callback) { m_ActionCallback = std::move(callback); }
    void setCloseCallback(CloseCallback callback) { m_CloseCallback = std::move(callback); }

    void showPanel(const QRect& parentGeometry);
    void closePanel();

    void updateState(const QString& recordingSummary,
                     const QString& replayStatus,
                     int speed,
                     int progress,
                     bool recordingLoaded,
                     bool replayPlaying,
                     bool hostSupported,
                     bool mouseFilterEnabled);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    enum class HitTarget {
        None,
        Close,
        ChooseRecording,
        StartReplay,
        StopReplay,
        Speed1,
        Speed2,
        Speed4,
        MouseFilter,
    };

    QRect closeButtonRect() const;
    QRect chooseButtonRect() const;
    QRect replayButtonRect() const;
    QRect stopButtonRect() const;
    QRect speedButtonRect(int index) const;
    QRect mouseFilterRect() const;
    HitTarget hitTargetAt(const QPoint& position) const;
    bool isTargetEnabled(HitTarget target) const;
    void dispatch(HitTarget target);
    void forceRepaint();

    ActionCallback m_ActionCallback;
    CloseCallback m_CloseCallback;

    QString m_RecordingSummary;
    QString m_ReplayStatus;
    int m_Speed;
    int m_Progress;
    bool m_RecordingLoaded;
    bool m_ReplayPlaying;
    bool m_HostSupported;
    bool m_MouseFilterEnabled;
    HitTarget m_HoveredTarget;
    bool m_Dragging;
    QPoint m_DragStartGlobal;
    QPoint m_DragStartWindowPosition;

    QFont m_TitleFont;
    QFont m_LabelFont;
    QFont m_DetailFont;
};
