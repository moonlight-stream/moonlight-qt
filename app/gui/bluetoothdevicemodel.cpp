#include "bluetoothdevicemodel.h"

BluetoothDeviceModel::BluetoothDeviceModel(QObject* parent)
    : QAbstractListModel(parent),
      m_Manager(nullptr)
{
}

void BluetoothDeviceModel::initialize(BluetoothManager* manager)
{
    Q_ASSERT(manager != nullptr);

    // Notify views attached before initialize()
    beginResetModel();
    m_Manager = manager;
    m_Devices = m_Manager->devices();
    endResetModel();

    connect(m_Manager, &BluetoothManager::deviceAdded,
            this, &BluetoothDeviceModel::handleDeviceAdded);
    connect(m_Manager, &BluetoothManager::deviceRemoved,
            this, &BluetoothDeviceModel::handleDeviceRemoved);
    connect(m_Manager, &BluetoothManager::deviceChanged,
            this, &BluetoothDeviceModel::handleDeviceChanged);
    connect(m_Manager, &BluetoothManager::devicesReset,
            this, &BluetoothDeviceModel::handleDevicesReset);
}

int BluetoothDeviceModel::rowCount(const QModelIndex& parent) const
{
    // We have no children
    if (parent.isValid()) {
        return 0;
    }

    return m_Devices.count();
}

QVariant BluetoothDeviceModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_Devices.count()) {
        return QVariant();
    }

    const BluetoothDevice& device = m_Devices.at(index.row());

    switch (role) {
    case PathRole:
        return device.path;
    case NameRole:
        if (!device.name.isEmpty()) {
            return device.name;
        }
        else if (!device.address.isEmpty()) {
            return device.address;
        }
        else {
            return tr("Unknown device");
        }
    case AddressRole:
        return device.address;
    case PairedRole:
        return device.paired;
    case TrustedRole:
        return device.trusted;
    case ConnectedRole:
        return device.connected;
    case BusyRole:
        return device.busy;
    case StatusTextRole:
        return statusTextForDevice(device);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> BluetoothDeviceModel::roleNames() const
{
    QHash<int, QByteArray> names;

    names[PathRole] = "path";
    names[NameRole] = "name";
    names[AddressRole] = "address";
    names[PairedRole] = "paired";
    names[TrustedRole] = "trusted";
    names[ConnectedRole] = "connected";
    names[BusyRole] = "busy";
    names[StatusTextRole] = "statusText";

    return names;
}

void BluetoothDeviceModel::handleDeviceAdded(int index)
{
    Q_ASSERT(index >= 0 && index <= m_Devices.count());

    beginInsertRows(QModelIndex(), index, index);
    m_Devices = m_Manager->devices();
    endInsertRows();
}

void BluetoothDeviceModel::handleDeviceRemoved(int index)
{
    Q_ASSERT(index >= 0 && index < m_Devices.count());

    beginRemoveRows(QModelIndex(), index, index);
    m_Devices = m_Manager->devices();
    endRemoveRows();
}

void BluetoothDeviceModel::handleDeviceChanged(int index)
{
    Q_ASSERT(index >= 0 && index < m_Devices.count());

    m_Devices = m_Manager->devices();
    emit dataChanged(createIndex(index, 0), createIndex(index, 0));
}

void BluetoothDeviceModel::handleDevicesReset()
{
    beginResetModel();
    m_Devices = m_Manager->devices();
    endResetModel();
}

QString BluetoothDeviceModel::statusTextForDevice(const BluetoothDevice& device) const
{
    if (device.busy) {
        return tr("Working…");
    }
    else if (device.blocked) {
        return tr("Blocked");
    }
    else if (device.connected) {
        return device.trusted ? tr("Connected") : tr("Connected (not trusted)");
    }
    else if (device.paired) {
        return device.trusted ? tr("Paired") : tr("Paired (not trusted)");
    }
    else {
        return tr("Available to pair");
    }
}
