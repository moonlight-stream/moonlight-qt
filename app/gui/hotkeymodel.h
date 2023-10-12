#pragma once

#include "backend/computermanager.h"

#include <QAbstractListModel>

#include "settings/hotkeymanager.h"


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
    Q_INVOKABLE void initialize(ComputerManager* computerManager, HotkeyManager* hotkeyManager);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

private:
    ComputerManager* m_ComputerManager;
    HotkeyManager* m_HotkeyManager;
};
