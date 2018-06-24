#include "computermanager.h"
#include "nvhttp.h"

#include <QThread>

ComputerManager::ComputerManager()
    : m_Polling(false)
{

}

void ComputerManager::startPolling()
{
    m_Polling = true;


}

class PcMonitorThread : public QThread
{
    Q_OBJECT

#define TRIES_BEFORE_OFFLINING 2
#define POLLS_PER_APPLIST_FETCH 10

    PcMonitorThread(NvComputer* computer, IdentityManager im)
        : m_Im(im),
          m_Computer(computer)
    {

    }

#define DECLARE_UPDATE_COMPUTER_FIELD(Type)                 \
    bool UpdateComputerField(Type& oldValue, Type newValue) \
    {                                                       \
        if (oldValue == newValue) {                         \
            return false;                                   \
        }                                                   \
                                                            \
        oldValue = newValue;                                \
        return true;                                        \
    }

    DECLARE_UPDATE_COMPUTER_FIELD(QString)
    DECLARE_UPDATE_COMPUTER_FIELD(int)
    DECLARE_UPDATE_COMPUTER_FIELD(QByteArray)
    DECLARE_UPDATE_COMPUTER_FIELD(NvComputer::ComputerState)
    DECLARE_UPDATE_COMPUTER_FIELD(NvComputer::PairState)

    bool TryPollComputer(QString& address, bool& changed)
    {
        NvHTTP http(address, m_Im);

        QString serverInfo;
        try {
            serverInfo = http.getServerInfo();
        } catch (...) {
            return false;
        }

        changed = false;

        QString newName = http.getXmlString(serverInfo, "hostname");
        if (newName.isNull()) {
            newName = "UNKNOWN";
        }
        changed |= UpdateComputerField(m_Computer->name, newName);
        changed |= UpdateComputerField(m_Computer->uuid, http.getXmlString(serverInfo, "uniqueid"));

        QString newMacString = http.getXmlString(serverInfo, "mac");
        QByteArray newMac;
        if (newMacString != "00:00:00:00:00:00") {
            QStringList macOctets = newMacString.split(':');
            for (QString macOctet : macOctets) {
                newMac.append((char) macOctet.toInt(nullptr, 16));
            }
            changed |= UpdateComputerField(m_Computer->macAddress, newMac);
        }
        changed |= UpdateComputerField(m_Computer->localAddress, http.getXmlString(serverInfo, "LocalIP"));
        changed |= UpdateComputerField(m_Computer->remoteAddress, http.getXmlString(serverInfo, "ExternalIP"));
        changed |= UpdateComputerField(m_Computer->pairState, http.getXmlString(serverInfo, "PairStatus") == "1" ?
                    NvComputer::PS_PAIRED : NvComputer::PS_NOT_PAIRED);
        changed |= UpdateComputerField(m_Computer->currentGameId, http.getCurrentGame(serverInfo));
        changed |= UpdateComputerField(m_Computer->activeAddress, address);
        changed |= UpdateComputerField(m_Computer->state, NvComputer::CS_ONLINE);

        return true;
    }

    bool UpdateAppList(bool& changed)
    {
        changed = false;
        return false;
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

                    if (TryPollComputer(address, stateChanged)) {
                        break;
                    }
                }

                // No need to continue retrying if we're online
                if (m_Computer->state == NvComputer::CS_ONLINE) {
                    break;
                }
            }

            // Check if we failed after all retry attempts
            if (m_Computer->state != NvComputer::CS_ONLINE) {
                if (m_Computer->state != NvComputer::CS_OFFLINE) {
                    m_Computer->state = NvComputer::CS_OFFLINE;
                    stateChanged = true;
                }
            }

            if (stateChanged) {
                // Tell anyone listening that we've changed state
                emit computerStateChanged(m_Computer);
            }

            // Grab the applist if it's empty or it's been long enough that we need to refresh
            pollsSinceLastAppListFetch++;
            if (m_Computer->state == NvComputer::CS_ONLINE &&
                    (m_Computer->appList.isEmpty() || pollsSinceLastAppListFetch >= POLLS_PER_APPLIST_FETCH)) {
                stateChanged = false;

                if (UpdateAppList(stateChanged)) {
                    pollsSinceLastAppListFetch = 0;
                }

                if (stateChanged) {
                    emit computerStateChanged(m_Computer);
                }
            }

            // Wait a bit to poll again
            QThread::sleep(3);
        }
    }

signals:
   void computerStateChanged(NvComputer* computer);

private:
    IdentityManager m_Im;
    NvComputer* m_Computer;
};
