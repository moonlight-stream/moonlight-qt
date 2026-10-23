#pragma once

#include "backend/bluetoothmanager.h"

#include <QAbstractListModel>

class BluetoothDeviceModel : public QAbstractListModel
{
    Q_OBJECT

    enum Roles
    {
        PathRole = Qt::UserRole,
        NameRole,
        AddressRole,
        PairedRole,
        TrustedRole,
        ConnectedRole,
        BusyRole,
        StatusTextRole
    };

public:
    explicit BluetoothDeviceModel(QObject* parent = nullptr);

    // Must be called before other functions
    Q_INVOKABLE void initialize(BluetoothManager* manager);

    QVariant data(const QModelIndex& index, int role) const override;

    int rowCount(const QModelIndex& parent) const override;

    QHash<int, QByteArray> roleNames() const override;

private slots:
    void handleDeviceAdded(int index);
    void handleDeviceRemoved(int index);
    void handleDeviceChanged(int index);
    void handleDevicesReset();

private:
    QString statusTextForDevice(const BluetoothDevice& device) const;

    QVector<BluetoothDevice> m_Devices;
    BluetoothManager* m_Manager;
};
