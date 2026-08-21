#include "windowplacement.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QWindow>

namespace {
constexpr auto GeometryKey = "mainwindow/geometry";
constexpr auto ScreenNameKey = "mainwindow/screen";
constexpr int OversizedWindowInset = 32;

int constrainedLength(int requested, int available)
{
    if (available <= 0) {
        return qMax(1, requested);
    }

    if (requested >= available) {
        return qMax(1, available > OversizedWindowInset
                           ? available - OversizedWindowInset
                           : available);
    }

    return qMax(1, requested);
}
}

WindowPlacement::WindowPlacement(QObject* parent)
    : QObject(parent)
{
    m_SaveTimer.setInterval(300);
    m_SaveTimer.setSingleShot(true);
    connect(&m_SaveTimer, &QTimer::timeout, this, &WindowPlacement::saveNow);
}

QWindow* WindowPlacement::window() const
{
    return m_Window;
}

void WindowPlacement::setWindow(QWindow* window)
{
    if (m_Window == window) {
        return;
    }

    m_SaveTimer.stop();
    if (m_Window) {
        disconnect(m_Window, nullptr, this, nullptr);
    }

    m_Window = window;
    if (m_Window) {
        connect(m_Window, &QWindow::xChanged, this, &WindowPlacement::scheduleSave);
        connect(m_Window, &QWindow::yChanged, this, &WindowPlacement::scheduleSave);
        connect(m_Window, &QWindow::widthChanged, this, &WindowPlacement::scheduleSave);
        connect(m_Window, &QWindow::heightChanged, this, &WindowPlacement::scheduleSave);
        connect(m_Window, &QWindow::screenChanged, this, &WindowPlacement::scheduleSave);
        connect(m_Window, &QWindow::visibilityChanged, this, [this](QWindow::Visibility visibility) {
            if (visibility == QWindow::Windowed) {
                scheduleSave();
            }
            else {
                m_SaveTimer.stop();
            }
        });
    }

    emit windowChanged();
}

bool WindowPlacement::isEnabled() const
{
    return m_Enabled;
}

void WindowPlacement::setEnabled(bool enabled)
{
    if (m_Enabled == enabled) {
        return;
    }

    m_Enabled = enabled;
    if (m_Enabled) {
        scheduleSave();
    }
    else {
        m_SaveTimer.stop();
    }

    emit enabledChanged();
}

void WindowPlacement::restore()
{
    if (!m_Window) {
        return;
    }

    QSettings settings;
    const QRect savedGeometry = settings.value(GeometryKey).toRect();
    const bool hasSavedGeometry = m_Enabled && savedGeometry.isValid();

    QScreen* screen = hasSavedGeometry
            ? findSavedScreen(settings.value(ScreenNameKey).toString(), savedGeometry)
            : m_Window->screen();
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }

    m_Restoring = true;
    if (hasSavedGeometry) {
        m_Window->setScreen(screen);
        m_Window->setGeometry(constrainGeometry(savedGeometry, screen->availableGeometry()));
    }
    else {
        const QRect availableGeometry = screen->availableGeometry();
        const QSize requestedSize = m_Window->size();
        m_Window->resize(constrainedLength(requestedSize.width(), availableGeometry.width()),
                         constrainedLength(requestedSize.height(), availableGeometry.height()));
    }
    m_Restoring = false;
}

void WindowPlacement::flush()
{
    m_SaveTimer.stop();
    saveNow();
}

QScreen* WindowPlacement::findSavedScreen(const QString& name, const QRect& geometry)
{
    const auto screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen->name() == name) {
            return screen;
        }
    }

    if (QScreen* screen = QGuiApplication::screenAt(geometry.center())) {
        return screen;
    }

    return QGuiApplication::primaryScreen();
}

QRect WindowPlacement::constrainGeometry(const QRect& geometry, const QRect& availableGeometry)
{
    if (!availableGeometry.isValid()) {
        return geometry;
    }

    QRect result = geometry;
    result.setWidth(constrainedLength(result.width(), availableGeometry.width()));
    result.setHeight(constrainedLength(result.height(), availableGeometry.height()));

    const int maximumX = availableGeometry.right() - result.width() + 1;
    const int maximumY = availableGeometry.bottom() - result.height() + 1;
    result.moveLeft(qBound(availableGeometry.left(), result.left(), maximumX));
    result.moveTop(qBound(availableGeometry.top(), result.top(), maximumY));
    return result;
}

void WindowPlacement::scheduleSave()
{
    if (m_Enabled && !m_Restoring && m_Window &&
            m_Window->visibility() == QWindow::Windowed) {
        m_SaveTimer.start();
    }
}

void WindowPlacement::saveNow()
{
    if (!m_Enabled || m_Restoring || !m_Window ||
            m_Window->visibility() != QWindow::Windowed ||
            !m_Window->geometry().isValid()) {
        return;
    }

    QScreen* screen = QGuiApplication::screenAt(m_Window->geometry().center());
    if (!screen) {
        screen = m_Window->screen();
    }

    QSettings settings;
    settings.setValue(GeometryKey, m_Window->geometry());
    if (screen) {
        settings.setValue(ScreenNameKey, screen->name());
    }
    settings.sync();
}
