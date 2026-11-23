#include "computermodel.h"
#include "path.h"
#include <QDateTime>
#ifdef Q_OS_WIN
#include "streaming/filetransferwindow.h"
#include "streaming/streamwindow.h"
#include <QApplication>
#include <QTimer>
#endif

#include <QThreadPool>

ComputerModel::ComputerModel(QObject* object)
    : QAbstractListModel(object) {}

ComputerModel::~ComputerModel()
{
#ifdef Q_OS_WIN
    closeFileTransfers();
#endif
}

void ComputerModel::initialize(ComputerManager* computerManager)
{
    m_ComputerManager = computerManager;
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerModel::handleComputerStateChanged);
    connect(m_ComputerManager, &ComputerManager::pairingCompleted,
            this, &ComputerModel::handlePairingCompleted);

    m_Computers = m_ComputerManager->getComputers();
#ifdef Q_OS_WIN
    connect(qApp, &QCoreApplication::aboutToQuit, this, &ComputerModel::closeFileTransfers);
    refreshStreamWindows();
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ComputerModel::refreshStreamWindows);
    timer->start(500);
#endif
}

#ifdef Q_OS_WIN
void ComputerModel::closeFileTransfers()
{
    // Destroy workers before the application's final thread-pool drain, giving
    // their asynchronous HTTPS session-end requests time to complete.
    const auto windows = m_FileWindows.values();
    qDeleteAll(windows);
}

void ComputerModel::refreshStreamWindows()
{
    QHash<QString, StreamStatus> current;
    for (const auto* computer : std::as_const(m_Computers)) {
        StreamStatus status;
        status.state = static_cast<StreamWindowState>(StreamWindow::state(computer->uuid, &status.progressText));
        if (status.state != NoStreamWindow) {
            status.viewOnly = StreamWindow::isViewOnly(computer->uuid);
            current.insert(computer->uuid, status);
        }
    }
    m_StreamWindows.swap(current);
    for (int row = 0; row < m_Computers.size(); ++row) {
        const auto& uuid = m_Computers.at(row)->uuid;
        const auto old = current.value(uuid);
        const auto status = m_StreamWindows.value(uuid);
        if (old.state != status.state || old.progressText != status.progressText || old.viewOnly != status.viewOnly)
            emit dataChanged(index(row), index(row), {StreamWindowStateRole, StreamProgressTextRole, ViewOnlyStreamWindowRole});
    }
}
#endif

