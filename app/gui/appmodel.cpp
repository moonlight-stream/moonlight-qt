#include "appmodel.h"
#include <algorithm>

AppModel::AppModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_BoxArtManager, &BoxArtManager::boxArtLoadComplete,
            this, &AppModel::handleBoxArtLoaded);
}

void AppModel::initialize(ComputerManager* computerManager, const QString& computerUuid, bool showHiddenGames)
{
    const auto hosts = computerManager->getComputers();
    for (auto* host : hosts) {
        if (host->uuid == computerUuid) {
            initializeForComputer(computerManager, host, showHiddenGames);
            return;
        }
    }
}

void AppModel::initializeForComputer(ComputerManager* computerManager, NvComputer* computer, bool showHiddenGames)
{
    m_ComputerManager = computerManager;
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &AppModel::handleComputerStateChanged);

    m_Computer = computer;
    m_CurrentGameId = m_Computer->currentGameId;
    m_ShowHiddenGames = showHiddenGames;

    updateAppList(m_Computer->appList);
}

int AppModel::getRunningAppId()
{
    return m_CurrentGameId;
}

QString AppModel::getRunningAppName()
{
    if (m_CurrentGameId != 0) {
        for (int i = 0; i < m_AllApps.count(); i++) {
            if (m_AllApps[i].id == m_CurrentGameId) {
                return m_AllApps[i].name;
            }
        }
    }

    return nullptr;
}

Session* AppModel::createSessionForApp(int appIndex)
{
    Q_ASSERT(appIndex >= 0 && appIndex < m_VisibleApps.count());
    NvApp app = m_VisibleApps.at(appIndex);

    return new Session(m_Computer, app);
}

Session* AppModel::createSessionForAppId(int appId)
{
    for (int i = 0; i < m_VisibleApps.size(); ++i) {
        if (m_VisibleApps[i].id == appId) {
            return createSessionForApp(i);
        }
    }
    return nullptr;
}

int AppModel::getDirectLaunchAppIndex()
{
    for (int i = 0; i < m_VisibleApps.count(); i++) {
        if (m_VisibleApps[i].directLaunch) {
            return i;
        }
    }

    return -1;
}

int AppModel::rowCount(const QModelIndex &parent) const
{
    // For list models only the root node (an invalid parent) should return the list's size. For all
    // other (valid) parents, rowCount() should return 0 so that it does not become a tree model.
    if (parent.isValid())
        return 0;

    return m_VisibleApps.count();
}

QVariant AppModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    Q_ASSERT(index.row() >= 0 && index.row() < m_VisibleApps.count());
    NvApp app = m_VisibleApps.at(index.row());

    switch (role)
    {
    case NameRole:
        return app.name;
    case RunningRole:
        return m_Computer->currentGameId == app.id;
    case BoxArtRole:
        // FIXME: const-correctness
        return const_cast<BoxArtManager&>(m_BoxArtManager).loadBoxArt(m_Computer, app);
    case HiddenRole:
        return app.hidden;
    case AppIdRole:
        return app.id;
    case DirectLaunchRole:
        return app.directLaunch;
    case AppCollectorGameRole:
        return app.isAppCollectorGame;
    case DesktopRole:
        return app.isDesktop();
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
    names[HiddenRole] = "hidden";
    names[AppIdRole] = "appid";
    names[DirectLaunchRole] = "directLaunch";
    names[AppCollectorGameRole] = "appCollectorGame";
    names[DesktopRole] = "isDesktop";

    return names;
}

void AppModel::quitRunningApp()
{
    m_ComputerManager->quitRunningApp(m_Computer);
}

bool AppModel::hasHiddenApps() const
{
    return std::any_of(m_AllApps.cbegin(), m_AllApps.cend(), [](const NvApp& app) { return app.hidden; });
}

void AppModel::setShowHiddenApps(bool show)
{
    if (m_ShowHiddenGames != show) {
        m_ShowHiddenGames = show;
        updateAppList(m_AllApps);
        emit showHiddenAppsChanged();
    }
}

QVector<NvApp> AppModel::getVisibleApps(const QVector<NvApp>& appList)
{
    QVector<NvApp> visibleApps;

    for (const NvApp& app : appList) {
        if (m_ShowHiddenGames || !app.hidden) {
            visibleApps.append(app);
        }
    }

    return visibleApps;
}

