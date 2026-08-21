#pragma once

#include <QRect>
#include <QString>
#include <QtGlobal>

class QScreen;
class QWindow;

namespace WindowsDisplayGeometry {

// Snapshot in native physical desktop coordinates. Native monitor handles are
// intentionally not retained because they become invalid after display changes.
struct Monitor
{
    QRect bounds;
    QRect workArea;
    QString name;

    bool isValid() const;
};

QScreen* screenForName(const QString& name);
QScreen* screenForMonitor(const Monitor& monitor);

bool monitorForName(const QString& name, Monitor& monitor);
bool monitorForScreen(QScreen* screen, Monitor& monitor);
bool monitorForWindow(QWindow* window, Monitor& monitor);
bool monitorForRect(const QRect& rect, Monitor& monitor);

QRect windowRect(QWindow* window);
bool clientRectForOverlappedWindow(QWindow* referenceWindow,
                                   const QRect& frameRect,
                                   QRect& clientRect);
bool setWindowRect(QWindow* window, const QRect& geometry, quint32* errorCode = nullptr);
bool isNormalWindow(QWindow* window);

qreal scaleFactor(const Monitor& monitor, QScreen* preferredScreen = nullptr);

} // namespace WindowsDisplayGeometry
