#pragma once

#include "identitymanager.h"
#include "nvhttp.h"

#include <openssl/aes.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

class NvPairingManager
{
public:
    enum PairState
    {
        PAIRED,
        PIN_WRONG,
        FAILED,
        ALREADY_IN_PROGRESS
    };

    explicit NvPairingManager(QString address);

    ~NvPairingManager();

    PairState
    pair(QString appVersion, QString pin, QSslCertificate& serverCert);

private:
    QByteArray
    generateRandomBytes(int length);

    QByteArray
    saltPin(const QByteArray& salt, QString pin);

    QByteArray
    encrypt(const QByteArray& plaintext, AES_KEY* key);

    QByteArray
    decrypt(const QByteArray& ciphertext, AES_KEY* key);

    QByteArray
    getSignatureFromPemCert(const QByteArray& certificate);

    bool
    verifySignature(const QByteArray& data, const QByteArray& signature, const QByteArray& serverCertificate);

    QByteArray
    signMessage(const QByteArray& message);

    NvHTTP m_Http;
    X509* m_Cert;
    EVP_PKEY* m_PrivateKey;
};
