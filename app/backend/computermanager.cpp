#include "computermanager.h"
#include "nvhttp.h"
#include "settings/streamingpreferences.h"

#include <QThread>
#include <QThreadPool>

#define SER_HOSTS "hosts"

class PcMonitorThread : public QThread
{
    Q_OBJECT

#define TRIES_BEFORE_OFFLINING 2
#define POLLS_PER_APPLIST_FETCH 10

public:
    PcMonitorThread(NvComputer* computer)
        : m_Computer(computer)
    {
        setObjectName("Polling thread for " + computer->name);
    }

private:
    bool tryPollComputer(QString address, bool& changed)
    {
        NvHTTP http(address);

        QString serverInfo;
        try {
            serverInfo = http.getServerInfo(NvHTTP::NvLogLevel::NONE);
        } catch (...) {
            return false;
        }

        NvComputer newState(address, serverInfo);

        // Ensure the machine that responded is the one we intended to contact
        if (m_Computer->uuid != newState.uuid) {
            qInfo() << "Found unexpected PC " << newState.name << " looking for " << m_Computer->name;
            return false;
        }

        changed = m_Computer->update(newState);
        return true;
    }

    bool updateAppList(bool& changed)
    {
        Q_ASSERT(m_Computer->activeAddress != nullptr);

        NvHTTP http(m_Computer->activeAddress);

        QVector<NvApp> appList;

        try {
            appList = http.getAppList();
            if (appList.isEmpty()) {
                return false;
            }
        } catch (...) {
            return false;
        }

        QWriteLocker lock(&m_Computer->lock);
        if (m_Computer->appList != appList) {
            m_Computer->appList = appList;
            m_Computer->sortAppList();
            changed = true;
        }

        return true;
    }

    void run() override
    {
        // Always fetch the applist the first time
        int pollsSinceLastAppListFetch = POLLS_PER_APPLIST_FETCH;
        while (!isInterruptionRequested()) {
            bool stateChanged = false;
            bool online = false;
            bool wasOnline = m_Computer->state == NvComputer::CS_ONLINE;
            for (int i = 0; i < TRIES_BEFORE_OFFLINING && !online; i++) {
                for (auto& address : m_Computer->uniqueAddresses()) {
                    if (isInterruptionRequested()) {
                        return;
                    }

                    if (tryPollComputer(address, stateChanged)) {
                        if (!wasOnline) {
                            qInfo() << m_Computer->name << "is now online at" << m_Computer->activeAddress;
                        }
                        online = true;
                        break;
                    }
                }
            }

            // Check if we failed after all retry attempts
            // Note: we don't need to acquire the read lock here,
            // because we're on the writing thread.
            if (!online && m_Computer->state != NvComputer::CS_OFFLINE) {
                qInfo() << m_Computer->name << "is now offline";
                m_Computer->state = NvComputer::CS_OFFLINE;
                stateChanged = true;
            }

            // Grab the applist if it's empty or it's been long enough that we need to refresh
            pollsSinceLastAppListFetch++;
            if (m_Computer->state == NvComputer::CS_ONLINE &&
                    m_Computer->pairState == NvComputer::PS_PAIRED &&
                    (m_Computer->appList.isEmpty() || pollsSinceLastAppListFetch >= POLLS_PER_APPLIST_FETCH)) {
                // Notify prior to the app list poll since it may take a while, and we don't
                // want to delay onlining of a machine, especially if we already have a cached list.
                if (stateChanged) {
                    emit computerStateChanged(m_Computer);
                    stateChanged = false;
                }

                if (updateAppList(stateChanged)) {
                    pollsSinceLastAppListFetch = 0;
                }
            }

            if (stateChanged) {
                // Tell anyone listening that we've changed state
                emit computerStateChanged(m_Computer);
            }

            // Wait a bit to poll again
            QThread::sleep(3);
        }
    }

signals:
   void computerStateChanged(NvComputer* computer);

private:
    NvComputer* m_Computer;
};

ComputerManager::ComputerManager(QObject *parent)
    : QObject(parent),
      m_PollingRef(0),
      m_MdnsBrowser(nullptr)
{
    QSettings settings;

    // Inflate our hosts from QSettings
    int hosts = settings.beginReadArray(SER_HOSTS);
    for (int i = 0; i < hosts; i++) {
        settings.setArrayIndex(i);
        NvComputer* computer = new NvComputer(settings);
        m_KnownHosts[computer->uuid] = computer;
    }
    settings.endArray();
}

