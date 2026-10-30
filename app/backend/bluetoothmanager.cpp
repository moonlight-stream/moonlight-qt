#include "bluetoothmanager.h"

#include <QDebug>

#ifdef HAVE_BLUEZ

#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>

#define BLUEZ_SERVICE QStringLiteral("org.bluez")
#define BLUEZ_ROOT_PATH QStringLiteral("/")
#define BLUEZ_MANAGER_PATH QStringLiteral("/org/bluez")

#define ADAPTER_IFACE QStringLiteral("org.bluez.Adapter1")
#define DEVICE_IFACE QStringLiteral("org.bluez.Device1")
#define AGENT_MANAGER_IFACE QStringLiteral("org.bluez.AgentManager1")

#define OBJECT_MANAGER_IFACE QStringLiteral("org.freedesktop.DBus.ObjectManager")
#define PROPERTIES_IFACE QStringLiteral("org.freedesktop.DBus.Properties")

#define AGENT_PATH QStringLiteral("/org/moonlight_stream/bluetooth/agent")

// Numeric comparison pairing for modern devices
#define AGENT_CAPABILITY QStringLiteral("DisplayYesNo")

// Far longer than D-Bus default timeout
#define DEVICE_CALL_TIMEOUT_MS (120 * 1000)

namespace
{

// Helper definitions live at the end of this file
QDBusMessage createAdapterCall(const QString& adapterPath, const QString& method);
QDBusMessage createSetPropertyCall(const QString& path, const QString& interface,
                                   const QString& property, const QVariant& value);
void applyDeviceProperties(BluetoothDevice& device, const QVariantMap& properties);
bool deviceLessThan(const BluetoothDevice& a, const BluetoothDevice& b);
int insertPositionIn(const QVector<BluetoothDevice>& devices, const BluetoothDevice& device);

}

BluetoothAgent::BluetoothAgent(BluetoothManager* manager)
    : QObject(manager),
      m_Manager(manager),
      m_RequestPending(false),
      m_PendingType(BluetoothManager::ConfirmAuthorization)
{
}

void BluetoothAgent::beginRequest(int type, const QString& devicePath, const QString& detail)
{
    // We always reply outside this slot
    setDelayedReply(true);

    if (m_RequestPending) {
        qWarning() << "Rejecting Bluetooth agent request for" << devicePath << "- another request is pending";
        QDBusConnection::systemBus().send(
            message().createErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                                       QStringLiteral("Another pairing request is already in progress")));
        return;
    }

    m_PendingMessage = message();
    m_PendingType = type;
    m_RequestPending = true;

    m_Manager->reportAgentRequest(type, devicePath, detail);
}

void BluetoothAgent::respond(bool accepted, const QString& value)
{
    if (!m_RequestPending) {
        return;
    }

    QDBusMessage pendingMessage = m_PendingMessage;
    int pendingType = m_PendingType;

    m_RequestPending = false;
    m_PendingMessage = QDBusMessage();

    if (!accepted) {
        QDBusConnection::systemBus().send(
            pendingMessage.createErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                                            QStringLiteral("Rejected by user")));
        return;
    }

    switch (pendingType) {
    case BluetoothManager::EnterPinCode:
        QDBusConnection::systemBus().send(pendingMessage.createReply(value));
        break;
    case BluetoothManager::EnterPasskey:
        QDBusConnection::systemBus().send(pendingMessage.createReply(QVariant(value.toUInt())));
        break;
    default:
        QDBusConnection::systemBus().send(pendingMessage.createReply());
        break;
    }
}

void BluetoothAgent::abandonPendingRequest()
{
    if (!m_RequestPending) {
        return;
    }

    m_RequestPending = false;
    QDBusConnection::systemBus().send(
        m_PendingMessage.createErrorReply(QStringLiteral("org.bluez.Error.Canceled"),
                                          QStringLiteral("Cancelled")));
    m_PendingMessage = QDBusMessage();
}

void BluetoothAgent::Release()
{
    qInfo() << "BlueZ released our pairing agent";
    abandonPendingRequest();
}

