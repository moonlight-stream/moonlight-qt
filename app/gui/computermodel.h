#include "backend/computermanager.h"

#include <QAbstractListModel>

class ComputerModel : public QAbstractListModel
{
    enum Roles
    {
        NameRole = Qt::UserRole,
        OnlineRole,
        PairedRole,
        BusyRole,
        AddPcRole
    };

public:
    ComputerModel(QObject* object = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

private slots:
    void handleComputerStateChanged(NvComputer* computer);

private:
    QVector<NvComputer*> m_Computers;
    ComputerManager m_ComputerManager;
};
