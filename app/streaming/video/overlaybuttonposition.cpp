#include "overlaybuttonposition.h"

#include "settings/devicelocalsettings.h"

#include <QtGlobal>

namespace {
constexpr auto LocalStateFileName = "overlay-button-placement.ini";
constexpr auto LocalStateVersionKey = "meta/version";
constexpr auto NormalizedXKey = "position/normalizedX";
constexpr auto NormalizedYKey = "position/normalizedY";
constexpr int LocalStateVersion = 1;

struct AxisBounds
{
    int start;
    int end;
};

AxisBounds axisBounds(int origin, int parentLength, int buttonLength, int margin)
{
    const int movableLength = qMax(0, parentLength - buttonLength);
    const int safeMargin = qMin(qMax(0, margin), movableLength / 2);
    return {origin + safeMargin, origin + movableLength - safeMargin};
}

int resolveAxis(qreal normalized, const AxisBounds& bounds)
{
    const qreal safeNormalized = qIsFinite(normalized)
            ? qBound<qreal>(0.0, normalized, 1.0)
            : 0.5;
    return qRound(bounds.start + (bounds.end - bounds.start) * safeNormalized);
}

qreal normalizeAxis(int position, const AxisBounds& bounds)
{
    const int length = bounds.end - bounds.start;
    if (length <= 0) {
        return 0.5;
    }
    return qBound<qreal>(0.0,
                         static_cast<qreal>(position - bounds.start) / length,
                         1.0);
}
}

QPoint OverlayButtonPlacement::defaultPosition(const QRect& parentGeometry,
                                               const QSize& buttonSize,
                                               int margin)
{
    return resolve(QPointF(1.0, 0.0), parentGeometry, buttonSize, margin);
}

QPoint OverlayButtonPlacement::clamp(const QPoint& position,
                                     const QRect& parentGeometry,
                                     const QSize& buttonSize,
                                     int margin)
{
    if (!parentGeometry.isValid() || !buttonSize.isValid()) {
        return position;
    }

    const AxisBounds horizontal = axisBounds(parentGeometry.x(),
                                             parentGeometry.width(),
                                             buttonSize.width(),
                                             margin);
    const AxisBounds vertical = axisBounds(parentGeometry.y(),
                                           parentGeometry.height(),
                                           buttonSize.height(),
                                           margin);
    return QPoint(qBound(horizontal.start, position.x(), horizontal.end),
                  qBound(vertical.start, position.y(), vertical.end));
}

QPoint OverlayButtonPlacement::resolve(const QPointF& normalizedPosition,
                                       const QRect& parentGeometry,
                                       const QSize& buttonSize,
                                       int margin)
{
    if (!parentGeometry.isValid() || !buttonSize.isValid()) {
        return parentGeometry.topLeft();
    }

    const AxisBounds horizontal = axisBounds(parentGeometry.x(),
                                             parentGeometry.width(),
                                             buttonSize.width(),
                                             margin);
    const AxisBounds vertical = axisBounds(parentGeometry.y(),
                                           parentGeometry.height(),
                                           buttonSize.height(),
                                           margin);
    return QPoint(resolveAxis(normalizedPosition.x(), horizontal),
                  resolveAxis(normalizedPosition.y(), vertical));
}

QPointF OverlayButtonPlacement::normalize(const QPoint& position,
                                          const QRect& parentGeometry,
                                          const QSize& buttonSize,
                                          int margin)
{
    if (!parentGeometry.isValid() || !buttonSize.isValid()) {
        return QPointF(0.5, 0.5);
    }

    const AxisBounds horizontal = axisBounds(parentGeometry.x(),
                                             parentGeometry.width(),
                                             buttonSize.width(),
                                             margin);
    const AxisBounds vertical = axisBounds(parentGeometry.y(),
                                           parentGeometry.height(),
                                           buttonSize.height(),
                                           margin);
    const QPoint constrained = clamp(position, parentGeometry, buttonSize, margin);
    return QPointF(normalizeAxis(constrained.x(), horizontal),
                   normalizeAxis(constrained.y(), vertical));
}

OverlayButtonPositionStore::OverlayButtonPositionStore(const QString& settingsPath)
    : m_Settings(settingsPath.isEmpty()
                         ? DeviceLocalSettings::filePath(
                                   QLatin1String(LocalStateFileName),
                                   "MOONLIGHT_OVERLAY_BUTTON_PLACEMENT_DIR")
                         : settingsPath,
                 QSettings::IniFormat)
{
}

std::optional<QPointF> OverlayButtonPositionStore::load() const
{
    if (m_Settings.value(QLatin1String(LocalStateVersionKey), 0).toInt() !=
            LocalStateVersion) {
        return std::nullopt;
    }

    bool xValid = false;
    bool yValid = false;
    const qreal x = m_Settings.value(QLatin1String(NormalizedXKey)).toDouble(&xValid);
    const qreal y = m_Settings.value(QLatin1String(NormalizedYKey)).toDouble(&yValid);
    if (!xValid || !yValid || !qIsFinite(x) || !qIsFinite(y)) {
        return std::nullopt;
    }

    return QPointF(qBound<qreal>(0.0, x, 1.0),
                   qBound<qreal>(0.0, y, 1.0));
}

bool OverlayButtonPositionStore::save(const QPointF& normalizedPosition)
{
    if (!qIsFinite(normalizedPosition.x()) || !qIsFinite(normalizedPosition.y())) {
        return false;
    }

    m_Settings.setValue(QLatin1String(LocalStateVersionKey), LocalStateVersion);
    m_Settings.setValue(QLatin1String(NormalizedXKey),
                        qBound<qreal>(0.0, normalizedPosition.x(), 1.0));
    m_Settings.setValue(QLatin1String(NormalizedYKey),
                        qBound<qreal>(0.0, normalizedPosition.y(), 1.0));
    m_Settings.sync();
    return m_Settings.status() == QSettings::NoError;
}
