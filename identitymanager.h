#pragma once


class IdentityManager
{
public:
    IdentityManager(QDir directory);

private:
    QDir m_RootDirectory;

    QString m_CachedUniqueId;
    QByteArray m_CachedPemCert;
    QByteArray m_CachedPrivateKey;
};
