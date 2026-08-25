#include "windowplacement.h"
#include "windowsdisplaygeometry.h"
#include "settings/devicelocalsettings.h"

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QStringList>
#include <QWindow>

namespace {
constexpr auto GeometryKey = "mainwindow/geometry";
constexpr auto WindowsOuterGeometryKey = "mainwindow/windowsOuterGeometry";
constexpr auto WindowsScreenNameKey = "mainwindow/windowsScreen";
constexpr auto ScreenNameKey = "mainwindow/screen";
constexpr auto MaximizedKey = "mainwindow/maximized";
constexpr auto LocalStateVersionKey = "meta/version";
constexpr auto LocalStateFileName = "window-placement.ini";
constexpr int LocalStateVersion = 1;

const QStringList& placementStateKeys()
{
    static const QStringList keys {
        QLatin1String(GeometryKey),
        QLatin1String(WindowsOuterGeometryKey),
        QLatin1String(WindowsScreenNameKey),
        QLatin1String(ScreenNameKey),
        QLatin1String(MaximizedKey),
    };
    return keys;
}

void migrateLegacyPlacementSettings(QSettings& localSettings)
{
    if (localSettings.value(QLatin1String(LocalStateVersionKey), 0).toInt() >=
            LocalStateVersion) {
        return;
    }

    QSettings globalSettings;
    bool hasLegacyState = false;
    for (const QString& key : placementStateKeys()) {
        if (!globalSettings.contains(key)) {
            continue;
        }

        hasLegacyState = true;
        if (!localSettings.contains(key)) {
            localSettings.setValue(key, globalSettings.value(key));
        }
    }

    localSettings.sync();
    if (localSettings.status() != QSettings::NoError) {
        qWarning() << "Failed to migrate device-local window placement state";
        return;
    }

    if (hasLegacyState) {
        for (const QString& key : placementStateKeys()) {
            globalSettings.remove(key);
        }
        globalSettings.sync();
        if (globalSettings.status() != QSettings::NoError) {
            qWarning() << "Failed to remove migrated window placement state from main settings";
            return;
        }
        qInfo() << "Migrated window placement state to device-local storage";
    }

    localSettings.setValue(QLatin1String(LocalStateVersionKey), LocalStateVersion);
    localSettings.sync();
    if (localSettings.status() != QSettings::NoError) {
        qWarning() << "Failed to finalize the device-local window placement migration";
    }
}

const char* visibilityName(QWindow::Visibility visibility)
{
    switch (visibility) {
    case QWindow::Hidden: return "hidden";
    case QWindow::AutomaticVisibility: return "automatic";
    case QWindow::Windowed: return "windowed";
    case QWindow::Minimized: return "minimized";
    case QWindow::Maximized: return "maximized";
    case QWindow::FullScreen: return "fullscreen";
    }
    return "unknown";
}

int constrainedLength(int requested, int available)
{
    if (available <= 0) {
        return qMax(1, requested);
    }

    if (requested > available) {
        return available;
    }

    return qMax(1, requested);
}

QRect constrainedGeometry(const QRect& geometry,
                          const QRect& availableGeometry)
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

#ifdef Q_OS_WIN32
QString rectText(const QRect& rect)
{
    return QStringLiteral("[%1,%2 %3x%4]")
            .arg(rect.x())
            .arg(rect.y())
            .arg(rect.width())
            .arg(rect.height());
}

QRect relativeLogicalToNative(const QRect& logicalGeometry,
                              const WindowsDisplayGeometry::Monitor& monitor,
                              qreal scale)
{
    return QRect(monitor.bounds.left() + qRound(logicalGeometry.left() * scale),
                 monitor.bounds.top() + qRound(logicalGeometry.top() * scale),
                 qMax(1, qRound(logicalGeometry.width() * scale)),
                 qMax(1, qRound(logicalGeometry.height() * scale)));
}

QRect nativeToRelativeLogical(const QRect& nativeGeometry,
                              const WindowsDisplayGeometry::Monitor& monitor,
                              qreal scale)
{
    if (scale <= 0) {
        scale = 1.0;
    }

    return QRect(qRound((nativeGeometry.left() - monitor.bounds.left()) / scale),
                 qRound((nativeGeometry.top() - monitor.bounds.top()) / scale),
                 qMax(1, qRound(nativeGeometry.width() / scale)),
                 qMax(1, qRound(nativeGeometry.height() / scale)));
}
#endif
}

