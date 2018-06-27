#include "computermanager.h"
#include "nvhttp.h"

#include <QThread>

NvComputer::NvComputer(QString address, QString serverInfo)
{
    this->name = NvHTTP::getXmlString(serverInfo, "hostname");
    if (this->name.isNull()) {
        this->name = "UNKNOWN";
    }

    this->uuid = NvHTTP::getXmlString(serverInfo, "uniqueid");
    QString newMacString = NvHTTP::getXmlString(serverInfo, "mac");
    if (newMacString != "00:00:00:00:00:00") {
        QStringList macOctets = newMacString.split(':');
        for (QString macOctet : macOctets) {
            this->macAddress.append((char) macOctet.toInt(nullptr, 16));
        }
    }

    QString codecSupport = NvHTTP::getXmlString(serverInfo, "ServerCodecModeSupport");
    if (!codecSupport.isNull()) {
        this->serverCodecModeSupport = codecSupport.toInt();
    }
    else {
        this->serverCodecModeSupport = 0;
    }

    this->localAddress = NvHTTP::getXmlString(serverInfo, "LocalIP");
    this->remoteAddress = NvHTTP::getXmlString(serverInfo, "ExternalIP");
    this->pairState = NvHTTP::getXmlString(serverInfo, "PairStatus") == "1" ?
                PS_PAIRED : PS_NOT_PAIRED;
    this->currentGameId = NvHTTP::getCurrentGame(serverInfo);
    this->activeAddress = address;
    this->state = NvComputer::CS_ONLINE;
}

bool NvComputer::update(NvComputer& that)
{
    bool changed = false;

    // Lock us for write and them for read
    QWriteLocker thisLock(&this->lock);
    QReadLocker thatLock(&that.lock);

    // UUID may not change or we're talking to a new PC
    Q_ASSERT(this->uuid == that.uuid);

#define ASSIGN_IF_CHANGED(field)       \
    if (this->field != that.field) {   \
        this->field = that.field;      \
        changed = true;                \
    }

#define ASSIGN_IF_CHANGED_AND_NONEMPTY(field) \
    if (!that.field.isEmpty() &&              \
        this->field != that.field) {          \
        this->field = that.field;             \
        changed = true;                       \
    }

    ASSIGN_IF_CHANGED(name);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(macAddress);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(localAddress);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(remoteAddress);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(manualAddress);
    ASSIGN_IF_CHANGED(pairState);
    ASSIGN_IF_CHANGED(serverCodecModeSupport);
    ASSIGN_IF_CHANGED(currentGameId);
    ASSIGN_IF_CHANGED(activeAddress);
    ASSIGN_IF_CHANGED(state);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(appList);
    return changed;
}

ComputerManager::ComputerManager()
    : m_Polling(false)
{

}

void ComputerManager::startPolling()
{
    QWriteLocker lock(&m_Lock);

    m_Polling = true;

    QMapIterator<QString, NvComputer*> i(m_KnownHosts);
    while (i.hasNext()) {
        i.next();
        startPollingComputer(i.value());
    }
}

void ComputerManager::stopPollingAsync()
{
    QWriteLocker lock(&m_Lock);

    m_Polling = false;

    // Interrupt all threads, but don't wait for them to terminate
    QMutableMapIterator<QString, QThread*> i(m_PollThreads);
    while (i.hasNext()) {
        i.next();

        // The threads will delete themselves when they terminate
        i.value()->requestInterruption();
    }
}

bool ComputerManager::addNewHost(QString address, bool mdns)
{
    NvHTTP http(address);

    QString serverInfo;
    try {
        serverInfo = http.getServerInfo();
    } catch (...) {
        return false;
    }

    NvComputer* newComputer = new NvComputer(address, serverInfo);

    // Update addresses depending on the context
    if (mdns) {
        newComputer->localAddress = address;
    }
    else {
        newComputer->manualAddress = address;
    }

    // Check if this PC already exists
    QWriteLocker lock(&m_Lock);
    NvComputer* existingComputer = m_KnownHosts[newComputer->uuid];
    if (existingComputer != nullptr) {
        // Fold it into the existing PC
        bool changed = existingComputer->update(*newComputer);
        delete newComputer;

        // Tell our client if something changed
        if (changed) {
            emit computerStateChanged(existingComputer);
        }
    }
    else {
        // Store this in our active sets
        m_KnownHosts[newComputer->uuid] = newComputer;

        // Tell our client about this new PC
        emit computerStateChanged(newComputer);

        // Start polling if enabled
        startPollingComputer(newComputer);
    }

    return true;
}

void
ComputerManager::handlePollThreadTermination(NvComputer* computer)
{
    QWriteLocker lock(&m_Lock);

    QThread* me = m_PollThreads[computer->uuid];
    Q_ASSERT(me != nullptr);

    m_PollThreads.remove(computer->uuid);
    me->deleteLater();
}

void
ComputerManager::handleComputerStateChanged(NvComputer* computer)
{
    emit computerStateChanged(computer);
}

// Must hold m_Lock for write
void
ComputerManager::startPollingComputer(NvComputer* computer)
{
    if (!m_Polling) {
        return;
    }

    if (m_PollThreads.contains(computer->uuid)) {
        Q_ASSERT(m_PollThreads[computer->uuid]->isRunning());
        return;
    }

    PcMonitorThread* thread = new PcMonitorThread(computer);
    connect(thread, SIGNAL(terminating(NvComputer*)),
            this, SLOT(handlePollThreadTermination(NvComputer*)));
    connect(thread, SIGNAL(computerStateChanged(NvComputer*)),
            this, SLOT(handleComputerStateChanged(NvComputer*)));
    m_PollThreads[computer->uuid] = thread;
    thread->start();
}