QVariant ComputerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Q_ASSERT(index.row() >= 0 && index.row() < m_Computers.count());

    NvComputer* computer = m_Computers[index.row()];
    QReadLocker lock(&computer->lock);

    switch (role) {
    case PreviewRole: {
        const auto file = Path::desktopPreview(computer->uuid);
        if (!file.exists()) {
            return QUrl();
        }
        auto url = QUrl::fromLocalFile(file.absoluteFilePath());
        url.setFragment(QString::number(file.lastModified().toMSecsSinceEpoch()));
        return url;
    }
    case NameRole:
        return computer->name;
    case UuidRole:
        return computer->uuid;
    case ViewOnlyStreamWindowRole:
#ifdef Q_OS_WIN
        return m_StreamWindows.value(computer->uuid).viewOnly;
#else
        return false;
#endif
    case StreamWindowStateRole:
#ifdef Q_OS_WIN
        return static_cast<int>(m_StreamWindows.value(computer->uuid).state);
#else
        return static_cast<int>(NoStreamWindow);
#endif
    case StreamProgressTextRole:
#ifdef Q_OS_WIN
        return m_StreamWindows.value(computer->uuid).progressText;
#else
        return QString();
#endif
    case OnlineRole:
        return computer->state == NvComputer::CS_ONLINE;
    case PairedRole:
        return computer->pairState == NvComputer::PS_PAIRED;
    case BusyRole:
        return computer->currentGameId != 0;
    case HostPowerAvailableRole:
        return computer->state == NvComputer::CS_ONLINE && computer->pairState == NvComputer::PS_PAIRED && computer->hostPowerAvailable;
    case HostPowerPendingRole:
        return m_PendingPower.contains(computer->uuid);
    case WakeableRole:
        return computer->macAddress.size() == 6;
    case StatusUnknownRole:
        return computer->state == NvComputer::CS_UNKNOWN;
    case ServerSupportedRole:
        return computer->isSupportedServerVersion;
    case DetailsRole: {
        QString state, pairState;

        switch (computer->state) {
        case NvComputer::CS_ONLINE:
            state = computer->pairState == NvComputer::PS_PAIRED ? tr("Online") : tr("Unpaired");
            break;
        case NvComputer::CS_OFFLINE:
            state = tr("Offline");
            break;
        default:
            state = tr("Unknown");
            break;
        }

        switch (computer->pairState) {
        case NvComputer::PS_PAIRED:
            pairState = tr("Paired");
            break;
        case NvComputer::PS_NOT_PAIRED:
            pairState = tr("Unpaired");
            break;
        default:
            pairState = tr("Unknown");
            break;
        }

        return tr("Name: %1").arg(computer->name) + '\n' +
               tr("Status: %1").arg(state) + '\n' +
               tr("Active Address: %1").arg(computer->activeAddress.toString()) + '\n' +
               tr("UUID: %1").arg(computer->uuid) + '\n' +
               tr("Local Address: %1").arg(computer->localAddress.toString()) + '\n' +
               tr("Remote Address: %1").arg(computer->remoteAddress.toString()) + '\n' +
               tr("IPv6 Address: %1").arg(computer->ipv6Address.toString()) + '\n' +
               tr("Manual Address: %1").arg(computer->manualAddress.toString()) + '\n' +
               tr("MAC Address: %1").arg(computer->macAddress.isEmpty() ? tr("Unknown") : QString(computer->macAddress.toHex(':'))) + '\n' +
               tr("Pair State: %1").arg(pairState) + '\n' +
               tr("Running Game ID: %1").arg(computer->state == NvComputer::CS_ONLINE ? QString::number(computer->currentGameId) : tr("Unknown")) + '\n' +
               tr("HTTPS Port: %1").arg(computer->state == NvComputer::CS_ONLINE ? QString::number(computer->activeHttpsPort) : tr("Unknown"));
    }
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

    return m_Computers.count();
}

QHash<int, QByteArray> ComputerModel::roleNames() const
{
    QHash<int, QByteArray> names;

    names[NameRole] = "name";
    names[HostPowerAvailableRole] = "hostPowerAvailable";
    names[HostPowerPendingRole] = "hostPowerPending";
    names[UuidRole] = "uuid";
    names[StreamWindowStateRole] = "streamWindowState";
    names[ViewOnlyStreamWindowRole] = "viewOnlyStreamWindow";
    names[StreamProgressTextRole] = "streamProgressText";
    names[OnlineRole] = "online";
    names[PairedRole] = "paired";
    names[BusyRole] = "busy";
    names[WakeableRole] = "wakeable";
    names[StatusUnknownRole] = "statusUnknown";
    names[ServerSupportedRole] = "serverSupported";
    names[DetailsRole] = "details";
    names[PreviewRole] = "desktopPreview";

    return names;
}

int ComputerModel::indexOfUuid(const QString& uuid) const
{
    for (int row = 0; row < m_Computers.size(); ++row) {
        if (m_Computers.at(row)->uuid == uuid) {
            return row;
        }
    }
    return -1;
}

void ComputerModel::initializeQuickApps(AppModel* apps, int computerIndex)
{
    if (apps && computerIndex >= 0 && computerIndex < m_Computers.size()) {
        apps->initializeForComputer(m_ComputerManager, m_Computers.at(computerIndex), false);
    }
}

Session* ComputerModel::createQuickSession(int computerIndex)
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());

    NvComputer* computer = m_Computers[computerIndex];

    NvApp selected;
    {
        QReadLocker lock(&computer->lock);
        for (const auto& app : std::as_const(computer->appList)) {
            if (computer->currentGameId) {
                if (app.id == computer->currentGameId) {
                    selected = app;
                    break;
                }
            }
            else if (app.directLaunch && !app.hidden) {
                selected = app;
                break;
            }
            else if (app.isDesktop() && !app.hidden) {
                selected = app;
            }
        }
    }
    return selected.isInitialized() ? new Session(computer, selected) : nullptr;
}

Session* ComputerModel::createViewOnlySession(int computerIndex)
{
    NvComputer* computer = m_Computers.value(computerIndex);
    if (!computer)
        return nullptr;
    QReadLocker lock(&computer->lock);
    if (computer->state == NvComputer::CS_ONLINE && computer->pairState == NvComputer::PS_PAIRED) {
        for (auto app : std::as_const(computer->appList)) {
            if (app.isDesktop() && (!computer->currentGameId || app.id == computer->currentGameId))
                return new Session(computer, app, nullptr, true);
        }
    }
    return nullptr;
}