QString BluetoothAgent::RequestPinCode(const QDBusObjectPath& device)
{
    beginRequest(BluetoothManager::EnterPinCode, device.path(), QString());
    return QString();
}

void BluetoothAgent::DisplayPinCode(const QDBusObjectPath& device, const QString& pinCode)
{
    // BlueZ expects an immediate reply here
    m_Manager->reportAgentRequest(BluetoothManager::DisplayCode, device.path(), pinCode);
}

uint BluetoothAgent::RequestPasskey(const QDBusObjectPath& device)
{
    beginRequest(BluetoothManager::EnterPasskey, device.path(), QString());
    return 0;
}

void BluetoothAgent::DisplayPasskey(const QDBusObjectPath& device, uint passkey, ushort entered)
{
    Q_UNUSED(entered);

    // Passkeys are always six digits
    m_Manager->reportAgentRequest(BluetoothManager::DisplayCode, device.path(),
                                  QStringLiteral("%1").arg(passkey, 6, 10, QLatin1Char('0')));
}

void BluetoothAgent::RequestConfirmation(const QDBusObjectPath& device, uint passkey)
{
    beginRequest(BluetoothManager::ConfirmPasskey, device.path(),
                 QStringLiteral("%1").arg(passkey, 6, 10, QLatin1Char('0')));
}

void BluetoothAgent::RequestAuthorization(const QDBusObjectPath& device)
{
    beginRequest(BluetoothManager::ConfirmAuthorization, device.path(), QString());
}

void BluetoothAgent::AuthorizeService(const QDBusObjectPath& device, const QString& uuid)
{
    Q_UNUSED(uuid);

    // Ask rather than silently granting access
    beginRequest(BluetoothManager::ConfirmAuthorization, device.path(), QString());
}

void BluetoothAgent::Cancel()
{
    // Remote device gave up on us
    abandonPendingRequest();

    emit m_Manager->agentRequestCancelled();
}

BluetoothManager::BluetoothManager(QObject* parent)
    : QObject(parent),
      m_ServiceWatcher(nullptr),
      m_Agent(nullptr),
      m_AgentRegistered(false),
      m_Powered(false),
      m_Discovering(false)
{
    qDBusRegisterMetaType<DBusInterfaceList>();
    qDBusRegisterMetaType<DBusManagedObjectList>();

    if (!QDBusConnection::systemBus().isConnected()) {
        qWarning() << "Unable to connect to the D-Bus system bus:"
                   << QDBusConnection::systemBus().lastError().message();
        m_StatusMessage = tr("Moonlight couldn't connect to the D-Bus system bus, so Bluetooth devices can't be managed here.");
        return;
    }

    m_ServiceWatcher = new QDBusServiceWatcher(BLUEZ_SERVICE, QDBusConnection::systemBus(),
                                               QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_ServiceWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, &BluetoothManager::onServiceRegistered);
    connect(m_ServiceWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &BluetoothManager::onServiceUnregistered);

    m_StatusMessage = tr("The BlueZ Bluetooth service isn't running on this system.");

    // Match rules survive bluetoothd restarting
    connectToBlueZ();

    refreshManagedObjects();
}

BluetoothManager::~BluetoothManager()
{
    disconnectFromBlueZ();
}

void BluetoothManager::connectToBlueZ()
{
    QDBusConnection bus = QDBusConnection::systemBus();

    bus.connect(BLUEZ_SERVICE, BLUEZ_ROOT_PATH, OBJECT_MANAGER_IFACE,
                QStringLiteral("InterfacesAdded"), this,
                SLOT(onInterfacesAdded(QDBusObjectPath,DBusInterfaceList)));
    bus.connect(BLUEZ_SERVICE, BLUEZ_ROOT_PATH, OBJECT_MANAGER_IFACE,
                QStringLiteral("InterfacesRemoved"), this,
                SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));

    // Object path comes from trailing QDBusMessage
    bus.connect(BLUEZ_SERVICE, QString(), PROPERTIES_IFACE,
                QStringLiteral("PropertiesChanged"), this,
                SLOT(onPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));

    if (m_Agent == nullptr) {
        m_Agent = new BluetoothAgent(this);
        if (!bus.registerObject(AGENT_PATH, m_Agent, QDBusConnection::ExportAllSlots)) {
            qWarning() << "Unable to export our Bluetooth pairing agent on D-Bus:"
                       << bus.lastError().message();
        }
    }
}

