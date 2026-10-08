#include "wolproxysettings.h"

#include <QSet>
#include <QStringList>

QList<QHostAddress> WolProxySettings::parseAddressList(const QString& addressList, bool* valid)
{
    QList<QHostAddress> addresses;
    QSet<QString> seenAddresses;

    if (addressList.trimmed().isEmpty()) {
        if (valid) {
            *valid = true;
        }
        return addresses;
    }

    const QStringList entries = addressList.split(',');
    for (const QString& rawEntry : entries) {
        const QString entry = rawEntry.trimmed();
        QHostAddress address;

        if (entry.isEmpty() || !address.setAddress(entry)) {
            if (valid) {
                *valid = false;
            }
            return {};
        }

        const QString normalizedAddress = address.toString();
        if (!seenAddresses.contains(normalizedAddress)) {
            seenAddresses.insert(normalizedAddress);
            addresses.append(address);
        }
    }

    if (valid) {
        *valid = true;
    }
    return addresses;
}

bool WolProxySettings::isAddressListValid(const QString& addressList)
{
    bool valid;
    parseAddressList(addressList, &valid);
    return valid;
}

QString WolProxySettings::normalizeAddressList(const QString& addressList)
{
    bool valid;
    const QList<QHostAddress> addresses = parseAddressList(addressList, &valid);
    if (!valid) {
        return {};
    }

    QStringList normalizedAddresses;
    normalizedAddresses.reserve(addresses.size());
    for (const QHostAddress& address : addresses) {
        normalizedAddresses.append(address.toString());
    }
    return normalizedAddresses.join(", ");
}
