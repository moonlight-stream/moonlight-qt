#include "stylusreplaypanel.h"
#include "uifont.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSurfaceFormat>
#include <QTimer>

namespace {

constexpr int PanelWidth = 520;
constexpr int PanelHeight = 430;
constexpr int Margin = 20;
constexpr int TitleHeight = 52;
constexpr int ButtonHeight = 44;
constexpr int ButtonGap = 12;

const QColor Surface(0x17, 0x1A, 0x20);
const QColor RaisedSurface(0x20, 0x24, 0x2C);
const QColor HoverSurface(0x2A, 0x30, 0x39);
const QColor Line(0x3A, 0x41, 0x4C);
const QColor Accent(0x39, 0xC5, 0xBB);
const QColor Text(0xEE, 0xF0, 0xEC);
const QColor MutedText(0xA4, 0xAB, 0xB4);
const QColor DisabledText(0x65, 0x6C, 0x75);

}

StylusReplayPanel::StylusReplayPanel(QWindow* parent)
    : QRasterWindow(parent),
      m_RecordingSummary(tr("No recording")),
      m_Speed(1),
      m_Progress(-1),
      m_RecordingLoaded(false),
      m_ReplayPlaying(false),
      m_HostSupported(false),
      m_MouseFilterEnabled(true),
      m_HoveredTarget(HitTarget::None),
      m_Dragging(false)
{
    setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setTitle(tr("Function Tests"));
    setMinimumSize(QSize(PanelWidth, PanelHeight));
    setMaximumSize(QSize(PanelWidth, PanelHeight));

    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    setFormat(format);

    m_TitleFont.setFamilies(UiFont::familyChain(QStringLiteral("Manrope")));
    m_TitleFont.setPointSize(13);
    m_TitleFont.setWeight(QFont::DemiBold);

    m_LabelFont = m_TitleFont;
    m_LabelFont.setPointSize(10);

    m_DetailFont = m_LabelFont;
    m_DetailFont.setPointSize(9);
    m_DetailFont.setWeight(QFont::Normal);
}

void StylusReplayPanel::showPanel(const QRect& parentGeometry)
{
    if (isVisible()) {
        raise();
        requestActivate();
        return;
    }

    const int x = parentGeometry.x() + (parentGeometry.width() - PanelWidth) / 2;
    const int y = parentGeometry.y() + (parentGeometry.height() - PanelHeight) / 2;
    setGeometry(x, y, PanelWidth, PanelHeight);
    m_HoveredTarget = HitTarget::None;
    m_Dragging = false;
    show();
    raise();
    requestActivate();
    forceRepaint();

    // Windows may finish activating the SDL window after this callback. Ask
    // again on the next Qt event turn so Escape reliably belongs to the panel.
    QTimer::singleShot(0, this, [this]() {
        if (isVisible()) {
            raise();
            requestActivate();
        }
    });
}

void StylusReplayPanel::closePanel()
{
    if (!isVisible()) {
        return;
    }

    hide();
    m_HoveredTarget = HitTarget::None;
    m_Dragging = false;
    if (m_CloseCallback) {
        m_CloseCallback();
    }
}

void StylusReplayPanel::updateState(const QString& recordingSummary,
                                    const QString& replayStatus,
                                    int speed,
                                    int progress,
                                    bool recordingLoaded,
                                    bool replayPlaying,
                                    bool hostSupported,
                                    bool mouseFilterEnabled)
{
    const bool playbackStateChanged = m_ReplayPlaying != replayPlaying;
    m_RecordingSummary = recordingSummary;
    m_ReplayStatus = replayStatus;
    m_Speed = speed;
    m_Progress = progress;
    m_RecordingLoaded = recordingLoaded;
    m_ReplayPlaying = replayPlaying;
    m_HostSupported = hostSupported;
    m_MouseFilterEnabled = mouseFilterEnabled;
    if (playbackStateChanged) {
        m_HoveredTarget = HitTarget::None;
        setCursor(Qt::ArrowCursor);
    }
    forceRepaint();
}

void StylusReplayPanel::forceRepaint()
{
    if (!isExposed()) {
        return;
    }

    // QRasterWindow updates can otherwise remain queued until another SDL or
    // Qt event arrives. Deliver the update synchronously so button selection
    // and enabled state follow the controller state in the same click.
    update(QRect(0, 0, width(), height()));
    QEvent updateRequest(QEvent::UpdateRequest);
    QCoreApplication::sendEvent(this, &updateRequest);
}

QRect StylusReplayPanel::closeButtonRect() const
{
    return QRect(width() - TitleHeight, 0, TitleHeight, TitleHeight);
}

QRect StylusReplayPanel::chooseButtonRect() const
{
    return QRect(Margin, 158, width() - Margin * 2, ButtonHeight);
}

QRect StylusReplayPanel::replayButtonRect() const
{
    const int buttonWidth = (width() - Margin * 2 - ButtonGap) / 2;
    return QRect(Margin, 214, buttonWidth, ButtonHeight);
}

QRect StylusReplayPanel::stopButtonRect() const
{
    const QRect replay = replayButtonRect();
    return QRect(replay.right() + 1 + ButtonGap, replay.y(), replay.width(), replay.height());
}

