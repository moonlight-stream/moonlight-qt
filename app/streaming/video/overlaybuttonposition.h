#pragma once

#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSettings>
#include <QSize>

#include <optional>

class OverlayButtonPlacement
{
public:
    static QPoint defaultPosition(const QRect& parentGeometry,
                                  const QSize& buttonSize,
                                  int margin);
    static QPoint clamp(const QPoint& position,
                        const QRect& parentGeometry,
                        const QSize& buttonSize,
                        int margin);
    static QPoint resolve(const QPointF& normalizedPosition,
                          const QRect& parentGeometry,
                          const QSize& buttonSize,
                          int margin);
    static QPointF normalize(const QPoint& position,
                             const QRect& parentGeometry,
                             const QSize& buttonSize,
                             int margin);
};

/** Device-local exact position; intentionally excluded from configuration sync. */
class OverlayButtonPositionStore
{
public:
    explicit OverlayButtonPositionStore(const QString& settingsPath = QString());

    std::optional<QPointF> load() const;
    bool save(const QPointF& normalizedPosition);

private:
    QSettings m_Settings;
};
