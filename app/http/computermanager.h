#pragma once
#include "nvhttp.h"


class ComputerManager
{
public:
    ComputerManager();

    void startPolling();

private:
    bool m_Polling;
};

class NvApp
{
public:
    int id;
    QString name;
    bool hdrSupported;
};

class NvComputer
{
public:
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

    // Persisted traits
    QString localAddress;
    QString remoteAddress;
    QString manualAddress;
    QByteArray macAddress;
    QString name;
    QString uuid;
    int serverCodecModeSupport;
    QVector<NvApp> appList;
};