WindowPlacement::WindowPlacement(QObject* parent)
    : QObject(parent),
      m_PlacementSettings(DeviceLocalSettings::filePath(
                                  QLatin1String(LocalStateFileName),
                                  "MOONLIGHT_WINDOW_PLACEMENT_DIR"),
                          QSettings::IniFormat)
{
    migrateLegacyPlacementSettings(m_PlacementSettings);
    m_SaveTimer.setInterval(300);
    m_SaveTimer.setSingleShot(true);
    connect(&m_SaveTimer, &QTimer::timeout, this, &WindowPlacement::saveNow);
    if (qGuiApp) {
        connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen*) {
            scheduleSave();
        });
    }
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
            saveWindowState(visibility);
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
        if (m_Window) {
            saveWindowState(m_Window->visibility());
        }
    }
    else {
        m_SaveTimer.stop();
    }

    emit enabledChanged();
}

bool WindowPlacement::restore(bool defaultMaximized)
{
    const bool hasSavedMaximized = m_Enabled &&
            m_PlacementSettings.contains(QLatin1String(MaximizedKey));
    const bool savedMaximized = hasSavedMaximized &&
            m_PlacementSettings.value(QLatin1String(MaximizedKey)).toBool();
    const bool selectedMaximized = hasSavedMaximized
            ? savedMaximized
            : defaultMaximized;

    qInfo().noquote()
            << QStringLiteral("Window placement state restore saved=%1 fallbackMaximized=%2 "
                              "selectedMaximized=%3")
                       .arg(hasSavedMaximized
                                    ? (savedMaximized ? QStringLiteral("maximized")
                                                      : QStringLiteral("windowed"))
                                    : QStringLiteral("none"))
                       .arg(defaultMaximized ? 1 : 0)
                       .arg(selectedMaximized ? 1 : 0);

    if (!m_Window) {
        return selectedMaximized;
    }

    const QRect savedGeometry = m_PlacementSettings.value(
            QLatin1String(GeometryKey)).toRect();
#ifdef Q_OS_WIN32
    const QRect savedWindowsGeometry = m_PlacementSettings.value(
            QLatin1String(WindowsOuterGeometryKey)).toRect();
    const bool hasSavedWindowsGeometry = m_Enabled && savedWindowsGeometry.isValid();
#else
    const bool hasSavedGeometry = m_Enabled && savedGeometry.isValid();
#endif

#ifdef Q_OS_WIN32
    const bool hasLegacyGeometry = m_Enabled && !hasSavedWindowsGeometry && savedGeometry.isValid();
    QScreen* screen = m_Window->screen();
    if (hasSavedWindowsGeometry) {
        if (QScreen* savedScreen = WindowsDisplayGeometry::screenForName(
                    m_PlacementSettings.value(QLatin1String(ScreenNameKey)).toString())) {
            screen = savedScreen;
        }
    }
    else if (hasLegacyGeometry) {
        screen = findSavedScreen(m_PlacementSettings.value(
                                     QLatin1String(ScreenNameKey)).toString(),
                                 savedGeometry);
    }
#else
    QScreen* screen = hasSavedGeometry
            ? findSavedScreen(m_PlacementSettings.value(
                                  QLatin1String(ScreenNameKey)).toString(),
                              savedGeometry)
            : m_Window->screen();
#endif
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return selectedMaximized;
    }

    m_Restoring = true;
