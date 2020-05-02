#pragma once

#include "nvhttp.h"

#include <QThread>
#include <QReadWriteLock>
#include <QSettings>
#include <QRunnable>

class NvComputer
{
    friend class PcMonitorThread;
    friend class ComputerManager;
    friend class PendingQuitTask;

private:
    void sortAppList();

    bool pendingQuit;

public:
    explicit NvComputer(QString address, QString serverInfo, QSslCertificate serverCert);

    explicit NvComputer(QSettings& settings);

    bool
    update(NvComputer& that);

    bool
    wake();

    bool
    isReachableOverVpn();

    QVector<QString>
    uniqueAddresses() const;

    void
    serialize(QSettings& settings) const;

    enum PairState
    {
        PS_UNKNOWN,
        PS_PAIRED,
        PS_NOT_PAIRED
    };

    enum ComputerState
    {
        CS_UNKNOWN,
        CS_ONLINE,
        CS_OFFLINE
    };

    // Ephemeral traits
    ComputerState state;
    PairState pairState;
    QString activeAddress;
    int currentGameId;
    QString gfeVersion;
    QString appVersion;
    QVector<NvDisplayMode> displayModes;
    int maxLumaPixelsHEVC;
    int serverCodecModeSupport;
    QString gpuModel;

    // Persisted traits
    QString localAddress;
    QString remoteAddress;
    QString ipv6Address;
    QString manualAddress;
    QByteArray macAddress;
    QString name;
    bool hasCustomName;
    QString uuid;
    QSslCertificate serverCert;
    QVector<NvApp> appList;

    // Synchronization
    mutable QReadWriteLock lock;
};
