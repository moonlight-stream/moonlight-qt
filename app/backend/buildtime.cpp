#include "systemproperties.h"

#include <QDate>
#include <QLocale>

QString SystemProperties::buildTimeString() const
{
    // The compiler uses English month names and pads single-digit days.
    const QDate buildDate = QLocale::c().toDate(QStringLiteral(__DATE__).simplified(),
                                               QStringLiteral("MMM d yyyy"));
    return buildDate.toString(Qt::ISODate) + QStringLiteral(" " __TIME__);
}
