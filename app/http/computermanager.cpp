#include "computermanager.h"
#include "nvhttp.h"

#include <QThread>

#define SER_HOSTS "hosts"
#define SER_NAME "hostname"
#define SER_UUID "uuid"
#define SER_MAC "mac"
#define SER_CODECSUPP "codecsupport"
#define SER_LOCALADDR "localaddress"
#define SER_REMOTEADDR "remoteaddress"
#define SER_MANUALADDR "manualaddress"
#define SER_APPLIST "apps"

#define SER_APPNAME "name"
#define SER_APPID "id"
#define SER_APPHDR "hdr"

NvComputer::NvComputer(QSettings& settings)
{
    this->name = settings.value(SER_NAME).toString();
    this->uuid = settings.value(SER_UUID).toString();
    this->macAddress = settings.value(SER_MAC).toByteArray();
    this->serverCodecModeSupport = settings.value(SER_CODECSUPP).toInt();
    this->localAddress = settings.value(SER_LOCALADDR).toString();
    this->remoteAddress = settings.value(SER_REMOTEADDR).toString();
    this->manualAddress = settings.value(SER_MANUALADDR).toString();

    int appCount = settings.beginReadArray(SER_APPLIST);
    for (int i = 0; i < appCount; i++) {
        NvApp app;

        settings.setArrayIndex(i);

        app.name = settings.value(SER_APPNAME).toString();
        app.id = settings.value(SER_APPID).toInt();
        app.hdrSupported = settings.value(SER_APPHDR).toBool();

        this->appList.append(app);
    }
    settings.endArray();

    this->activeAddress = nullptr;
    this->currentGameId = 0;
    this->pairState = PS_UNKNOWN;
    this->state = CS_UNKNOWN;
}

void
NvComputer::serialize(QSettings& settings)
{
    QReadLocker lock(&this->lock);

    settings.setValue(SER_NAME, name);
    settings.setValue(SER_UUID, uuid);
    settings.setValue(SER_MAC, macAddress);
    settings.setValue(SER_CODECSUPP, serverCodecModeSupport);
    settings.setValue(SER_LOCALADDR, localAddress);
    settings.setValue(SER_REMOTEADDR, remoteAddress);
    settings.setValue(SER_MANUALADDR, manualAddress);

    // Avoid deleting an existing applist if we couldn't get one
    if (!appList.isEmpty()) {
        settings.remove(SER_APPLIST);
        settings.beginWriteArray(SER_APPLIST);
        for (int i = 0; i < appList.count(); i++) {
            settings.setArrayIndex(i);

            settings.setValue(SER_APPNAME, appList[i].name);
            settings.setValue(SER_APPID, appList[i].id);
            settings.setValue(SER_APPHDR, appList[i].hdrSupported);
        }
        settings.endArray();
    }
}

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

        // Drop the lock before notifying
        lock.unlock();

        // Tell our client if something changed
        if (changed) {
            handleComputerStateChanged(existingComputer);
        }
    }
    else {
        // Store this in our active sets
        m_KnownHosts[newComputer->uuid] = newComputer;

        // Start polling if enabled (write lock required)
        startPollingComputer(newComputer);

        // Drop the lock before notifying
        lock.unlock();

        // Tell our client about this new PC
        handleComputerStateChanged(newComputer);
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

    // Save updated hosts to QSettings
    saveHosts();
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

