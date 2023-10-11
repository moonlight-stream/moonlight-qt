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
     * @brief hotkeyNumber
     * @param computerName
     * @param appName
     * @return index found, or -1
     */
    Q_INVOKABLE int hotkeyNumber(QString computerName, QString appName);

    Q_INVOKABLE void hotkeyPut(int hotkeyNumber, QString computerName, QString appName);

    Q_INVOKABLE void hotkeyRemove(int hotkeyNumber);

signals:
    void hotkeysChanged();

private:
    StreamingPreferences* m_pPrefs;
};
