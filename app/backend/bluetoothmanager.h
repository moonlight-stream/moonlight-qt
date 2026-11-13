#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#ifdef HAVE_BLUEZ
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QMap>
#include <QVariantMap>

// Types used by org.freedesktop.DBus.ObjectManager
typedef QMap<QString, QVariantMap> DBusInterfaceList;
typedef QMap<QDBusObjectPath, DBusInterfaceList> DBusManagedObjectList;

Q_DECLARE_METATYPE(DBusInterfaceList)
Q_DECLARE_METATYPE(DBusManagedObjectList)
#endif

class BluetoothManager;

// Device known to BlueZ
struct BluetoothDevice
{
    QString path;
    QString address;
    QString name;
    bool paired = false;
    bool trusted = false;
    bool connected = false;
    bool blocked = false;

    // Set while our operation is pending
    bool busy = false;
};

#ifdef HAVE_BLUEZ
// Implements org.bluez.Agent1 for pairing prompts
class BluetoothAgent : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")

public:
    explicit BluetoothAgent(BluetoothManager* manager);

    // Completes the outstanding agent request
    void respond(bool accepted, const QString& value);

    // Rejects any outstanding request
    void abandonPendingRequest();

public slots:
    void Release();
    QString RequestPinCode(const QDBusObjectPath& device);
    void DisplayPinCode(const QDBusObjectPath& device, const QString& pinCode);
    uint RequestPasskey(const QDBusObjectPath& device);
    void DisplayPasskey(const QDBusObjectPath& device, uint passkey, ushort entered);
    void RequestConfirmation(const QDBusObjectPath& device, uint passkey);
    void RequestAuthorization(const QDBusObjectPath& device);
    void AuthorizeService(const QDBusObjectPath& device, const QString& uuid);
    void Cancel();

private:
    // Rejects the call if one is pending
    void beginRequest(int type, const QString& devicePath, const QString& detail);

    BluetoothManager* m_Manager;
    QDBusMessage m_PendingMessage;
    bool m_RequestPending;
    int m_PendingType;
};
#endif

class BluetoothManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(bool powered READ isPowered WRITE setPowered NOTIFY poweredChanged)
    Q_PROPERTY(bool discovering READ isDiscovering NOTIFY discoveringChanged)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY availableChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY availableChanged)

public:
    // User interactions BlueZ requests during pairing
    enum AgentRequestType
    {
        // User accepts or rejects pairing
        ConfirmAuthorization,

        // User confirms passkey matches device
        ConfirmPasskey,

        // User types the device's PIN code
        EnterPinCode,

        // User types the device's passkey
        EnterPasskey,

        // Code must be entered on device
        DisplayCode
    };
    Q_ENUM(AgentRequestType)

    explicit BluetoothManager(QObject* parent = nullptr);
    ~BluetoothManager();

    bool isAvailable() const;
    bool isPowered() const;
    bool isDiscovering() const;
    QString adapterName() const;
    QString statusMessage() const;

    const QVector<BluetoothDevice>& devices() const;

    Q_INVOKABLE void setPowered(bool powered);
    Q_INVOKABLE void startDiscovery();
    Q_INVOKABLE void stopDiscovery();

    Q_INVOKABLE void pairDevice(QString path);
    Q_INVOKABLE void cancelPairing(QString path);
    Q_INVOKABLE void connectDevice(QString path);
    Q_INVOKABLE void disconnectDevice(QString path);
    Q_INVOKABLE void setDeviceTrusted(QString path, bool trusted);
    Q_INVOKABLE void removeDevice(QString path);

    // Answers the most recent agentRequest()
    Q_INVOKABLE void respondToAgentRequest(bool accepted, QString value = QString());

signals:
    void availableChanged();
    void poweredChanged();
    void discoveringChanged();

    // Emitted after devices() is updated
    void deviceAdded(int index);
    void deviceRemoved(int index);
    void deviceChanged(int index);
    void devicesReset();

    void operationFailed(QString deviceName, QString operation, QString error);

    // BlueZ needs input to finish pairing
    void agentRequest(int type, QString devicePath, QString deviceName, QString detail);

    // Pending request went away
    void agentRequestCancelled();

private:
#ifdef HAVE_BLUEZ
    friend class BluetoothAgent;

    void connectToBlueZ();
    void disconnectFromBlueZ();
    void refreshManagedObjects();

    void setAdapter(const QString& path, const QVariantMap& properties);
    void updateAdapterProperties(const QVariantMap& properties);

    void addOrUpdateDevice(const QString& path, const QVariantMap& properties);
    void removeDeviceByPath(const QString& path);
    int indexOfDevice(const QString& path) const;
    int sortedInsertPosition(const BluetoothDevice& device) const;
    void replaceDevice(int index, const BluetoothDevice& device);
    void setDeviceBusy(const QString& path, bool busy);

    QString deviceNameForPath(const QString& path) const;

    // Async call reporting failures via operationFailed()
    void callDeviceMethod(const QString& path, const QString& method, const QString& operationName);

    void reportAgentRequest(int type, const QString& devicePath, const QString& detail);

private slots:
    void onServiceRegistered();
    void onServiceUnregistered();
    void onInterfacesAdded(const QDBusObjectPath& path, const DBusInterfaceList& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);
    void onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                             const QStringList& invalidated, const QDBusMessage& message);

private:
    QDBusServiceWatcher* m_ServiceWatcher;
    BluetoothAgent* m_Agent;
    bool m_AgentRegistered;
    QString m_AdapterPath;
    QString m_AdapterName;
    bool m_Powered;
    bool m_Discovering;
#endif

    QVector<BluetoothDevice> m_Devices;
    QString m_StatusMessage;
};
