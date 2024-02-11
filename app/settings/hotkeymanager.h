#pragma once

#include <QSettings>
#include <QVariantMap>
#include <QDataStream>
#include <QDebug>

class HotkeyInfo : public QObject
{
    Q_OBJECT
public:
    HotkeyInfo();
    HotkeyInfo(QString computerName, QString appName);

    Q_PROPERTY(QString computerName MEMBER m_computerName)
    Q_PROPERTY(QString appName MEMBER m_appName)

    operator QString() const;

private:
    QString m_computerName;
    QString m_appName;
};

QDebug operator<<(QDebug debug, const HotkeyInfo &hotkeyInfo);


using MapHotkeys = QMap<QString, QVariant>;

class HotkeyManager : public QObject
{
    Q_OBJECT
public:
    HotkeyManager(QObject *parent = nullptr);

    // Used in AppView.qml
    Q_INVOKABLE int count();

    // Used in main.xml
    Q_INVOKABLE QList<int> hotkeyNumbers() const;

    // Used in AppView.qml
    Q_INVOKABLE int hotkeyNumber(QString computerName, QString appName);

    // Used in main.qml
    Q_INVOKABLE HotkeyInfo* get(const int hotkeyNumber) const;

    // Used in AppView.qml
    Q_INVOKABLE void put(const int hotkeyNumber, QString computerName, QString appName);

    // Used in HotkeyView.qml
    Q_INVOKABLE void remove(QString computerName, QString appName);

    // Used in AppView.qml
    Q_INVOKABLE void remove(const int hotkeyNumber);

    void clear();

    operator QString() const;

signals:
    void hotkeysChanged();
};

QDebug operator<<(QDebug debug, const HotkeyManager &hotkeyManager);