void BluetoothManager::disconnectFromBlueZ()
{
    QDBusConnection bus = QDBusConnection::systemBus();

    if (m_AgentRegistered) {
        QDBusMessage msg = QDBusMessage::createMethodCall(BLUEZ_SERVICE, BLUEZ_MANAGER_PATH,
                                                          AGENT_MANAGER_IFACE,
                                                          QStringLiteral("UnregisterAgent"));
        msg << QVariant::fromValue(QDBusObjectPath(AGENT_PATH));
        bus.call(msg, QDBus::NoBlock);
        m_AgentRegistered = false;
    }

    if (m_Agent != nullptr) {
        m_Agent->abandonPendingRequest();
        bus.unregisterObject(AGENT_PATH);
    }
}

void BluetoothManager::refreshManagedObjects()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(BLUEZ_SERVICE, BLUEZ_ROOT_PATH,
                                                      OBJECT_MANAGER_IFACE,
                                                      QStringLiteral("GetManagedObjects"));

    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* w) {
        w->deleteLater();

        QDBusPendingReply<DBusManagedObjectList> reply = *w;
        if (reply.isError()) {
            qWarning() << "GetManagedObjects() failed:" << reply.error().message();
            m_StatusMessage = tr("The BlueZ Bluetooth service isn't running on this system.");
            emit availableChanged();
            return;
        }

        const DBusManagedObjectList objects = reply.value();

        // Find an adapter before devices
        for (auto it = objects.constBegin(); it != objects.constEnd(); it++) {
            if (it.value().contains(ADAPTER_IFACE) && m_AdapterPath.isEmpty()) {
                setAdapter(it.key().path(), it.value().value(ADAPTER_IFACE));
            }
        }

        if (m_AdapterPath.isEmpty()) {
            qInfo() << "BlueZ is running but no Bluetooth adapter was found";
            m_StatusMessage = tr("No Bluetooth adapter was found on this system.");
            emit availableChanged();
            return;
        }

        for (auto it = objects.constBegin(); it != objects.constEnd(); it++) {
            if (it.value().contains(DEVICE_IFACE)) {
                addOrUpdateDevice(it.key().path(), it.value().value(DEVICE_IFACE));
            }
        }
    });
}

void BluetoothManager::setAdapter(const QString& path, const QVariantMap& properties)
{
    qInfo() << "Using Bluetooth adapter" << path;

    m_AdapterPath = path;
    m_AdapterName = properties.value(QStringLiteral("Alias")).toString();
    if (m_AdapterName.isEmpty()) {
        m_AdapterName = properties.value(QStringLiteral("Name")).toString();
    }
    m_StatusMessage = QString();

    m_Powered = properties.value(QStringLiteral("Powered")).toBool();
    m_Discovering = properties.value(QStringLiteral("Discovering")).toBool();

    // Register our agent now BlueZ exists
    if (!m_AgentRegistered && m_Agent != nullptr) {
        QDBusMessage msg = QDBusMessage::createMethodCall(BLUEZ_SERVICE, BLUEZ_MANAGER_PATH,
                                                          AGENT_MANAGER_IFACE,
                                                          QStringLiteral("RegisterAgent"));
        msg << QVariant::fromValue(QDBusObjectPath(AGENT_PATH)) << AGENT_CAPABILITY;

        auto watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* w) {
            w->deleteLater();

            QDBusPendingReply<> reply = *w;
            if (reply.isError()) {
                // Not fatal when another agent exists
                qWarning() << "Unable to register our Bluetooth pairing agent:" << reply.error().message();
                return;
            }

            m_AgentRegistered = true;

            // Become default so prompts reach Moonlight
            QDBusMessage defaultMsg = QDBusMessage::createMethodCall(BLUEZ_SERVICE, BLUEZ_MANAGER_PATH,
                                                                     AGENT_MANAGER_IFACE,
                                                                     QStringLiteral("RequestDefaultAgent"));
            defaultMsg << QVariant::fromValue(QDBusObjectPath(AGENT_PATH));
            QDBusConnection::systemBus().call(defaultMsg, QDBus::NoBlock);
        });
    }

    emit availableChanged();
    emit poweredChanged();
    emit discoveringChanged();
}