QRect StylusReplayPanel::speedButtonRect(int index) const
{
    const int availableWidth = width() - Margin * 2 - ButtonGap * 2;
    const int buttonWidth = availableWidth / 3;
    return QRect(Margin + index * (buttonWidth + ButtonGap), 300,
                 buttonWidth, ButtonHeight);
}

QRect StylusReplayPanel::mouseFilterRect() const
{
    return QRect(Margin, 354, width() - Margin * 2, ButtonHeight);
}

StylusReplayPanel::HitTarget StylusReplayPanel::hitTargetAt(const QPoint& position) const
{
    if (closeButtonRect().contains(position)) return HitTarget::Close;
    if (chooseButtonRect().contains(position)) return HitTarget::ChooseRecording;
    if (replayButtonRect().contains(position)) return HitTarget::StartReplay;
    if (stopButtonRect().contains(position)) return HitTarget::StopReplay;
    if (speedButtonRect(0).contains(position)) return HitTarget::Speed1;
    if (speedButtonRect(1).contains(position)) return HitTarget::Speed2;
    if (speedButtonRect(2).contains(position)) return HitTarget::Speed4;
    if (mouseFilterRect().contains(position)) return HitTarget::MouseFilter;
    return HitTarget::None;
}

bool StylusReplayPanel::isTargetEnabled(HitTarget target) const
{
    switch (target) {
    case HitTarget::ChooseRecording:
        return m_HostSupported && !m_ReplayPlaying;
    case HitTarget::StartReplay:
        return m_HostSupported && m_RecordingLoaded && !m_ReplayPlaying;
    case HitTarget::StopReplay:
        return m_ReplayPlaying;
    case HitTarget::Speed1:
    case HitTarget::Speed2:
    case HitTarget::Speed4:
        return !m_ReplayPlaying;
    case HitTarget::MouseFilter:
        return !m_ReplayPlaying;
    case HitTarget::Close:
        return true;
    case HitTarget::None:
        return false;
    }
    return false;
}

void StylusReplayPanel::dispatch(HitTarget target)
{
    if (target == HitTarget::Close) {
        closePanel();
        return;
    }
    if (target == HitTarget::None || !m_ActionCallback) {
        return;
    }

    switch (target) {
    case HitTarget::ChooseRecording:
        m_ActionCallback(Action::ChooseRecording);
        break;
    case HitTarget::StartReplay:
        m_ActionCallback(Action::StartReplay);
        break;
    case HitTarget::StopReplay:
        m_ActionCallback(Action::StopReplay);
        break;
    case HitTarget::Speed1:
        m_ActionCallback(Action::SetSpeed1);
        break;
    case HitTarget::Speed2:
        m_ActionCallback(Action::SetSpeed2);
        break;
    case HitTarget::Speed4:
        m_ActionCallback(Action::SetSpeed4);
        break;
    case HitTarget::MouseFilter:
        m_ActionCallback(Action::ToggleMouseFilter);
        break;
    case HitTarget::None:
    case HitTarget::Close:
        break;
    }
}

