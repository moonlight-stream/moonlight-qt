#pragma once
#include "nvhttp.h"

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
                        goto Terminate;
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

    Terminate:
        emit terminating(m_Computer);
    }

signals:
   void computerStateChanged(NvComputer* computer);
   void terminating(NvComputer* computer);

private:
    NvComputer* m_Computer;
};

class ComputerManager : public QObject
{
    Q_OBJECT

public:
    explicit ComputerManager(QObject *parent = nullptr);

    void startPolling();

    void stopPollingAsync();

    bool addNewHost(QString address, bool mdns);

signals:
    void computerStateChanged(NvComputer* computer);

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handlePollThreadTermination(NvComputer* computer);

private:
    void saveHosts();

    void startPollingComputer(NvComputer* computer);

    bool m_Polling;
    QReadWriteLock m_Lock;
    QMap<QString, NvComputer*> m_KnownHosts;
    QMap<QString, QThread*> m_PollThreads;
};