void ComputerModel::refreshDesktopPreviews()
{
    if (!m_Computers.isEmpty()) {
        emit dataChanged(index(0), index(m_Computers.size() - 1), {PreviewRole});
    }
}

void ComputerModel::deleteComputer(int computerIndex)
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());

#ifdef Q_OS_WIN
    delete m_FileWindows.value(m_Computers[computerIndex]->uuid);
#endif
    beginRemoveRows(QModelIndex(), computerIndex, computerIndex);

    // m_Computer[computerIndex] will be deleted by this call
    QFile::remove(Path::desktopPreview(m_Computers[computerIndex]->uuid).absoluteFilePath());
    m_ComputerManager->deleteHost(m_Computers[computerIndex]);

    // Remove the now invalid item
    m_Computers.removeAt(computerIndex);

    endRemoveRows();
}

// Copy the target under its lock. Removing a device while a request is in
// flight must not invalidate the worker or change the target of the action.
class DeviceActionTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    enum Action { Wake, Restart, Shutdown };
    DeviceActionTask(const NvComputer& computer, Action action)
        : m_Computer(computer), m_Action(action) {}

    void run() override
    {
        if (m_Action == Wake) {
            bool sent = m_Computer.wake();
            emit finished(sent, sent ? ComputerModel::tr("Wake packet sent to %1.").arg(m_Computer.name)
                                     : ComputerModel::tr("Could not send a wake packet to %1.").arg(m_Computer.name));
            return;
        }
        try {
            NvHTTP http(&m_Computer);
            http.requestHostPower(m_Action == Restart);
            const auto message = m_Action == Restart ? ComputerModel::tr("Restart requested for %1.")
                                                     : ComputerModel::tr("Shutdown requested for %1.");
            emit finished(true, message.arg(m_Computer.name));
        }
        catch (const std::exception& error) {
            emit finished(false, ComputerModel::tr("Power request to %1: %2").arg(m_Computer.name, QString::fromUtf8(error.what())));
        }
    }

signals:
    void finished(bool success, QString message);

private:
    NvComputer m_Computer;
    Action m_Action;
};

void ComputerModel::wakeComputer(int computerIndex)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.size()) return;
    auto* computer = m_Computers.at(computerIndex);
    QReadLocker lock(&computer->lock);
    if (computer->state == NvComputer::CS_ONLINE || computer->macAddress.size() != 6) return;
    auto* task = new DeviceActionTask(*computer, DeviceActionTask::Wake);
    connect(task, &DeviceActionTask::finished, this, [this, uuid = computer->uuid](bool success, QString message) {
        emit toolActionResult(uuid, message, success);
    });
    QThreadPool::globalInstance()->start(task);
}

void ComputerModel::requestHostPower(const QString& uuid, bool restart)
{
    const int row = indexOfUuid(uuid);
    if (row < 0 || m_PendingPower.contains(uuid)) return;
    auto* computer = m_Computers.at(row);
    QReadLocker lock(&computer->lock);
    if (computer->state != NvComputer::CS_ONLINE || computer->pairState != NvComputer::PS_PAIRED ||
            computer->serverCert.isNull() || !computer->activeHttpsPort || !computer->hostPowerAvailable) {
        lock.unlock();
        emit toolActionResult(uuid, tr("Power controls are unavailable for this host."), false);
        return;
    }
    auto* task = new DeviceActionTask(*computer, restart ? DeviceActionTask::Restart : DeviceActionTask::Shutdown);
    lock.unlock();
    m_PendingPower.insert(uuid);
    emit dataChanged(index(row), index(row), {HostPowerPendingRole});
    connect(task, &DeviceActionTask::finished, this, [this, uuid](bool success, QString message) {
        m_PendingPower.remove(uuid);
        const int current = indexOfUuid(uuid);
        if (current >= 0) emit dataChanged(index(current), index(current), {HostPowerPendingRole});
        emit toolActionResult(uuid, message, success);
    });
    QThreadPool::globalInstance()->start(task);
}

