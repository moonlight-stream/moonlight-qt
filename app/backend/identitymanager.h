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

    // Human-readable description of a problem found with the client identity at
    // startup that the user should be told about, or empty if none
    QString
    getIdentityProblem() const;

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

    // Each loadIdentityFrom*() function loads the client identity from one source.
    // On success, it sets the identity manager's identity members and returns
    // true. On failure, it returns false and sets error to a reason that is fatal.

    // Loads the identity encoded in the cert and key settings. Never fails,
    // since a missing or unusable identity is replaced with a new one.
    bool
    loadIdentityFromSettings(QSettings& settings, QString* error);

    // Initialized in constructor
    QByteArray m_CachedPrivateKey;
    QByteArray m_CachedPemCert;

    QString m_IdentityProblem;

    // Lazy initialized
    QString m_CachedUniqueId;
    QSslCertificate m_CachedSslCert;
    QSslKey m_CachedSslKey;

    static IdentityManager* s_Im;
};
