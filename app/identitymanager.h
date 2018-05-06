#pragma once

#include <QDir>
#include <QSslConfiguration>

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

    QSslConfiguration
    getSslConfig();

private:
    QDir m_RootDirectory;

    QByteArray m_CachedPrivateKey;
    QByteArray m_CachedPemCert;
    QString m_CachedUniqueId;
};
