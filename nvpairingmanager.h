#pragma once

#include <nvhttp.h>

class NvPairingManager
{
public:
    enum PairState
    {
        NOT_PAIRED,
        PAIRED,
        PIN_WRONG,
        FAILED,
        ALREADY_IN_PROGRESS
    };

    NvPairingManager(QString address);

    QString
    generatePinString();

    PairState
    pair(QString serverInfo, QString pin);

private:
    NvHTTP m_Http;
};
