#pragma once
#include "nvhttp.h"
#include "nvpairingmanager.h"

#include <qmdnsengine/server.h>
#include <qmdnsengine/cache.h>
#include <qmdnsengine/browser.h>
#include <qmdnsengine/service.h>
#include <qmdnsengine/resolver.h>

#include <QThread>
#include <QReadWriteLock>
#include <QSettings>
#include <QRunnable>

class NvComputer
{
    friend class PcMonitorThread;

private:
    void sortAppList();

public:
    explicit NvComputer(QString address, QString serverInfo);

    explicit NvComputer(QSettings& settings);

    bool
    update(NvComputer& that);

    bool
    wake();

    QVector<QString>
    uniqueAddresses();

    void
    serialize(QSettings& settings);

    enum PairState
    {
        PS_UNKNOWN,
        PS_PAIRED,
        PS_NOT_PAIRED
    };

    enum ComputerState
    {
        CS_UNKNOWN,
        CS_ONLINE,
        CS_OFFLINE
    };

    // Ephemeral traits
    ComputerState state;
    PairState pairState;
    QString activeAddress;
    int currentGameId;
    QString gfeVersion;
    QString appVersion;
    QVector<NvDisplayMode> displayModes;
    int maxLumaPixelsHEVC;
    int serverCodecModeSupport;

    // Persisted traits
    QString localAddress;
    QString remoteAddress;
    QString manualAddress;
    QByteArray macAddress;
    QString name;
    QString uuid;
    QVector<NvApp> appList;

    // Synchronization
    QReadWriteLock lock;
};

// FIXME: MOC isn't finding Q_OBJECT properly when this is confined
// to computermanager.cpp as it should be.
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
            serverInfo = http.getServerInfo();
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
            for (int i = 0; i < TRIES_BEFORE_OFFLINING; i++) {
                for (auto& address : m_Computer->uniqueAddresses()) {
                    if (isInterruptionRequested()) {
                        return;
                    }

                    if (tryPollComputer(address, stateChanged)) {
                        break;
                    }
                }

                // No need to continue retrying if we're online
                if (m_Computer->state == NvComputer::CS_ONLINE) {
                    break;
                }
            }

            // Check if we failed after all retry attempts
            // Note: we don't need to acquire the read lock here,
            // because we're on the writing thread.
            if (m_Computer->state != NvComputer::CS_ONLINE) {
                if (m_Computer->state != NvComputer::CS_OFFLINE) {
                    m_Computer->state = NvComputer::CS_OFFLINE;
                    stateChanged = true;
                }
            }

            // Grab the applist if it's empty or it's been long enough that we need to refresh
            pollsSinceLastAppListFetch++;
            if (m_Computer->state == NvComputer::CS_ONLINE &&
                    m_Computer->pairState == NvComputer::PS_PAIRED &&
                    (m_Computer->appList.isEmpty() || pollsSinceLastAppListFetch >= POLLS_PER_APPLIST_FETCH)) {
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

class MdnsPendingComputer : public QObject
{
    Q_OBJECT

public:
    explicit MdnsPendingComputer(QMdnsEngine::Server* server,
                                 QMdnsEngine::Cache* cache,
                                 const QMdnsEngine::Service& service)
        : m_Hostname(service.hostname()),
          m_Resolver(server, m_Hostname, cache)
    {
        connect(&m_Resolver, SIGNAL(resolved(QHostAddress)),
                this, SLOT(handleResolved(QHostAddress)));
    }

    QString hostname()
    {
        return m_Hostname;
    }

private slots:
    void handleResolved(const QHostAddress& address)
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            m_Resolver.disconnect();
            emit resolvedv4(this, address);
        }
    }

signals:
    void resolvedv4(MdnsPendingComputer*, const QHostAddress&);

private:
    QByteArray m_Hostname;
    QMdnsEngine::Resolver m_Resolver;
};

class ComputerManager : public QObject
{
    Q_OBJECT

    friend class DeferredHostDeletionTask;
    friend class PendingAddTask;

public:
    explicit ComputerManager(QObject *parent = nullptr);

    Q_INVOKABLE void startPolling();

    Q_INVOKABLE void stopPollingAsync();

    Q_INVOKABLE void addNewHost(QString address, bool mdns);

    void pairHost(NvComputer* computer, QString pin);

    QVector<NvComputer*> getComputers();

    // computer is deleted inside this call
    void deleteHost(NvComputer* computer);

signals:
    void computerStateChanged(NvComputer* computer);

    void pairingCompleted(NvComputer* computer, QString error);

    void computerAddCompleted(QVariant success);

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handleMdnsServiceResolved(MdnsPendingComputer* computer, const QHostAddress& address);

private:
    void saveHosts();

    void startPollingComputer(NvComputer* computer);

    bool m_Polling;
    QReadWriteLock m_Lock;
    QMap<QString, NvComputer*> m_KnownHosts;
    QMap<QString, QThread*> m_PollThreads;
    QMdnsEngine::Server m_MdnsServer;
    QMdnsEngine::Browser* m_MdnsBrowser;
    QMdnsEngine::Cache m_MdnsCache;
    QVector<MdnsPendingComputer*> m_PendingResolution;
};

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

        QString serverInfo;
        try {
            serverInfo = http.getServerInfo();
        } catch (...) {
            emit computerAddCompleted(false);
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
