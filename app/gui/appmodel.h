#pragma once

#include "backend/boxartmanager.h"
#include "backend/computermanager.h"

#include <QAbstractListModel>

class AppModel : public QAbstractListModel
{
    Q_OBJECT

    enum Roles
    {
        NameRole = Qt::UserRole,
        RunningRole,
        BoxArtRole
    };

public:
    explicit AppModel(QObject *parent = nullptr);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(int computerIndex);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handleBoxArtLoaded(NvComputer* computer, NvApp app, QUrl image);

private:
    NvComputer* m_Computer;
    BoxArtManager m_BoxArtManager;
    ComputerManager m_ComputerManager;
    QVector<NvApp> m_Apps;
    int m_CurrentGameId;
};