void BluetoothManager::updateAdapterProperties(const QVariantMap& properties)
{
    if (properties.contains(QStringLiteral("Powered"))) {
        bool powered = properties.value(QStringLiteral("Powered")).toBool();
        if (powered != m_Powered) {
            m_Powered = powered;
            emit poweredChanged();
        }
    }

    if (properties.contains(QStringLiteral("Discovering"))) {
        bool discovering = properties.value(QStringLiteral("Discovering")).toBool();
        if (discovering != m_Discovering) {
            m_Discovering = discovering;
            emit discoveringChanged();
        }
    }

    if (properties.contains(QStringLiteral("Alias")) || properties.contains(QStringLiteral("Name"))) {
        QString name = properties.value(QStringLiteral("Alias")).toString();
        if (name.isEmpty()) {
            name = properties.value(QStringLiteral("Name")).toString();
        }
        if (!name.isEmpty() && name != m_AdapterName) {
            m_AdapterName = name;
            emit availableChanged();
        }
    }
}

int BluetoothManager::indexOfDevice(const QString& path) const
{
    for (int i = 0; i < m_Devices.count(); i++) {
        if (m_Devices.at(i).path == path) {
            return i;
        }
    }

    return -1;
}

int BluetoothManager::sortedInsertPosition(const BluetoothDevice& device) const
{
    return insertPositionIn(m_Devices, device);
}

void BluetoothManager::replaceDevice(int index, const BluetoothDevice& device)
{
    QVector<BluetoothDevice> without = m_Devices;
    without.removeAt(index);

    int newIndex = insertPositionIn(without, device);
    if (newIndex == index) {
        m_Devices[index] = device;
        emit deviceChanged(index);
        return;
    }

    // Sort position changed, so move it
    m_Devices.removeAt(index);
    emit deviceRemoved(index);

    m_Devices.insert(newIndex, device);
    emit deviceAdded(newIndex);
}

void BluetoothManager::addOrUpdateDevice(const QString& path, const QVariantMap& properties)
{
    int index = indexOfDevice(path);
    if (index >= 0) {
        BluetoothDevice device = m_Devices.at(index);
        applyDeviceProperties(device, properties);
        replaceDevice(index, device);
        return;
    }

    BluetoothDevice device;
    device.path = path;
    applyDeviceProperties(device, properties);

    int insertIndex = sortedInsertPosition(device);
    m_Devices.insert(insertIndex, device);
    emit deviceAdded(insertIndex);
}

void BluetoothManager::removeDeviceByPath(const QString& path)
{
    int index = indexOfDevice(path);
    if (index < 0) {
        return;
    }

    m_Devices.removeAt(index);
    emit deviceRemoved(index);
}

void BluetoothManager::setDeviceBusy(const QString& path, bool busy)
{
    int index = indexOfDevice(path);
    if (index < 0 || m_Devices.at(index).busy == busy) {
        return;
    }

    // Busy doesn't affect sort order
    m_Devices[index].busy = busy;
    emit deviceChanged(index);
}

QString BluetoothManager::deviceNameForPath(const QString& path) const
{
    int index = indexOfDevice(path);
    if (index < 0) {
        return tr("Unknown device");
    }

    const BluetoothDevice& device = m_Devices.at(index);
    if (!device.name.isEmpty()) {
        return device.name;
    }
    else if (!device.address.isEmpty()) {
        return device.address;
    }
    else {
        return tr("Unknown device");
    }
}