void ComputerModel::openFileTransfer(const QString& uuid)
{
#ifdef Q_OS_WIN
    const int row = indexOfUuid(uuid);
    if (row < 0) return;
    if (auto* window = m_FileWindows.value(uuid)) {
        if (window->isMinimized()) window->showNormal();
        else window->show();
        window->raise();
        window->activateWindow();
        return;
    }
    const auto* computer = m_Computers.at(row);
    QReadLocker lock(&computer->lock);
    if (computer->state != NvComputer::CS_ONLINE || computer->pairState != NvComputer::PS_PAIRED ||
        computer->activeAddress.isNull() || !computer->activeHttpsPort || computer->serverCert.isNull()) {
        lock.unlock();
        emit toolActionResult(uuid, tr("Connect to a paired, online host to transfer files."), false);
        return;
    }
    QUrl host;
    host.setScheme(QStringLiteral("https"));
    host.setHost(computer->activeAddress.address());
    host.setPort(computer->activeHttpsPort);
    const auto certificate = computer->serverCert;
    const auto name = computer->name;
    lock.unlock();
    const auto token = FileTransferEngine::newSessionToken();
    if (token.isEmpty()) {
        emit toolActionResult(uuid, tr("Could not open the file transfer window."), false);
        return;
    }
    auto* window = new FileTransferWindow(host, certificate, token, name, uuid, nullptr, true);
    window->setAttribute(Qt::WA_DeleteOnClose);
    m_FileWindows.insert(uuid, window);
    connect(window, &QObject::destroyed, this, [this, uuid] { m_FileWindows.remove(uuid); });
    window->show();
    window->raise();
    window->activateWindow();
#else
    emit toolActionResult(uuid, tr("File transfer requires Windows."), false);
#endif
}

void ComputerModel::renameComputer(int computerIndex, QString name)
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());

    m_ComputerManager->renameHost(m_Computers[computerIndex], name);
}

QString ComputerModel::generatePinString()
{
    return m_ComputerManager->generatePinString();
}

class DeferredTestConnectionTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    void run()
    {
        unsigned int portTestResult = LiTestClientConnectivity("qt.conntest.moonlight-stream.org", 443, ML_PORT_FLAG_ALL);
        if (portTestResult == ML_TEST_RESULT_INCONCLUSIVE) {
            emit connectionTestCompleted(-1, QString());
        }
        else {
            char blockedPorts[512];
            LiStringifyPortFlags(portTestResult, "\n", blockedPorts, sizeof(blockedPorts));
            emit connectionTestCompleted(portTestResult, QString(blockedPorts));
        }
    }

signals:
    void connectionTestCompleted(int result, QString blockedPorts);
};

void ComputerModel::testConnectionForComputer(int)
{
    DeferredTestConnectionTask* testConnectionTask = new DeferredTestConnectionTask();
    QObject::connect(testConnectionTask, &DeferredTestConnectionTask::connectionTestCompleted,
                     this, &ComputerModel::connectionTestCompleted);
    QThreadPool::globalInstance()->start(testConnectionTask);
}

void ComputerModel::pairComputer(int computerIndex, QString pin)
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());

    m_ComputerManager->pairHost(m_Computers[computerIndex], pin);
}

void ComputerModel::handlePairingCompleted(NvComputer*, QString error)
{
    emit pairingCompleted(error.isEmpty() ? QVariant() : error);
}

void ComputerModel::handleComputerStateChanged(NvComputer* computer)
{
    const auto hosts = m_ComputerManager->getComputers();
    if (hosts != m_Computers) {
        // Keep surviving delegates and the selected host as availability changes.
        for (int row = m_Computers.size() - 1; row >= 0; --row) {
            if (!hosts.contains(m_Computers.at(row))) {
                beginRemoveRows({}, row, row);
                m_Computers.removeAt(row);
                endRemoveRows();
            }
        }
        for (int row = 0; row < hosts.size(); ++row) {
            const int previous = m_Computers.indexOf(hosts.at(row), row);
            if (previous < 0) {
                beginInsertRows({}, row, row);
                m_Computers.insert(row, hosts.at(row));
                endInsertRows();
            }
            else if (previous != row) {
                beginMoveRows({}, previous, previous, {}, row);
                m_Computers.move(previous, row);
                endMoveRows();
            }
        }
    }

    const int row = m_Computers.indexOf(computer);
    if (row >= 0) {
        emit dataChanged(index(row), index(row));
    }
}

#include "computermodel.moc"
