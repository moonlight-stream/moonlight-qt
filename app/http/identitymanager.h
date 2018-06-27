#pragma once

#include <QDir>
#include <QSslConfiguration>

class IdentityManager
{
public:
    QString
    getUniqueId();

    QByteArray
    getCertificate();

    QByteArray
    getPrivateKey();

    QSslConfiguration
    getSslConfig();

    static
    IdentityManager*
    get();

private:
    IdentityManager();

    QDir m_RootDirectory;

    QByteArray m_CachedPrivateKey;
    QByteArray m_CachedPemCert;
    QString m_CachedUniqueId;

    static IdentityManager* s_Im;
};