ComputerManager::~ComputerManager()
{
    QWriteLocker lock(&m_Lock);

    // Delete machines that haven't been resolved yet
    while (!m_PendingResolution.isEmpty()) {
        MdnsPendingComputer* computer = m_PendingResolution.first();
        delete computer;
        m_PendingResolution.removeFirst();
    }

    // Delete the browser to stop discovery
    delete m_MdnsBrowser;
    m_MdnsBrowser = nullptr;

    {
        QMutableMapIterator<QString, QThread*> i(m_PollThreads);

        // Interrupt all polling threads
        while (i.hasNext()) {
            i.next();
            i.value()->requestInterruption();
        }

        // Wait for all threads to terminate before destroying
        // the NvComputer objects.
        i.toFront();
        while (i.hasNext()) {
            i.next();

            QThread* thread = i.value();
            thread->wait();
            delete thread;

            i.remove();
        }
    }

    {
        QMutableMapIterator<QString, NvComputer*> i(m_KnownHosts);

        // Destroy all NvComputer objects
        while (i.hasNext()) {
            i.next();
            delete i.value();
            i.remove();
        }
    }
}

void ComputerManager::saveHosts()
{
    QSettings settings;
    QReadLocker lock(&m_Lock);

    settings.remove(SER_HOSTS);
    settings.beginWriteArray(SER_HOSTS);
    for (int i = 0; i < m_KnownHosts.count(); i++) {
        settings.setArrayIndex(i);
        m_KnownHosts[m_KnownHosts.keys()[i]]->serialize(settings);
    }
    settings.endArray();
}

void ComputerManager::startPolling()
{
    QWriteLocker lock(&m_Lock);

    if (++m_PollingRef > 1) {
        return;
    }

    StreamingPreferences prefs;

    if (prefs.enableMdns) {
        // Start an MDNS query for GameStream hosts
        m_MdnsBrowser = new QMdnsEngine::Browser(&m_MdnsServer, "_nvstream._tcp.local.", &m_MdnsCache);
        connect(m_MdnsBrowser, &QMdnsEngine::Browser::serviceAdded,
                this, [this](const QMdnsEngine::Service& service) {
            qInfo() << "Discovered mDNS host:" << service.hostname();

            MdnsPendingComputer* pendingComputer = new MdnsPendingComputer(&m_MdnsServer, &m_MdnsCache, service);
            connect(pendingComputer, SIGNAL(resolvedv4(MdnsPendingComputer*,QHostAddress)),
                    this, SLOT(handleMdnsServiceResolved(MdnsPendingComputer*,QHostAddress)));
            m_PendingResolution.append(pendingComputer);
        });
    }
    else {
        qWarning() << "mDNS is disabled by user preference";
    }

    // Start polling threads for each known host
    QMapIterator<QString, NvComputer*> i(m_KnownHosts);
    while (i.hasNext()) {
        i.next();
        startPollingComputer(i.value());
    }
}

// Must hold m_Lock for write
void ComputerManager::startPollingComputer(NvComputer* computer)
{
    if (m_PollingRef == 0) {
        return;
    }

    if (m_PollThreads.contains(computer->uuid)) {
        Q_ASSERT(m_PollThreads[computer->uuid]->isRunning());
        return;
    }

    PcMonitorThread* thread = new PcMonitorThread(computer);
    connect(thread, SIGNAL(computerStateChanged(NvComputer*)),
            this, SLOT(handleComputerStateChanged(NvComputer*)));
    m_PollThreads[computer->uuid] = thread;
    thread->start();
}

void ComputerManager::handleMdnsServiceResolved(MdnsPendingComputer* computer,
                                                const QHostAddress& address)
{
    qInfo() << "Resolved" << computer->hostname() << "to" << address.toString();

    addNewHost(address.toString(), true);

    m_PendingResolution.removeOne(computer);
    computer->deleteLater();
}

void ComputerManager::handleComputerStateChanged(NvComputer* computer)
{
    emit computerStateChanged(computer);

    if (computer->pendingQuit && computer->currentGameId == 0) {
        computer->pendingQuit = false;
        emit quitAppCompleted(QVariant());
    }

    // Save updated hosts to QSettings
    saveHosts();
}

