#include "nvcomputer.h"

#include <QUdpSocket>
#include <QHostInfo>

#define SER_NAME "hostname"
#define SER_UUID "uuid"
#define SER_MAC "mac"
#define SER_LOCALADDR "localaddress"
#define SER_REMOTEADDR "remoteaddress"
#define SER_MANUALADDR "manualaddress"
#define SER_APPLIST "apps"
#define SER_SRVCERT "srvcert"

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
    this->serverCert = QSslCertificate(settings.value(SER_SRVCERT).toByteArray());

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

void NvComputer::serialize(QSettings& settings)
{
    QReadLocker lock(&this->lock);

    settings.setValue(SER_NAME, name);
    settings.setValue(SER_UUID, uuid);
    settings.setValue(SER_MAC, macAddress);
    settings.setValue(SER_LOCALADDR, localAddress);
    settings.setValue(SER_REMOTEADDR, remoteAddress);
    settings.setValue(SER_MANUALADDR, manualAddress);
    settings.setValue(SER_SRVCERT, serverCert.toPem());

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

NvComputer::NvComputer(QString address, QString serverInfo, QSslCertificate serverCert)
{
    this->serverCert = serverCert;

    this->name = NvHTTP::getXmlString(serverInfo, "hostname");
    if (this->name.isEmpty()) {
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
    if (!codecSupport.isEmpty()) {
        this->serverCodecModeSupport = codecSupport.toInt();
    }
    else {
        this->serverCodecModeSupport = 0;
    }

    QString maxLumaPixelsHEVC = NvHTTP::getXmlString(serverInfo, "MaxLumaPixelsHEVC");
    if (!maxLumaPixelsHEVC.isEmpty()) {
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

    if (macAddress.isEmpty()) {
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
                        qInfo().nospace().noquote() << "Sent WoL packet to " << name << " via " << address.toString() << ":" << port;
                        success = true;
                    }
                    else {
                        qWarning() << "Send failed:" << sock.error();
                    }
                }
            }
            else {
                qWarning() << "Bind failed:" << sock.error();
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
        if (uniqueAddressList[i].isEmpty()) {
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

#define ASSIGN_IF_CHANGED_AND_NONNULL(field)  \
    if (!that.field.isNull() &&               \
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
    ASSIGN_IF_CHANGED_AND_NONNULL(serverCert);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(appList);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(displayModes);
    return changed;
}
