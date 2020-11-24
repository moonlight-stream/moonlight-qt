#include "nvcomputer.h"
#include "nvapp.h"

#include <QUdpSocket>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QNetworkProxy>

#define SER_NAME "hostname"
#define SER_UUID "uuid"
#define SER_MAC "mac"
#define SER_LOCALADDR "localaddress"
#define SER_REMOTEADDR "remoteaddress"
#define SER_MANUALADDR "manualaddress"
#define SER_IPV6ADDR "ipv6address"
#define SER_APPLIST "apps"
#define SER_SRVCERT "srvcert"
#define SER_CUSTOMNAME "customname"

NvComputer::NvComputer(QSettings& settings)
{
    this->name = settings.value(SER_NAME).toString();
    this->uuid = settings.value(SER_UUID).toString();
    this->hasCustomName = settings.value(SER_CUSTOMNAME).toBool();
    this->macAddress = settings.value(SER_MAC).toByteArray();
    this->localAddress = settings.value(SER_LOCALADDR).toString();
    this->remoteAddress = settings.value(SER_REMOTEADDR).toString();
    this->ipv6Address = settings.value(SER_IPV6ADDR).toString();
    this->manualAddress = settings.value(SER_MANUALADDR).toString();
    this->serverCert = QSslCertificate(settings.value(SER_SRVCERT).toByteArray());

    int appCount = settings.beginReadArray(SER_APPLIST);
    for (int i = 0; i < appCount; i++) {
        settings.setArrayIndex(i);

        NvApp app(settings);
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

void NvComputer::serialize(QSettings& settings) const
{
    QReadLocker lock(&this->lock);

    settings.setValue(SER_NAME, name);
    settings.setValue(SER_CUSTOMNAME, hasCustomName);
    settings.setValue(SER_UUID, uuid);
    settings.setValue(SER_MAC, macAddress);
    settings.setValue(SER_LOCALADDR, localAddress);
    settings.setValue(SER_REMOTEADDR, remoteAddress);
    settings.setValue(SER_IPV6ADDR, ipv6Address);
    settings.setValue(SER_MANUALADDR, manualAddress);
    settings.setValue(SER_SRVCERT, serverCert.toPem());

    // Avoid deleting an existing applist if we couldn't get one
    if (!appList.isEmpty()) {
        settings.remove(SER_APPLIST);
        settings.beginWriteArray(SER_APPLIST);
        for (int i = 0; i < appList.count(); i++) {
            settings.setArrayIndex(i);
            appList[i].serialize(settings);
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

    this->hasCustomName = false;
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

    // We can get an IPv4 loopback address if we're using the GS IPv6 Forwarder
    this->localAddress = NvHTTP::getXmlString(serverInfo, "LocalIP");
    if (this->localAddress.startsWith("127.")) {
        this->localAddress = QString();
    }

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
        9, // Standard WOL port (privileged port)
        47998, 47999, 48000, 48002, 48010, // Ports opened by GFE
        47009, // Port opened by Moonlight Internet Hosting Tool for WoL (non-privileged port)
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

    // Try to broadcast on all available NICs
    for (const QNetworkInterface& nic : QNetworkInterface::allInterfaces()) {
        // Ensure the interface is up and skip the loopback adapter
        if ((nic.flags() & QNetworkInterface::IsUp) == 0 ||
                (nic.flags() & QNetworkInterface::IsLoopBack) != 0) {
            continue;
        }

        QHostAddress allNodesMulticast("FF02::1");
        for (const QNetworkAddressEntry& addr : nic.addressEntries()) {
            // Store the scope ID for this NIC if IPv6 is enabled
            if (!addr.ip().scopeId().isEmpty()) {
                allNodesMulticast.setScopeId(addr.ip().scopeId());
            }

            // Skip IPv6 which doesn't support broadcast
            if (!addr.broadcast().isNull()) {
                addressList.append(addr.broadcast().toString());
            }
        }

        if (!allNodesMulticast.scopeId().isEmpty()) {
            addressList.append(allNodesMulticast.toString());
        }
    }

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
    }

    return success;
}

bool NvComputer::isReachableOverVpn()
{
    if (activeAddress.isEmpty()) {
        return false;
    }

    QTcpSocket s;

    s.setProxy(QNetworkProxy::NoProxy);
    s.connectToHost(activeAddress, 47984);
    if (s.waitForConnected(3000)) {
        Q_ASSERT(!s.localAddress().isNull());

        for (const QNetworkInterface& nic : QNetworkInterface::allInterfaces()) {
            // Ensure the interface is up
            if ((nic.flags() & QNetworkInterface::IsUp) == 0) {
                continue;
            }

            for (const QNetworkAddressEntry& addr : nic.addressEntries()) {
                if (addr.ip() == s.localAddress()) {
                    qInfo() << "Found matching interface:" << nic.humanReadableName() << nic.hardwareAddress() << nic.flags();

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
                    qInfo() << "Interface Type:" << nic.type();
                    qInfo() << "Interface MTU:" << nic.maximumTransmissionUnit();

                    if (nic.type() == QNetworkInterface::Virtual ||
                            nic.type() == QNetworkInterface::Ppp) {
                        // Treat PPP and virtual interfaces as likely VPNs
                        return true;
                    }

                    if (nic.maximumTransmissionUnit() != 0 && nic.maximumTransmissionUnit() < 1500) {
                        // Treat MTUs under 1500 as likely VPNs
                        return true;
                    }
#endif

                    if (nic.flags() & QNetworkInterface::IsPointToPoint) {
                        // Treat point-to-point links as likely VPNs.
                        // This check detects OpenVPN on Unix-like OSes.
                        return true;
                    }

                    if (nic.hardwareAddress().startsWith("00:FF", Qt::CaseInsensitive)) {
                        // OpenVPN TAP interfaces have a MAC address starting with 00:FF on Windows
                        return true;
                    }

                    if (nic.humanReadableName().startsWith("ZeroTier")) {
                        // ZeroTier interfaces always start with "ZeroTier"
                        return true;
                    }

                    if (nic.humanReadableName().contains("VPN")) {
                        // This one is just a final heuristic if all else fails
                        return true;
                    }

                    // Didn't meet any of our VPN heuristics
                    return false;
                }
            }
        }

        qWarning() << "No match found for address:" << s.localAddress();
        return false;
    }
    else {
        // If we fail to connect, just pretend that it's not a VPN
        return false;
    }
}

bool NvComputer::updateAppList(QVector<NvApp> newAppList) {
    if (appList == newAppList) {
        return false;
    }

    // Propagate client-side attributes to the new app list
    for (const NvApp& existingApp : appList) {
        for (NvApp& newApp : newAppList) {
            if (existingApp.id == newApp.id) {
                newApp.hidden = existingApp.hidden;
                newApp.directLaunch = existingApp.directLaunch;
            }
        }
    }

    appList = newAppList;
    sortAppList();
    return true;
}

QVector<QString> NvComputer::uniqueAddresses() const
{
    QVector<QString> uniqueAddressList;

    // Start with addresses correctly ordered
    uniqueAddressList.append(activeAddress);
    uniqueAddressList.append(localAddress);
    uniqueAddressList.append(remoteAddress);
    uniqueAddressList.append(ipv6Address);
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

    if (!hasCustomName) {
        // Only overwrite the name if it's not custom
        ASSIGN_IF_CHANGED(name);
    }
    ASSIGN_IF_CHANGED_AND_NONEMPTY(macAddress);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(localAddress);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(remoteAddress);
    ASSIGN_IF_CHANGED_AND_NONEMPTY(ipv6Address);
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
    ASSIGN_IF_CHANGED_AND_NONEMPTY(displayModes);

    if (!that.appList.isEmpty()) {
        // updateAppList() handles merging client-side attributes
        updateAppList(that.appList);
    }

    return changed;
}
