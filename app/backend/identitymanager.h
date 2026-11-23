#pragma once

#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>
#include <QSettings>

class IdentityManager
{
public:
    // Where the client identity comes from
    enum class IdentitySource
    {
        Settings,  // The identity encoded in the cert and key settings
        Files,     // The PEM files named by the certificatefile and keyfile settings
    };

    // The source the settings call for: Files if certificatefile or keyfile is
    // set, otherwise Settings
    static
    IdentitySource
    configuredSource();

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

    // Creates the identity manager with the client identity from source. Returns
    // false and sets error if the identity can't be loaded from that source, which
    // is fatal since Moonlight never substitutes another identity for it.
    static
    bool
    initialize(IdentitySource source, QString* error);

    // Checks that a PEM certificate (leaf first, optionally followed by intermediates)
    // and an unencrypted PEM private key parse and belong together. On failure,
    // returns false and sets error to a human-readable reason.
    static
    bool
    validateIdentity(const QByteArray& pemCert, const QByteArray& pemKey, QString* error);

    // One-line summary (subject, issuer, serial, validity, SANs, SHA-256 of the
    // certificate and of its public key) for diagnostics
    static
    QString
    describeCertificate(const QSslCertificate& cert);

    // Human-readable description of a problem found with the client identity at
    // startup that the user should be told about, or empty if none
    QString
    getIdentityProblem() const;

private:
    explicit
    IdentityManager(IdentitySource source);

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

    // Loads the identity from the PEM files named by the certificatefile and
    // keyfile settings
    bool
    loadIdentityFromFiles(QSettings& settings, QString* error);

    // Logs the certificate chain and the key type, never the key material itself
    static
    void
    logIdentityDetails(const QString& source, const QByteArray& pemCert, const QByteArray& pemKey);

    // Initialized in constructor
    QByteArray m_CachedPrivateKey;
    QByteArray m_CachedPemCert;

    QString m_IdentityProblem;
    QString m_FatalError;
    QString m_IdentitySource;

    // Lazy initialized
    QString m_CachedUniqueId;
    QSslCertificate m_CachedSslCert;
    QSslKey m_CachedSslKey;

    static IdentityManager* s_Im;
};
