#pragma once

#include <QAbstractListModel>

#include "settings/streamingpreferences.h"


class HotkeyModel : public QAbstractListModel
{
    Q_OBJECT

    enum Roles
    {
        AppNameRole = Qt::UserRole,
        ComputerNameRole,
        HotkeyNumberRole,
    };

public:
    explicit HotkeyModel(QObject *parent = nullptr);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(StreamingPreferences* prefs);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief hotkeyIndexGet
     * @param computerName
     * @param appName
     * @return index found, or -1
     */
    Q_INVOKABLE int hotkeyIndexGet(QString computerName, QString appName);

    /**
     * @brief addHotkey
     * @param computerName
     * @param appName
     * @return index added if value >= 0, otherwise index found = -(value + 1)
     */
    Q_INVOKABLE int hotkeyAdd(QString computerName, QString appName);

    /**
     * @brief hotkeyRemove
     * @param computerName
     * @param appName
     * @return index removed, or -1
     */
    Q_INVOKABLE int hotkeyRemove(QString computerName, QString appName);

signals:
    void hotkeysChanged();

private:
    int hotkeyIndexGet(QString json);

private:
    StreamingPreferences* m_pPrefs;
};