void BluetoothManager::reportAgentRequest(int type, const QString& devicePath, const QString& detail)
{
    emit agentRequest(type, devicePath, deviceNameForPath(devicePath), detail);
}

void BluetoothManager::onServiceRegistered()
{
    qInfo() << "BlueZ appeared on the system bus";

    // Match rules and agent still registered
    refreshManagedObjects();
}

void BluetoothManager::onServiceUnregistered()
{
    qInfo() << "BlueZ disappeared from the system bus";

    m_AgentRegistered = false;
    if (m_Agent != nullptr) {
        m_Agent->abandonPendingRequest();
    }

    m_AdapterPath.clear();
    m_AdapterName.clear();
    m_Powered = false;
    m_Discovering = false;
    m_StatusMessage = tr("The BlueZ Bluetooth service isn't running on this system.");

    m_Devices.clear();
    emit devicesReset();

    emit availableChanged();
    emit poweredChanged();
    emit discoveringChanged();
}

void BluetoothManager::onInterfacesAdded(const QDBusObjectPath& path, const DBusInterfaceList& interfaces)
{
    if (interfaces.contains(ADAPTER_IFACE) && m_AdapterPath.isEmpty()) {
        setAdapter(path.path(), interfaces.value(ADAPTER_IFACE));
    }

    if (interfaces.contains(DEVICE_IFACE)) {
        addOrUpdateDevice(path.path(), interfaces.value(DEVICE_IFACE));
    }
}

void BluetoothManager::onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces)
{
    if (interfaces.contains(DEVICE_IFACE)) {
        removeDeviceByPath(path.path());
    }

    if (interfaces.contains(ADAPTER_IFACE) && path.path() == m_AdapterPath) {
        qInfo() << "Bluetooth adapter" << m_AdapterPath << "went away";

        m_AdapterPath.clear();
        m_AdapterName.clear();
        m_Powered = false;
        m_Discovering = false;
        m_StatusMessage = tr("No Bluetooth adapter was found on this system.");

        m_Devices.clear();
        emit devicesReset();

        emit availableChanged();
        emit poweredChanged();
        emit discoveringChanged();

        // Another adapter may still be present
        refreshManagedObjects();
    }
}

void BluetoothManager::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                           const QStringList& invalidated, const QDBusMessage& message)
{
    // Needed so our slot signature matches PropertiesChanged
    Q_UNUSED(invalidated);

    if (interface == ADAPTER_IFACE) {
        if (message.path() == m_AdapterPath) {
            updateAdapterProperties(changed);
        }
    }
    else if (interface == DEVICE_IFACE) {
        int index = indexOfDevice(message.path());
        if (index < 0) {
            // InterfacesAdded always precedes PropertiesChanged
            return;
        }

        BluetoothDevice device = m_Devices.at(index);
        applyDeviceProperties(device, changed);

        replaceDevice(index, device);
    }
}

bool BluetoothManager::isAvailable() const
{
    return !m_AdapterPath.isEmpty();
}

bool BluetoothManager::isPowered() const
{
    return m_Powered;
}

bool BluetoothManager::isDiscovering() const
{
    return m_Discovering;
}

QString BluetoothManager::adapterName() const
{
    return m_AdapterName;
}

QString BluetoothManager::statusMessage() const
{
    return m_StatusMessage;
}

const QVector<BluetoothDevice>& BluetoothManager::devices() const
{
    return m_Devices;
}

void BluetoothManager::setPowered(bool powered)
{
    if (m_AdapterPath.isEmpty()) {
        return;
    }

    QDBusMessage msg = createSetPropertyCall(m_AdapterPath, ADAPTER_IFACE,
                                             QStringLiteral("Powered"), powered);

    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* w) {
        w->deleteLater();

        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            qWarning() << "Unable to change adapter power state:" << reply.error().message();
            emit operationFailed(adapterName(), tr("turn Bluetooth on or off"), reply.error().message());
        }
    });
}

