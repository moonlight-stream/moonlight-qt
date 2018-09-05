#include "computermanager.h"
#include "nvhttp.h"

#include <QThread>
#include <QUdpSocket>
#include <QHostInfo>
#include <QThreadPool>

#define SER_HOSTS "hosts"
#define SER_NAME "hostname"
#define SER_UUID "uuid"
#define SER_MAC "mac"
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
    sortAppList();

    this->activeAddress = nullptr;
    this->currentGameId = 0;
    this->pairState = PS_UNKNOWN;
    this->state = CS_UNKNOWN;
    this->gfeVersion = nullptr;
    this->appVersion = nullptr;
    this->maxLumaPixelsHEVC = 0;
    this->serverCodecModeSupport = 0;
    this->pendingQuit = false;
    this->gpuModel = nullptr;
}

void
NvComputer::serialize(QSettings& settings)
{
    QReadLocker lock(&this->lock);

    settings.setValue(SER_NAME, name);
    settings.setValue(SER_UUID, uuid);
    settings.setValue(SER_MAC, macAddress);
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

void NvComputer::sortAppList()
{
    std::stable_sort(appList.begin(), appList.end(), [](const NvApp& app1, const NvApp& app2) {
       return app1.name.toLower() < app2.name.toLower();
    });
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

    QString maxLumaPixelsHEVC = NvHTTP::getXmlString(serverInfo, "MaxLumaPixelsHEVC");
    if (!maxLumaPixelsHEVC.isNull()) {
        this->maxLumaPixelsHEVC = maxLumaPixelsHEVC.toInt();
    }
    else {
        this->maxLumaPixelsHEVC = 0;
    }

    this->displayModes = NvHTTP::getDisplayModeList(serverInfo);
    std::stable_sort(this->displayModes.begin(), this->displayModes.end(),
                     [](const NvDisplayMode& mode1, const NvDisplayMode& mode2) {
        return mode1.width * mode1.height * mode1.refreshRate <
                mode2.width * mode2.height * mode2.refreshRate;
    });

    this->localAddress = NvHTTP::getXmlString(serverInfo, "LocalIP");
    this->remoteAddress = NvHTTP::getXmlString(serverInfo, "ExternalIP");
    this->pairState = NvHTTP::getXmlString(serverInfo, "PairStatus") == "1" ?
                PS_PAIRED : PS_NOT_PAIRED;
    this->currentGameId = NvHTTP::getCurrentGame(serverInfo);
    this->appVersion = NvHTTP::getXmlString(serverInfo, "appversion");
    this->gfeVersion = NvHTTP::getXmlString(serverInfo, "GfeVersion");
    this->gpuModel = NvHTTP::getXmlString(serverInfo, "gputype");
    this->activeAddress = address;
    this->state = NvComputer::CS_ONLINE;
    this->pendingQuit = false;
}

bool NvComputer::wake()
{
    if (state == NvComputer::CS_ONLINE) {
        qWarning() << name << "is already online";
        return true;
    }

    if (macAddress.isNull()) {
        qWarning() << name << "has no MAC address stored";
        return false;
    }

    const quint16 WOL_PORTS[] = {
        7, 9, // Standard WOL ports
        47998, 47999, 48000, // Ports opened by GFE
    };

    // Create the WoL payload
    QByteArray wolPayload;
    wolPayload.append(QByteArray::fromHex("FFFFFFFFFFFF"));
    for (int i = 0; i < 16; i++) {
        wolPayload.append(macAddress);
    }
    Q_ASSERT(wolPayload.count() == 102);

    // Add the addresses that we know this host to be
    // and broadcast addresses for this link just in
    // case the host has timed out in ARP entries.
    QVector<QString> addressList = uniqueAddresses();
    addressList.append("255.255.255.255");

    // Try all unique address strings or host names
    bool success = false;
    for (QString& addressString : addressList) {
        QHostInfo hostInfo = QHostInfo::fromName(addressString);

        if (hostInfo.error() != QHostInfo::NoError) {
            qWarning() << "Error resolving" << addressString << ":" << hostInfo.errorString();
            continue;
        }

        // Try all IP addresses that this string resolves to
        for (QHostAddress& address : hostInfo.addresses()) {
            QUdpSocket sock;

            // Bind to any address on the correct protocol
            if (sock.bind(address.protocol() == QUdpSocket::IPv4Protocol ?
                          QHostAddress::AnyIPv4 : QHostAddress::AnyIPv6)) {

                // Send to all ports
                for (quint16 port : WOL_PORTS) {
                    if (sock.writeDatagram(wolPayload, address, port)) {
                        qInfo().nospace().noquote() << "Send WoL packet to " << name << " via " << address.toString() << ":" << port;
                        success = true;
                    }
                }
            }
        }
    }

    return success;
}

QVector<QString> NvComputer::uniqueAddresses()
{
    QVector<QString> uniqueAddressList;

    // Start with addresses correctly ordered
    uniqueAddressList.append(activeAddress);
    uniqueAddressList.append(localAddress);
    uniqueAddressList.append(remoteAddress);
    uniqueAddressList.append(manualAddress);

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

    // We must have at least 1 address
    Q_ASSERT(!uniqueAddressList.isEmpty());

    return uniqueAddressList;
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
    ASSIGN_IF_CHANGED(gfeVersion);
    ASSIGN_IF_CHANGED(appVersion);
    ASSIGN_IF_CHANGED(maxLumaPixelsHEVC);
    ASSIGN_IF_CHANGED(gpuModel);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(appList);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(displayModes);
    return changed;
}

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

    // Start polling threads for each known host
    QMapIterator<QString, NvComputer*> i(m_KnownHosts);
    while (i.hasNext()) {
        i.next();
        startPollingComputer(i.value());
    }
}

