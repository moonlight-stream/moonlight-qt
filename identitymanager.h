#pragma once

#include <QDir>

class IdentityManager
{
public:
    IdentityManager();

    QString
    getUniqueId();

    QByteArray
    getCertificate();

    QByteArray
    getPrivateKey();

private:
    QDir m_RootDirectory;

    QByteArray m_CachedPrivateKey;
    QByteArray m_CachedPemCert;
    QString m_CachedUniqueId;
};