void BluetoothManager::startDiscovery()
{
    if (m_AdapterPath.isEmpty() || m_Discovering) {
        return;
    }

    auto watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(createAdapterCall(m_AdapterPath, QStringLiteral("StartDiscovery"))), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* w) {
        w->deleteLater();

        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            qWarning() << "StartDiscovery() failed:" << reply.error().message();
            emit operationFailed(adapterName(), tr("start scanning"), reply.error().message());
        }
    });
}

void BluetoothManager::stopDiscovery()
{
    if (m_AdapterPath.isEmpty() || !m_Discovering) {
        return;
    }

    // Scan stops when our connection ends
    QDBusConnection::systemBus().asyncCall(createAdapterCall(m_AdapterPath, QStringLiteral("StopDiscovery")));
}

void BluetoothManager::callDeviceMethod(const QString& path, const QString& method, const QString& operationName)
{
    if (indexOfDevice(path) < 0) {
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(BLUEZ_SERVICE, path, DEVICE_IFACE, method);

    setDeviceBusy(path, true);

    auto watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(msg, DEVICE_CALL_TIMEOUT_MS), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, path, method, operationName](QDBusPendingCallWatcher* w) {
        w->deleteLater();

        setDeviceBusy(path, false);

        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            qWarning() << method << "failed on" << path << ":" << reply.error().message();
            emit operationFailed(deviceNameForPath(path), operationName, reply.error().message());
        }
    });
}

void BluetoothManager::pairDevice(QString path)
{
    if (indexOfDevice(path) < 0) {
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(BLUEZ_SERVICE, path, DEVICE_IFACE,
                                                      QStringLiteral("Pair"));

    setDeviceBusy(path, true);

    auto watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(msg, DEVICE_CALL_TIMEOUT_MS), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, path](QDBusPendingCallWatcher* w) {
        w->deleteLater();

        setDeviceBusy(path, false);

        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            qWarning() << "Pair() failed on" << path << ":" << reply.error().message();
            emit operationFailed(deviceNameForPath(path), tr("pair"), reply.error().message());
            return;
        }

        // Untrusted devices cannot reconnect themselves
        setDeviceTrusted(path, true);
    });
}

void BluetoothManager::cancelPairing(QString path)
{
    if (m_Agent != nullptr) {
        m_Agent->abandonPendingRequest();
    }

    callDeviceMethod(path, QStringLiteral("CancelPairing"), tr("cancel pairing with"));
}

void BluetoothManager::connectDevice(QString path)
{
    callDeviceMethod(path, QStringLiteral("Connect"), tr("connect to"));
}

void BluetoothManager::disconnectDevice(QString path)
{
    callDeviceMethod(path, QStringLiteral("Disconnect"), tr("disconnect from"));
}

void BluetoothManager::setDeviceTrusted(QString path, bool trusted)
{
    if (indexOfDevice(path) < 0) {
        return;
    }

    QDBusMessage msg = createSetPropertyCall(path, DEVICE_IFACE, QStringLiteral("Trusted"), trusted);

    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, path](QDBusPendingCallWatcher* w) {
        w->deleteLater();

        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            qWarning() << "Unable to change trusted state of" << path << ":" << reply.error().message();
            emit operationFailed(deviceNameForPath(path), tr("trust"), reply.error().message());
        }
    });
}

void BluetoothManager::removeDevice(QString path)
{
    if (m_AdapterPath.isEmpty() || indexOfDevice(path) < 0) {
        return;
    }

    QDBusMessage msg = createAdapterCall(m_AdapterPath, QStringLiteral("RemoveDevice"));
    msg << QVariant::fromValue(QDBusObjectPath(path));

    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, path](QDBusPendingCallWatcher* w) {
        w->deleteLater();

        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            qWarning() << "RemoveDevice() failed on" << path << ":" << reply.error().message();
            emit operationFailed(deviceNameForPath(path), tr("remove"), reply.error().message());
        }
    });
}