void AppModel::updateAppList(QVector<NvApp> newList)
{
    const bool hadHiddenApps = hasHiddenApps();
    m_AllApps = newList;

    QVector<NvApp> newVisibleList = getVisibleApps(newList);

    // Process removals and updates first
    for (int i = 0; i < m_VisibleApps.count(); i++) {
        const NvApp& existingApp = m_VisibleApps.at(i);

        bool found = false;
        for (const NvApp& newApp : std::as_const(newVisibleList)) {
            if (existingApp.id == newApp.id) {
                // If the data changed, update it in our list
                if (existingApp != newApp) {
                    m_VisibleApps.replace(i, newApp);
                    emit dataChanged(createIndex(i, 0), createIndex(i, 0));
                }

                found = true;
                break;
            }
        }

        if (!found) {
            beginRemoveRows(QModelIndex(), i, i);
            m_VisibleApps.removeAt(i);
            endRemoveRows();
            i--;
        }
    }

    // Process additions now
    for (const NvApp& newApp : std::as_const(newVisibleList)) {
        int insertionIndex = m_VisibleApps.size();
        bool found = false;

        for (int i = 0; i < m_VisibleApps.count(); i++) {
            const NvApp& existingApp = m_VisibleApps.at(i);

            if (existingApp.id == newApp.id) {
                found = true;
                break;
            }
            else if (existingApp.name.toLower() > newApp.name.toLower()) {
                insertionIndex = i;
                break;
            }
        }

        if (!found) {
            beginInsertRows(QModelIndex(), insertionIndex, insertionIndex);
            m_VisibleApps.insert(insertionIndex, newApp);
            endInsertRows();
        }
    }

    Q_ASSERT(newVisibleList == m_VisibleApps);
    if (hadHiddenApps != hasHiddenApps()) {
        if (hadHiddenApps && m_ShowHiddenGames) {
            m_ShowHiddenGames = false;
            emit showHiddenAppsChanged();
        }
        emit hasHiddenAppsChanged();
    }
}

void AppModel::setAppHidden(int appIndex, bool hidden)
{
    Q_ASSERT(appIndex >= 0 && appIndex < m_VisibleApps.count());
    int appId = m_VisibleApps.at(appIndex).id;

    {
        QWriteLocker lock(&m_Computer->lock);

        for (NvApp& app : m_Computer->appList) {
            if (app.id == appId) {
                app.hidden = hidden;
                break;
            }
        }
    }

    m_ComputerManager->clientSideAttributeUpdated(m_Computer);
}

void AppModel::setAppDirectLaunch(int appIndex, bool directLaunch)
{
    Q_ASSERT(appIndex >= 0 && appIndex < m_VisibleApps.count());
    int appId = m_VisibleApps.at(appIndex).id;

    {
        QWriteLocker lock(&m_Computer->lock);

        for (NvApp& app : m_Computer->appList) {
            if (directLaunch) {
                // We must clear direct launch from all other apps
                // to set it on the new app.
                app.directLaunch = app.id == appId;
            }
            else if (app.id == appId) {
                // If we're clearing direct launch, we're done once we
                // find our matching app ID.
                app.directLaunch = false;
                break;
            }
        }
    }

    m_ComputerManager->clientSideAttributeUpdated(m_Computer);
}

void AppModel::handleComputerStateChanged(NvComputer* computer)
{
    // Ignore updates for computers that aren't ours
    if (computer != m_Computer) {
        return;
    }

    // If the computer has gone offline or we've been unpaired,
    // signal the UI so we can go back to the PC view.
    if (m_Computer->state == NvComputer::CS_OFFLINE ||
            m_Computer->pairState == NvComputer::PS_NOT_PAIRED) {
        emit computerLost();
        return;
    }

    // First, process additions/removals from the app list. This
    // is required because the new game may now be running, so
    // we can't check that first.
    if (computer->appList != m_AllApps) {
        updateAppList(computer->appList);
    }

    // Finally, process changes to the active app
    if (computer->currentGameId != m_CurrentGameId) {
        // First, invalidate the running state of newly running game
        for (int i = 0; i < m_VisibleApps.count(); i++) {
            if (m_VisibleApps[i].id == computer->currentGameId) {
                emit dataChanged(createIndex(i, 0),
                                 createIndex(i, 0),
                                 QVector<int>() << RunningRole);
                break;
            }
        }

        // Next, invalidate the running state of the old game (if it exists)
        if (m_CurrentGameId != 0) {
            for (int i = 0; i < m_VisibleApps.count(); i++) {
                if (m_VisibleApps[i].id == m_CurrentGameId) {
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

    int index = m_VisibleApps.indexOf(app);

    // Make sure we're not delivering a callback to an app that's already been removed
    if (index >= 0) {
        // Let our view know the box art data has changed for this app
        emit dataChanged(createIndex(index, 0),
                         createIndex(index, 0),
                         QVector<int>() << BoxArtRole);
    }
    else {
        qWarning() << "App not found for box art callback:" << app.name;
    }
}
