#include "appmodel.h"

AppModel::AppModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_BoxArtManager, &BoxArtManager::boxArtLoadComplete,
            this, &AppModel::handleBoxArtLoaded);
}

void AppModel::initialize(ComputerManager* computerManager, int computerIndex)
{
    m_ComputerManager = computerManager;
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &AppModel::handleComputerStateChanged);

    Q_ASSERT(computerIndex < m_ComputerManager->getComputers().count());
    m_Computer = m_ComputerManager->getComputers().at(computerIndex);
    m_Apps = m_Computer->appList;
    m_CurrentGameId = m_Computer->currentGameId;
}

Session* AppModel::createSessionForApp(int appIndex)
{
    Q_ASSERT(appIndex < m_Apps.count());
    NvApp app = m_Apps.at(appIndex);

    return new Session(m_Computer, app);
}

int AppModel::rowCount(const QModelIndex &parent) const
{
    // For list models only the root node (an invalid parent) should return the list's size. For all
    // other (valid) parents, rowCount() should return 0 so that it does not become a tree model.
    if (parent.isValid())
        return 0;

    return m_Apps.count();
}

QVariant AppModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    Q_ASSERT(index.row() < m_Apps.count());
    NvApp app = m_Apps.at(index.row());

    switch (role)
    {
    case NameRole:
        return app.name;
    case RunningRole:
        return m_Computer->currentGameId == app.id;
    case BoxArtRole:
        // FIXME: const-correctness
        return const_cast<BoxArtManager&>(m_BoxArtManager).loadBoxArt(m_Computer, app);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> AppModel::roleNames() const
{
    QHash<int, QByteArray> names;

    names[NameRole] = "name";
    names[RunningRole] = "running";
    names[BoxArtRole] = "boxart";

    return names;
}

void AppModel::handleComputerStateChanged(NvComputer* computer)
{
    // Ignore updates for computers that aren't ours
    if (computer != m_Computer) {
        return;
    }

    // First, process additions/removals from the app list. This
    // is required because the new game may now be running, so
    // we can't check that first.
    if (computer->appList != m_Apps) {
        // Just reset the whole thing if the list changes
        beginResetModel();
        m_Apps = computer->appList;
        m_CurrentGameId = computer->currentGameId;
        endResetModel();
        return;
    }

    // Finally, process changes to the active app
    if (computer->currentGameId != m_CurrentGameId) {
        // First, invalidate the running state of newly running game
        for (int i = 0; i < m_Apps.count(); i++) {
            if (m_Apps[i].id == computer->currentGameId) {
                emit dataChanged(createIndex(i, 0),
                                 createIndex(i, 0),
                                 QVector<int>() << RunningRole);
                break;
            }
        }

        // Next, invalidate the running state of the old game (if it exists)
        if (m_CurrentGameId != 0) {
            for (int i = 0; i < m_Apps.count(); i++) {
                if (m_Apps[i].id == m_CurrentGameId) {
                    emit dataChanged(createIndex(i, 0),
                                     createIndex(i, 0),
                                     QVector<int>() << RunningRole);
                    break;
                }
            }
        }

        // Now update our internal state
        m_CurrentGameId = m_Computer->currentGameId;
    }
}

void AppModel::handleBoxArtLoaded(NvComputer* computer, NvApp app, QUrl /* image */)
{
    Q_ASSERT(computer == m_Computer);

    int index = m_Apps.indexOf(app);
    Q_ASSERT(index >= 0);

    // Let our view know the box art data has changed for this app
    emit dataChanged(createIndex(index, 0),
                     createIndex(index, 0),
                     QVector<int>() << BoxArtRole);
}