void BluetoothManager::respondToAgentRequest(bool accepted, QString value)
{
    if (m_Agent != nullptr) {
        m_Agent->respond(accepted, value);
    }
}

// Helper functions declared near the top
namespace
{

QDBusMessage createAdapterCall(const QString& adapterPath, const QString& method)
{
    return QDBusMessage::createMethodCall(BLUEZ_SERVICE, adapterPath, ADAPTER_IFACE, method);
}

QDBusMessage createSetPropertyCall(const QString& path, const QString& interface,
                                   const QString& property, const QVariant& value)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(BLUEZ_SERVICE, path, PROPERTIES_IFACE,
                                                      QStringLiteral("Set"));
    msg << interface << property << QVariant::fromValue(QDBusVariant(value));
    return msg;
}

void applyDeviceProperties(BluetoothDevice& device, const QVariantMap& properties)
{
    if (properties.contains(QStringLiteral("Address"))) {
        device.address = properties.value(QStringLiteral("Address")).toString();
    }
    if (properties.contains(QStringLiteral("Alias"))) {
        device.name = properties.value(QStringLiteral("Alias")).toString();
    }
    if (properties.contains(QStringLiteral("Name")) && device.name.isEmpty()) {
        device.name = properties.value(QStringLiteral("Name")).toString();
    }
    if (properties.contains(QStringLiteral("Paired"))) {
        device.paired = properties.value(QStringLiteral("Paired")).toBool();
    }
    if (properties.contains(QStringLiteral("Trusted"))) {
        device.trusted = properties.value(QStringLiteral("Trusted")).toBool();
    }
    if (properties.contains(QStringLiteral("Connected"))) {
        device.connected = properties.value(QStringLiteral("Connected")).toBool();
    }
    if (properties.contains(QStringLiteral("Blocked"))) {
        device.blocked = properties.value(QStringLiteral("Blocked")).toBool();
    }
}

// Connected first, then paired, then alphabetical
bool deviceLessThan(const BluetoothDevice& a, const BluetoothDevice& b)
{
    if (a.connected != b.connected) {
        return a.connected;
    }
    if (a.paired != b.paired) {
        return a.paired;
    }
    if (a.name.isEmpty() != b.name.isEmpty()) {
        return !a.name.isEmpty();
    }

    int nameComparison = a.name.compare(b.name, Qt::CaseInsensitive);
    if (nameComparison != 0) {
        return nameComparison < 0;
    }

    return a.address < b.address;
}

int insertPositionIn(const QVector<BluetoothDevice>& devices, const BluetoothDevice& device)
{
    int i = 0;
    while (i < devices.count() && deviceLessThan(devices.at(i), device)) {
        i++;
    }
    return i;
}

}

#else

// No-op stubs when BlueZ is unavailable

BluetoothManager::BluetoothManager(QObject* parent)
    : QObject(parent)
{
    m_StatusMessage = tr("This build of Moonlight doesn't include Bluetooth support.");
}

BluetoothManager::~BluetoothManager() {}

bool BluetoothManager::isAvailable() const { return false; }
bool BluetoothManager::isPowered() const { return false; }
bool BluetoothManager::isDiscovering() const { return false; }
QString BluetoothManager::adapterName() const { return QString(); }
QString BluetoothManager::statusMessage() const { return m_StatusMessage; }

const QVector<BluetoothDevice>& BluetoothManager::devices() const { return m_Devices; }

void BluetoothManager::setPowered(bool) {}
void BluetoothManager::startDiscovery() {}
void BluetoothManager::stopDiscovery() {}
void BluetoothManager::pairDevice(QString) {}
void BluetoothManager::cancelPairing(QString) {}
void BluetoothManager::connectDevice(QString) {}
void BluetoothManager::disconnectDevice(QString) {}
void BluetoothManager::setDeviceTrusted(QString, bool) {}
void BluetoothManager::removeDevice(QString) {}
void BluetoothManager::respondToAgentRequest(bool, QString) {}

#endif
