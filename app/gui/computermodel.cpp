#include "computermodel.h"

ComputerModel::ComputerModel(QObject* object)
    : QAbstractListModel(object)
{
    connect(&m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerModel::handleComputerStateChanged);

    m_Computers = m_ComputerManager.getComputers();
    m_ComputerManager.startPolling();
}

QVariant ComputerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() == m_Computers.count()) {
        // We insert a synthetic item at the end for the Add PC option
        switch (role) {
        case NameRole:
            return "Add PC";
        case AddPcRole:
            return true;
        }
    }
    else if (index.row() > m_Computers.count()) {
        qWarning() << "Index out of bounds: " << index.row();
        return QVariant();
    }

    NvComputer* computer = m_Computers[index.row()];
    QReadLocker lock(&computer->lock);

    switch (role) {
    case NameRole:
        return computer->name;
    case OnlineRole:
        return computer->state == NvComputer::CS_ONLINE;
    case PairedRole:
        return computer->state == NvComputer::PS_PAIRED;
    case BusyRole:
        return computer->currentGameId != 0;
    case AddPcRole:
        return false;
    default:
        return QVariant();
    }
}

int ComputerModel::rowCount(const QModelIndex& parent) const
{
    // We should not return a count for valid index values,
    // only the parent (which will not have a "valid" index).
    if (parent.isValid()) {
        return 0;
    }

    // Add PC placeholder counts as 1
    return m_Computers.count() + 1;
}

QHash<int, QByteArray> ComputerModel::roleNames() const
{
    QHash<int, QByteArray> names;

    names[NameRole] = "name";
    names[OnlineRole] = "online";
    names[PairedRole] = "paired";
    names[BusyRole] = "busy";

    return names;
}

void ComputerModel::handleComputerStateChanged(NvComputer* computer)
{
    // If this is an existing computer, we can report the data changed
    int index = m_Computers.indexOf(computer);
    if (index >= 0) {
        // Let the view know that this specific computer changed
        emit dataChanged(createIndex(index, 0), createIndex(index, 0));
    }
    else {
        // This is a new PC that will be inserted at the end
        beginInsertRows(QModelIndex(), m_Computers.count(), m_Computers.count());
        m_Computers.append(computer);
        endInsertRows();
    }

    // Our view of the world must be in sync with ComputerManager's
    Q_ASSERT(m_Computers.count() == m_ComputerManager.getComputers().count());
}

