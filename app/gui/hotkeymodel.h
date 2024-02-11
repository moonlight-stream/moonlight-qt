#pragma once

#include <QAbstractListModel>

#include "backend/boxartmanager.h"
#include "backend/computermanager.h"
#include "settings/hotkeymanager.h"


class HotkeyModel : public QAbstractListModel
{
    Q_OBJECT

    enum Roles
    {
        AppNameRole = Qt::UserRole,
        ComputerNameRole,
        HotkeyNumberRole,

        ComputerIsOnlineRole,
        ComputerIsPairedRole,
        ComputerIsStatusUnknownRole,

        AppIsRunningRole,
        AppBoxArtRole,
    };

public:
    explicit HotkeyModel(QObject *parent = nullptr);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(ComputerManager* computerManager, HotkeyManager* hotkeyManager);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

private slots:
    void handleComputerStateChanged(NvComputer* computer);
    void handleBoxArtLoaded(NvComputer* computer, NvApp app, QUrl image);

private:
    NvComputer* getComputer(QString computerName) const;
    bool getApp(QString computerName, QString appName, NvApp& app) const;
    bool getApp(NvComputer* computer, QString appName, NvApp& app) const;

    BoxArtManager m_BoxArtManager;
    ComputerManager* m_ComputerManager;
    HotkeyManager* m_HotkeyManager;
};