void ComputerManager::handleMdnsServiceResolved(MdnsPendingComputer* computer,
                                                const QHostAddress& address)
{
    qInfo() << "Resolved" << computer->hostname() << "to" << address.toString();

    addNewHost(address.toString(), true);

    m_PendingResolution.removeOne(computer);
    computer->deleteLater();
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
        QWriteLocker lock(&m_ComputerManager->m_Lock);

        QThread* pollingThread = m_ComputerManager->m_PollThreads[m_Computer->uuid];
        if (pollingThread != nullptr) {
            pollingThread->requestInterruption();

            // We must wait here because we're going to delete computer
            // and we can't do that out from underneath the poller.
            pollingThread->wait();

            // The thread is safe to delete now
            Q_ASSERT(pollingThread->isFinished());
            delete pollingThread;
        }

        m_ComputerManager->m_PollThreads.remove(m_Computer->uuid);
        m_ComputerManager->m_KnownHosts.remove(m_Computer->uuid);

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

void ComputerManager::pairHost(NvComputer* computer, QString pin)
{
    // Punt to a worker thread to avoid stalling the
    // UI while waiting for pairing to complete
    PendingPairingTask* pairing = new PendingPairingTask(this, computer, pin);
    QThreadPool::globalInstance()->start(pairing);
}

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

void ComputerManager::addNewHost(QString address, bool mdns)
{
    // Punt to a worker thread to avoid stalling the
    // UI while waiting for serverinfo query to complete
    PendingAddTask* addTask = new PendingAddTask(this, address, mdns);
    QThreadPool::globalInstance()->start(addTask);
}

void
ComputerManager::handleComputerStateChanged(NvComputer* computer)
{
    emit computerStateChanged(computer);

    if (computer->pendingQuit && computer->currentGameId == 0) {
        computer->pendingQuit = false;
        emit quitAppCompleted(QVariant());
    }

    // Save updated hosts to QSettings
    saveHosts();
}

// Must hold m_Lock for write
void
ComputerManager::startPollingComputer(NvComputer* computer)
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