void StylusReplayPanel::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(QRect(0, 0, width(), height()), Surface);
    painter.setPen(QPen(Line, 1));
    painter.drawRect(QRect(0, 0, width() - 1, height() - 1));
    painter.fillRect(QRect(0, 0, 4, height()), Accent);

    painter.setFont(m_TitleFont);
    painter.setPen(Text);
    painter.drawText(QRect(Margin, 0, width() - Margin * 2 - TitleHeight, TitleHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, tr("Function Tests"));

    const QRect closeRect = closeButtonRect();
    if (m_HoveredTarget == HitTarget::Close) {
        painter.fillRect(closeRect, HoverSurface);
    }
    painter.setFont(m_TitleFont);
    painter.drawText(closeRect, Qt::AlignCenter, QString::fromUtf8("\303\227"));
    painter.setPen(Line);
    painter.drawLine(Margin, TitleHeight, width() - Margin, TitleHeight);

    const QRect statusRect(Margin, 66, width() - Margin * 2, 76);
    painter.fillRect(statusRect, RaisedSurface);
    painter.setPen(QPen(Line, 1));
    painter.drawRect(statusRect.adjusted(0, 0, -1, -1));
    painter.setFont(m_LabelFont);
    painter.setPen(Text);
    painter.drawText(statusRect.adjusted(14, 8, -14, -40),
                     Qt::AlignLeft | Qt::AlignVCenter, tr("Stylus Test"));
    painter.setFont(m_DetailFont);
    painter.setPen(m_HostSupported ? MutedText : DisabledText);
    const QString statusText = !m_HostSupported ? tr("The connected host does not support stylus input.") :
                               !m_ReplayStatus.isEmpty() ? m_ReplayStatus : m_RecordingSummary;
    painter.drawText(statusRect.adjusted(14, 34, -14, -8),
                     Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                     statusText);
    if (m_ReplayPlaying && m_Progress >= 0) {
        const QRect progressTrack(statusRect.left() + 14, statusRect.bottom() - 7,
                                  statusRect.width() - 28, 3);
        painter.fillRect(progressTrack, Line);
        painter.fillRect(QRect(progressTrack.topLeft(),
                               QSize(progressTrack.width() * qBound(0, m_Progress, 100) / 100,
                                     progressTrack.height())),
                         Accent);
    }

    const auto drawButton = [&](const QRect& rect, HitTarget target, const QString& label,
                                bool selected = false) {
        const bool enabled = isTargetEnabled(target);
        QColor background = selected ? Accent : RaisedSurface;
        if (enabled && m_HoveredTarget == target) {
            background = selected ? Accent.lighter(110) : HoverSurface;
        }
        painter.fillRect(rect, background);
        painter.setPen(QPen(selected ? Accent.darker(145) : Line, 1));
        painter.drawRect(rect.adjusted(0, 0, -1, -1));
        painter.setFont(m_LabelFont);
        painter.setPen(!enabled ? DisabledText : selected ? Surface : Text);
        painter.drawText(rect, Qt::AlignCenter, label);
    };

    drawButton(chooseButtonRect(), HitTarget::ChooseRecording,
               tr("Choose Recording"));
    drawButton(replayButtonRect(), HitTarget::StartReplay, tr("Start Replay"));
    drawButton(stopButtonRect(), HitTarget::StopReplay, tr("Stop Replay"));

    painter.setFont(m_LabelFont);
    painter.setPen(Text);
    painter.drawText(QRect(Margin, 266, width() - Margin * 2, 24),
                     Qt::AlignLeft | Qt::AlignVCenter, tr("Playback Speed"));
    drawButton(speedButtonRect(0), HitTarget::Speed1, tr("1x"), m_Speed == 1);
    drawButton(speedButtonRect(1), HitTarget::Speed2, tr("2x"), m_Speed == 2);
    drawButton(speedButtonRect(2), HitTarget::Speed4, tr("4x"), m_Speed == 4);

    const QRect filterRect = mouseFilterRect();
    const bool filterEnabled = isTargetEnabled(HitTarget::MouseFilter);
    painter.fillRect(filterRect,
                     filterEnabled && m_HoveredTarget == HitTarget::MouseFilter ?
                         HoverSurface : RaisedSurface);
    painter.setPen(QPen(Line, 1));
    painter.drawRect(filterRect.adjusted(0, 0, -1, -1));
    painter.setFont(m_LabelFont);
    painter.setPen(filterEnabled ? Text : DisabledText);
    painter.drawText(filterRect.adjusted(14, 0, -70, 0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("Filter Local Mouse Input During Replay"));
    const QRect toggleTrack(filterRect.right() - 52, filterRect.center().y() - 9, 38, 18);
    painter.fillRect(toggleTrack, m_MouseFilterEnabled ? Accent : Line);
    const int knobX = m_MouseFilterEnabled ? toggleTrack.right() - 14 : toggleTrack.left() + 3;
    painter.fillRect(QRect(knobX, toggleTrack.top() + 3, 12, 12),
                     m_MouseFilterEnabled ? Surface : MutedText);

    painter.setFont(m_DetailFont);
    painter.setPen(MutedText);
    painter.drawText(QRect(Margin, 404, width() - Margin * 2, 20),
                     Qt::AlignRight | Qt::AlignVCenter, tr("Press Esc to close"));
}

void StylusReplayPanel::mouseMoveEvent(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint globalPosition = event->globalPosition().toPoint();
#else
    const QPoint globalPosition = event->globalPos();
#endif
    if (m_Dragging) {
        if (!(event->buttons() & Qt::LeftButton)) {
            m_Dragging = false;
        }
        else {
            setPosition(m_DragStartWindowPosition + globalPosition - m_DragStartGlobal);
            return;
        }
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const HitTarget target = hitTargetAt(event->position().toPoint());
#else
    const HitTarget target = hitTargetAt(event->pos());
#endif
    if (target != m_HoveredTarget) {
        m_HoveredTarget = target;
        setCursor(target == HitTarget::None ? Qt::ArrowCursor : Qt::PointingHandCursor);
        forceRepaint();
    }
}

void StylusReplayPanel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint position = event->position().toPoint();
#else
    const QPoint position = event->pos();
#endif
    const HitTarget target = hitTargetAt(position);
    if (target == HitTarget::None && position.y() < TitleHeight) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_DragStartGlobal = event->globalPosition().toPoint();
#else
        m_DragStartGlobal = event->globalPos();
#endif
        m_DragStartWindowPosition = this->position();
        m_Dragging = true;
        return;
    }
    if (!isTargetEnabled(target)) {
        return;
    }
    dispatch(target);
}

void StylusReplayPanel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_Dragging = false;
    }
}

void StylusReplayPanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        event->accept();
        closePanel();
        return;
    }
    QRasterWindow::keyPressEvent(event);
}

void StylusReplayPanel::closeEvent(QCloseEvent* event)
{
    // The panel intentionally supports only its own close button and Escape.
    // In particular, Alt+F4 must not add another local shortcut while testing
    // streaming keyboard behavior.
    event->ignore();
}