#ifdef Q_OS_WIN32
    WindowsDisplayGeometry::Monitor nativeMonitor;
    const QString savedNativeScreenName = m_PlacementSettings.value(
            QLatin1String(WindowsScreenNameKey)).toString();
    if (!WindowsDisplayGeometry::monitorForName(savedNativeScreenName, nativeMonitor) &&
            !WindowsDisplayGeometry::monitorForScreen(screen, nativeMonitor)) {
        WindowsDisplayGeometry::monitorForWindow(m_Window, nativeMonitor);
    }

    if (QScreen* matchingScreen = WindowsDisplayGeometry::screenForMonitor(nativeMonitor)) {
        screen = matchingScreen;
    }

    const QRect currentWindowGeometry = WindowsDisplayGeometry::windowRect(m_Window);
    if (nativeMonitor.isValid() && currentWindowGeometry.isValid()) {
        const qreal scale = WindowsDisplayGeometry::scaleFactor(nativeMonitor, screen);
        QRect requestedNativeGeometry = currentWindowGeometry;
        QRect logicalGeometry;

        if (hasSavedWindowsGeometry) {
            logicalGeometry = savedWindowsGeometry;
            requestedNativeGeometry = relativeLogicalToNative(logicalGeometry, nativeMonitor, scale);
        }
        else if (hasLegacyGeometry) {
            logicalGeometry = savedGeometry.translated(-screen->geometry().topLeft());
            requestedNativeGeometry = relativeLogicalToNative(logicalGeometry, nativeMonitor, scale);
        }

        const QRect constrainedNativeGeometry = constrainedGeometry(
                requestedNativeGeometry, nativeMonitor.workArea);

        quint32 errorCode = 0;
        if (!WindowsDisplayGeometry::setWindowRect(
                    m_Window, constrainedNativeGeometry, &errorCode)) {
            qWarning() << "Failed to restore native window geometry:" << errorCode;
        }

        const QRect restoredWindowGeometry = WindowsDisplayGeometry::windowRect(m_Window);
        qInfo().noquote()
                << QStringLiteral("Window placement restore screen=%1 saved=%2 requestedNative=%3 "
                                  "constrainedNative=%4 actualNative=%5 scale=%6")
                           .arg(nativeMonitor.name,
                                logicalGeometry.isValid() ? rectText(logicalGeometry)
                                                          : QStringLiteral("default"),
                                rectText(requestedNativeGeometry),
                                rectText(constrainedNativeGeometry),
                                rectText(restoredWindowGeometry))
                           .arg(scale);
    }
    else {
        const QRect availableGeometry = screen->availableGeometry();
        if (hasLegacyGeometry) {
            m_Window->setScreen(screen);
            m_Window->setGeometry(constrainGeometry(savedGeometry, availableGeometry));
        }
        else {
            const QSize requestedSize = m_Window->size();
            m_Window->resize(constrainedLength(requestedSize.width(), availableGeometry.width()),
                             constrainedLength(requestedSize.height(), availableGeometry.height()));
        }
    }
#else
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
#endif
    m_Restoring = false;
    return selectedMaximized;
}

void WindowPlacement::flush()
{
    m_SaveTimer.stop();
    if (m_Window) {
        saveWindowState(m_Window->visibility());
    }
    saveNow();
}

QScreen* WindowPlacement::findSavedScreen(const QString& name, const QRect& geometry)
{
    if (QScreen* screen = WindowsDisplayGeometry::screenForName(name)) {
        return screen;
    }

    if (QScreen* screen = QGuiApplication::screenAt(geometry.center())) {
        return screen;
    }

    return QGuiApplication::primaryScreen();
}

QRect WindowPlacement::constrainGeometry(const QRect& geometry, const QRect& availableGeometry)
{
    return constrainedGeometry(geometry, availableGeometry);
}

