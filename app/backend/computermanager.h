#pragma once
#include "nvhttp.h"

#include <qmdnsengine/server.h>
#include <qmdnsengine/cache.h>
#include <qmdnsengine/browser.h>
#include <qmdnsengine/service.h>
#include <qmdnsengine/resolver.h>

#include <QThread>
#include <QReadWriteLock>
#include <QSettings>

class NvComputer
{
public:
    explicit NvComputer(QString address, QString serverInfo);

    explicit NvComputer(QSettings& settings);

    bool
    update(NvComputer& that);

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

    // Persisted traits
    QString localAddress;
    QString remoteAddress;
    QString manualAddress;
    QByteArray macAddress;
    QString name;
    QString uuid;
    int serverCodecModeSupport;
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
            changed = true;
        }

        return true;
    }

    void run() override
    {
        // Always fetch the applist the first time
        int pollsSinceLastAppListFetch = POLLS_PER_APPLIST_FETCH;
        while (!isInterruptionRequested()) {
            QVector<QString> uniqueAddressList;

            // Start with addresses correctly ordered
            uniqueAddressList.append(m_Computer->activeAddress);
            uniqueAddressList.append(m_Computer->localAddress);
            uniqueAddressList.append(m_Computer->remoteAddress);
            uniqueAddressList.append(m_Computer->manualAddress);

            // Prune duplicates (always giving precedence to the first)
            for (int i = 0; i < uniqueAddressList.count(); i++) {
                if (uniqueAddressList[i].isEmpty() || uniqueAddressList[i].isNull()) {
                    uniqueAddressList.remove(i);
                    i--;
                    continue;
                }
                for (int j = i + 1; j < uniqueAddressList.count(); j++) {
                    if (uniqueAddressList[i] == uniqueAddressList[j]) {
                        // Always remove the later occurrence
                        uniqueAddressList.remove(j);
                        j--;
                    }
                }
            }

            // We must have at least 1 address for this host
            Q_ASSERT(uniqueAddressList.count() != 0);

            bool stateChanged = false;
            for (int i = 0; i < TRIES_BEFORE_OFFLINING; i++) {
                for (auto& address : uniqueAddressList) {
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

public:
    explicit ComputerManager(QObject *parent = nullptr);

    void startPolling();

    void stopPollingAsync();

    bool addNewHost(QString address, bool mdns);

    QVector<NvComputer*> getComputers();

    // computer is deleted inside this call
    void deleteHost(NvComputer* computer);

signals:
    void computerStateChanged(NvComputer* computer);

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
