/**
 * @file app/backend/identitymanager.h
 * @brief Manages client identity, certificates, and SSL configuration.
 */

#pragma once

#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>
#include <QSettings>

/**
 * @brief Manages client identity, SSL certificates, and private keys for GameStream authentication.
 */
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

    QSslCertificate
    getSslCertificate();

    QSslKey
    getSslKey();

    void
    createCredentials(QSettings& settings);

    // Initialized in constructor
    QByteArray m_CachedPrivateKey;
    QByteArray m_CachedPemCert;

    // Lazy initialized
    QString m_CachedUniqueId;
    QSslCertificate m_CachedSslCert;
    QSslKey m_CachedSslKey;

    static IdentityManager* s_Im;
};