void WindowPlacement::scheduleSave()
{
    if (m_Enabled && !m_Restoring && m_Window &&
            m_Window->visibility() == QWindow::Windowed) {
        m_SaveTimer.start();
    }
}

void WindowPlacement::saveWindowState(QWindow::Visibility visibility)
{
    if (!m_Enabled || m_Restoring || !m_Window) {
        return;
    }

    bool maximized;
    if (visibility == QWindow::Windowed) {
        maximized = false;
    }
    else if (visibility == QWindow::Maximized) {
        maximized = true;
    }
    else {
        // Minimized, fullscreen, hidden, and startup transitions are not stable
        // restore targets. Keep the last normal/maximized state instead.
        return;
    }

    if (m_PlacementSettings.contains(QLatin1String(MaximizedKey)) &&
            m_PlacementSettings.value(QLatin1String(MaximizedKey)).toBool() == maximized) {
        return;
    }
    m_PlacementSettings.setValue(QLatin1String(MaximizedKey), maximized);
    m_PlacementSettings.sync();
    if (m_PlacementSettings.status() != QSettings::NoError) {
        qWarning() << "Failed to save device-local window state";
        return;
    }
    qInfo().noquote()
            << QStringLiteral("Window placement state save maximized=%1 visibility=%2")
                       .arg(maximized ? 1 : 0)
                       .arg(QString::fromLatin1(visibilityName(visibility)));
}

void WindowPlacement::saveNow()
{
    if (!m_Enabled || m_Restoring || !m_Window ||
            m_Window->visibility() != QWindow::Windowed ||
            !m_Window->geometry().isValid()) {
        return;
    }

#ifdef Q_OS_WIN32
    if (!WindowsDisplayGeometry::isNormalWindow(m_Window)) {
        return;
    }

    const QRect nativeGeometry = WindowsDisplayGeometry::windowRect(m_Window);
    WindowsDisplayGeometry::Monitor nativeMonitor;
    if (!nativeGeometry.isValid() ||
            !WindowsDisplayGeometry::monitorForWindow(m_Window, nativeMonitor)) {
        return;
    }

    QScreen* screen = WindowsDisplayGeometry::screenForMonitor(nativeMonitor);
    if (!screen) {
        screen = m_Window->screen();
    }
    if (!screen) {
        screen = QGuiApplication::screenAt(m_Window->geometry().center());
    }
    const qreal scale = WindowsDisplayGeometry::scaleFactor(nativeMonitor, screen);
    const QRect logicalGeometry = nativeToRelativeLogical(
            nativeGeometry, nativeMonitor, scale);

    m_PlacementSettings.setValue(QLatin1String(WindowsOuterGeometryKey), logicalGeometry);
    m_PlacementSettings.setValue(QLatin1String(WindowsScreenNameKey), nativeMonitor.name);
    m_PlacementSettings.setValue(QLatin1String(ScreenNameKey),
                                 screen ? screen->name() : nativeMonitor.name);
    m_PlacementSettings.remove(QLatin1String(GeometryKey));
    qInfo().noquote()
            << QStringLiteral("Window placement save screen=%1 native=%2 relativeLogical=%3 scale=%4")
                       .arg(nativeMonitor.name,
                            rectText(nativeGeometry),
                            rectText(logicalGeometry))
                       .arg(scale);
#else
    QScreen* screen = QGuiApplication::screenAt(m_Window->geometry().center());
    if (!screen) {
        screen = m_Window->screen();
    }

    m_PlacementSettings.setValue(QLatin1String(GeometryKey), m_Window->geometry());
    if (screen) {
        m_PlacementSettings.setValue(QLatin1String(ScreenNameKey), screen->name());
    }
#endif
    m_PlacementSettings.sync();
    if (m_PlacementSettings.status() != QSettings::NoError) {
        qWarning() << "Failed to save device-local window geometry";
    }
}