QVector<NvComputer*> ComputerManager::getComputers()
{
    QReadLocker lock(&m_Lock);

    return QVector<NvComputer*>::fromList(m_KnownHosts.values());
}

class DeferredHostDeletionTask : public QRunnable
{
public:
    DeferredHostDeletionTask(ComputerManager* cm, NvComputer* computer)
        : m_Computer(computer),
          m_ComputerManager(cm) {}

    void run()
    {
        QThread* pollingThread;

        // Only do the minimum amount of work while holding the writer lock.
        // We must release it before calling saveHosts().
        {
            QWriteLocker lock(&m_ComputerManager->m_Lock);

            pollingThread = m_ComputerManager->m_PollThreads[m_Computer->uuid];

            m_ComputerManager->m_PollThreads.remove(m_Computer->uuid);
            m_ComputerManager->m_KnownHosts.remove(m_Computer->uuid);
        }

        // Persist the new host list
        m_ComputerManager->saveHosts();

        // Delete the polling thread
        if (pollingThread != nullptr) {
            pollingThread->requestInterruption();

            // We must wait here because we're going to delete computer
            // and we can't do that out from underneath the poller.
            pollingThread->wait();

            // The thread is safe to delete now
            Q_ASSERT(pollingThread->isFinished());
            delete pollingThread;
        }

        // Finally, delete the computer itself. This must be done
        // last because the polling thread might be using it.
        delete m_Computer;
    }

private:
    NvComputer* m_Computer;
    ComputerManager* m_ComputerManager;
};

void ComputerManager::deleteHost(NvComputer* computer)
{
    // Punt to a worker thread to avoid stalling the
    // UI while waiting for the polling thread to die
    QThreadPool::globalInstance()->start(new DeferredHostDeletionTask(this, computer));
}

class PendingPairingTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PendingPairingTask(ComputerManager* computerManager, NvComputer* computer, QString pin)
        : m_Computer(computer),
          m_Pin(pin)
    {
        connect(this, &PendingPairingTask::pairingCompleted,
                computerManager, &ComputerManager::pairingCompleted);
    }

signals:
    void pairingCompleted(NvComputer* computer, QString error);

private:
    void run()
    {
        NvPairingManager pairingManager(m_Computer->activeAddress);

        try {
           NvPairingManager::PairState result = pairingManager.pair(m_Computer->appVersion, m_Pin);
           switch (result)
           {
           case NvPairingManager::PairState::PIN_WRONG:
               emit pairingCompleted(m_Computer, "The PIN from the PC didn't match. Please try again.");
               break;
           case NvPairingManager::PairState::FAILED:
               emit pairingCompleted(m_Computer, "Pairing failed. Please try again.");
               break;
           case NvPairingManager::PairState::ALREADY_IN_PROGRESS:
               emit pairingCompleted(m_Computer, "Another pairing attempt is already in progress.");
               break;
           case NvPairingManager::PairState::PAIRED:
               emit pairingCompleted(m_Computer, nullptr);
               break;
           }
        } catch (const GfeHttpResponseException& e) {
            emit pairingCompleted(m_Computer, e.toQString());
        }
    }

    NvComputer* m_Computer;
    QString m_Pin;
};

void ComputerManager::pairHost(NvComputer* computer, QString pin)
{
    // Punt to a worker thread to avoid stalling the
    // UI while waiting for pairing to complete
    PendingPairingTask* pairing = new PendingPairingTask(this, computer, pin);
    QThreadPool::globalInstance()->start(pairing);
}

class PendingQuitTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PendingQuitTask(ComputerManager* computerManager, NvComputer* computer)
        : m_Computer(computer)
    {
        connect(this, &PendingQuitTask::quitAppFailed,
                computerManager, &ComputerManager::quitAppCompleted);
    }

signals:
    void quitAppFailed(QString error);

private:
    void run()
    {
        NvHTTP http(m_Computer->activeAddress);

        try {
            if (m_Computer->currentGameId != 0) {
                http.quitApp();
            }
        } catch (const GfeHttpResponseException& e) {
            {
                QWriteLocker lock(&m_Computer->lock);
                m_Computer->pendingQuit = false;
            }
            if (e.getStatusCode() == 599) {
                // 599 is a special code we make a custom message for
                emit quitAppFailed("The running game wasn't started by this PC. "
                                   "You must quit the game on the host PC manually or use the device that originally started the game.");
            }
            else {
                emit quitAppFailed(e.toQString());
            }
        }
    }

    NvComputer* m_Computer;
};

