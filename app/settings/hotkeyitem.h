#pragma once

#include <QObject>
#include <QSettings>

typedef QMap<QString, QVariant> MapIntString2HotkeyInfo;
Q_DECLARE_METATYPE(MapIntString2HotkeyInfo)

class HotkeyInfo
{
public:
    HotkeyInfo(QString computerName, QString appName) : computerName(computerName), appName(appName) {}
    HotkeyInfo() : HotkeyInfo(QString(), QString()) {}
\
    QString computerName;
    QString appName;

    operator QString() const
    {
        return QString("HotkeyInfo(computerName=\"%0\", appName=\"%1\")").arg(computerName, appName);
    }

    static void save(MapIntString2HotkeyInfo* hotkeys, QSettings* settings, QString groupName)
    {
        settings->beginGroup(groupName);
        auto hotkeyNumbers = settings->childGroups();
        for (const auto& hotkeyNumber : hotkeyNumbers) {
            settings->remove(hotkeyNumber);
        }
        for (auto it = hotkeys->begin(); it != hotkeys->end(); ++it) {
            auto hotkeyNumber = it.key();
            settings->beginGroup(hotkeyNumber);
            auto hotkeyInfo = it.value().value<HotkeyInfo>();
            settings->setValue("computerName", hotkeyInfo.computerName);
            settings->setValue("appName", hotkeyInfo.appName);
            settings->endGroup();
        }
        settings->endGroup();
    }

    static void load(QSettings* settings, QString groupName, MapIntString2HotkeyInfo* hotkeys)
    {
        settings->beginGroup(groupName);
        auto hotkeyNumbers = settings->childGroups();
        for (const auto& hotkeyNumber : hotkeyNumbers) {
            settings->beginGroup(hotkeyNumber);
            auto computerName = settings->value("computerName").toString();
            auto appName = settings->value("appName").toString();
            auto hotkeyInfo = HotkeyInfo(computerName, appName);
            hotkeys->insert(hotkeyNumber, QVariant::fromValue(hotkeyInfo));
            settings->endGroup();
        }
        settings->endGroup();

    }
};
Q_DECLARE_METATYPE(HotkeyInfo);
