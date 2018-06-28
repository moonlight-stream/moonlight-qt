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
        NOT_PAIRED,
        PAIRED,
        PIN_WRONG,
        FAILED,
        ALREADY_IN_PROGRESS
    };

    explicit NvPairingManager(QString address);

    ~NvPairingManager();

    QString
    generatePinString();

    PairState
    pair(QString serverInfo, QString pin);

private:
    QByteArray
    generateRandomBytes(int length);

    QByteArray
    saltPin(QByteArray salt, QString pin);

    QByteArray
    encrypt(QByteArray plaintext, AES_KEY* key);

    QByteArray
    decrypt(QByteArray ciphertext, AES_KEY* key);

    QByteArray
    getSignatureFromPemCert(QByteArray certificate);

    bool
    verifySignature(QByteArray data, QByteArray signature, QByteArray serverCertificate);

    QByteArray
    signMessage(QByteArray message);

    NvHTTP m_Http;
    X509* m_Cert;
    EVP_PKEY* m_PrivateKey;
};
