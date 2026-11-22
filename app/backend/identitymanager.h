#pragma once

#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>
#include <QSettings>

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

    // Checks that a PEM certificate (leaf first, optionally followed by intermediates)
    // and an unencrypted PEM private key parse and belong together. On failure,
    // returns false and sets error to a human-readable reason.
    static
    bool
    validateIdentity(const QByteArray& pemCert, const QByteArray& pemKey, QString* error);

private:
    IdentityManager();

    QSslCertificate
    getSslCertificate();

    QSslKey
    getSslKey();

    static
    QSslKey
    pemToSslKey(const QByteArray& pemKey, QSsl::KeyAlgorithm algorithm);

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
