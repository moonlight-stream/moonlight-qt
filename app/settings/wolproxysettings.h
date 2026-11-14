#pragma once

#include <QHostAddress>
#include <QList>
#include <QString>

class WolProxySettings
{
public:
    static QList<QHostAddress> parseAddressList(const QString& addressList, bool* valid = nullptr);
    static bool isAddressListValid(const QString& addressList);
    static QString normalizeAddressList(const QString& addressList);
};
