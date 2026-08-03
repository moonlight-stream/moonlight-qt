#pragma once

#include <QString>
#include <QStringList>

namespace UiFont
{

inline QStringList systemHanFallbackFamilies()
{
#ifdef Q_OS_DARWIN
    return {QStringLiteral("PingFang SC")};
#elif defined(Q_OS_WIN32)
    return {
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei"),
        QStringLiteral("Noto Sans SC"),
        QStringLiteral("DengXian"),
    };
#else
    return {
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Source Han Sans SC"),
    };
#endif
}

inline QStringList familyChain(const QString& primaryFamily)
{
    QStringList families;
    if (!primaryFamily.isEmpty()) {
        families << primaryFamily;
    }
    families << systemHanFallbackFamilies();
    return families;
}

} // namespace UiFont