void ComputerManager::quitRunningApp(NvComputer* computer)
{
    QWriteLocker lock(&computer->lock);
    computer->pendingQuit = true;

    PendingQuitTask* quit = new PendingQuitTask(this, computer);
    QThreadPool::globalInstance()->start(quit);
}

void ComputerManager::stopPollingAsync()
{
    QWriteLocker lock(&m_Lock);

    Q_ASSERT(m_PollingRef > 0);
    if (--m_PollingRef > 0) {
        return;
    }

    // Delete machines that haven't been resolved yet
    while (!m_PendingResolution.isEmpty()) {
        MdnsPendingComputer* computer = m_PendingResolution.first();
        computer->deleteLater();
        m_PendingResolution.removeFirst();
    }

    // Delete the browser to stop discovery
    delete m_MdnsBrowser;
    m_MdnsBrowser = nullptr;

    // Interrupt all threads, but don't wait for them to terminate
    QMutableMapIterator<QString, QThread*> i(m_PollThreads);
    while (i.hasNext()) {
        i.next();

        QThread* thread = i.value();

        // Let this thread delete itself when it's finished
        connect(thread, SIGNAL(finished()),
                thread, SLOT(deleteLater()));

        // The threads will delete themselves when they terminate,
        // but we remove them from the polling threads map here.
        i.value()->requestInterruption();
        i.remove();
    }
}

class PendingAddTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PendingAddTask(ComputerManager* computerManager, QString address, bool mdns)
        : m_ComputerManager(computerManager),
          m_Address(address),
          m_Mdns(mdns)
    {
        connect(this, &PendingAddTask::computerAddCompleted,
                computerManager, &ComputerManager::computerAddCompleted);
        connect(this, &PendingAddTask::computerStateChanged,
                computerManager, &ComputerManager::handleComputerStateChanged);
    }

signals:
    void computerAddCompleted(QVariant success);

    void computerStateChanged(NvComputer* computer);

private:
    void run()
    {
        NvHTTP http(m_Address);

        qInfo() << "Processing new PC at" << m_Address << "from" << (m_Mdns ? "mDNS" : "user");

        QString serverInfo;
        try {
            serverInfo = http.getServerInfo(NvHTTP::NvLogLevel::VERBOSE);
        } catch (...) {
            if (!m_Mdns) {
                emit computerAddCompleted(false);
            }
            return;
        }

        NvComputer* newComputer = new NvComputer(m_Address, serverInfo);

        // Update addresses depending on the context
        if (m_Mdns) {
            newComputer->localAddress = m_Address;
        }
        else {
            newComputer->manualAddress = m_Address;
        }

        // Check if this PC already exists
        QWriteLocker lock(&m_ComputerManager->m_Lock);
        NvComputer* existingComputer = m_ComputerManager->m_KnownHosts[newComputer->uuid];
        if (existingComputer != nullptr) {
            // Fold it into the existing PC
            bool changed = existingComputer->update(*newComputer);
            delete newComputer;

            // Drop the lock before notifying
            lock.unlock();

            // For non-mDNS clients, let them know it succeeded
            if (!m_Mdns) {
                emit computerAddCompleted(true);
            }

            // Tell our client if something changed
            if (changed) {
                qInfo() << existingComputer->name << "is now at" << existingComputer->activeAddress;
                emit computerStateChanged(existingComputer);
            }
        }
        else {
            // Store this in our active sets
            m_ComputerManager->m_KnownHosts[newComputer->uuid] = newComputer;

            // Start polling if enabled (write lock required)
            m_ComputerManager->startPollingComputer(newComputer);

            // Drop the lock before notifying
            lock.unlock();

            // For non-mDNS clients, let them know it succeeded
            if (!m_Mdns) {
                emit computerAddCompleted(true);
            }

            // Tell our client about this new PC
            emit computerStateChanged(newComputer);
        }
    }

    ComputerManager* m_ComputerManager;
    QString m_Address;
    bool m_Mdns;
};

void ComputerManager::addNewHost(QString address, bool mdns)
{
    // Punt to a worker thread to avoid stalling the
    // UI while waiting for serverinfo query to complete
    PendingAddTask* addTask = new PendingAddTask(this, address, mdns);
    QThreadPool::globalInstance()->start(addTask);
}

#include "computermanager.moc"
