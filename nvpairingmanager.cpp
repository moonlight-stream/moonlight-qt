#include "nvpairingmanager.h"
#include "utils.h"

#include <QRandomGenerator>

#include <openssl/bio.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

NvPairingManager::NvPairingManager(QString address, IdentityManager im) :
    m_Http(address, im),
    m_Im(im)
{
    QByteArray cert = m_Im.getCertificate();
    BIO *bio = BIO_new_mem_buf(cert.data(), -1);
    THROW_BAD_ALLOC_IF_NULL(bio);

    m_Cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free_all(bio);
    if (m_Cert == nullptr)
    {
        throw new std::runtime_error("Unable to load certificate");
    }

    QByteArray pk = m_Im.getPrivateKey();
    bio = BIO_new_mem_buf(pk.data(), -1);
    THROW_BAD_ALLOC_IF_NULL(bio);

    PEM_read_bio_PrivateKey(bio, &m_PrivateKey, nullptr, nullptr);
    BIO_free_all(bio);
    if (m_Cert == nullptr)
    {
        throw new std::runtime_error("Unable to load private key");
    }
}

NvPairingManager::~NvPairingManager()
{
    X509_free(m_Cert);
    EVP_PKEY_free(m_PrivateKey);
}

QString
NvPairingManager::generatePinString()
{
    return QString::number(QRandomGenerator::global()->bounded(10000));
}

QByteArray
NvPairingManager::generateRandomBytes(int length)
{
    char* data = static_cast<char*>(alloca(length));
    RAND_bytes(reinterpret_cast<unsigned char*>(data), length);
    return QByteArray(data, length);
}

QByteArray
NvPairingManager::encrypt(QByteArray plaintext, AES_KEY* key)
{
    QByteArray ciphertext(plaintext.size(), 0);

    for (int i = 0; i < plaintext.size(); i += 16)
    {
        AES_encrypt(reinterpret_cast<unsigned char*>(&plaintext.data()[i]),
                    reinterpret_cast<unsigned char*>(&ciphertext.data()[i]),
                    key);
    }

    return ciphertext;
}

QByteArray
NvPairingManager::decrypt(QByteArray ciphertext, AES_KEY* key)
{
    QByteArray plaintext(ciphertext.size(), 0);

    for (int i = 0; i < plaintext.size(); i += 16)
    {
        AES_decrypt(reinterpret_cast<unsigned char*>(&ciphertext.data()[i]),
                    reinterpret_cast<unsigned char*>(&plaintext.data()[i]),
                    key);
    }

    return plaintext;
}

bool
NvPairingManager::verifySignature(QByteArray data, QByteArray signature)
{
    EVP_PKEY* pubKey = X509_get_pubkey(m_Cert);
    THROW_BAD_ALLOC_IF_NULL(pubKey);

    EVP_MD_CTX* mdctx = EVP_MD_CTX_create();
    THROW_BAD_ALLOC_IF_NULL(mdctx);

    EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, pubKey);
    EVP_DigestVerifyUpdate(mdctx, data.data(), data.length());
    int result = EVP_DigestVerifyFinal(mdctx, reinterpret_cast<unsigned char*>(signature.data()), signature.length());

    EVP_PKEY_free(pubKey);
    EVP_MD_CTX_destroy(mdctx);

    return result > 0;
}

QByteArray
NvPairingManager::signMessage(QByteArray message)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    THROW_BAD_ALLOC_IF_NULL(ctx);

    const EVP_MD *md = EVP_get_digestbyname("SHA256");
    THROW_BAD_ALLOC_IF_NULL(md);

    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestSignInit(ctx, NULL, md, NULL, m_PrivateKey);
    EVP_DigestSignUpdate(ctx, reinterpret_cast<unsigned char*>(message.data()), message.length());

    size_t signatureLength = 0;
    EVP_DigestSignFinal(ctx, NULL, &signatureLength);

    QByteArray signature(signatureLength, 0);
    EVP_DigestSignFinal(ctx, reinterpret_cast<unsigned char*>(signature.data()), &signatureLength);

    EVP_MD_CTX_destroy(ctx);

    return signature;
}

QByteArray
NvPairingManager::saltPin(QByteArray salt, QString pin)
{
    return QByteArray().append(salt).append(pin.toLatin1());
}

NvPairingManager::PairState
NvPairingManager::pair(QString serverInfo, QString pin)
{
    int serverMajorVersion = m_Http.getServerVersionQuad(serverInfo).at(0);
    qDebug() << "Pairing with server generation: " << serverMajorVersion;

    QCryptographicHash::Algorithm hashAlgo;
    int hashLength;
    if (serverMajorVersion >= 7)
    {
        // Gen 7+ uses SHA-256 hashing
        hashAlgo = QCryptographicHash::Sha256;
        hashLength = 32;
    }
    else
    {
        // Prior to Gen 7 uses SHA-1 hashing
        hashAlgo = QCryptographicHash::Sha1;
        hashLength = 20;
    }

    QByteArray salt = generateRandomBytes(16);
    QByteArray saltedPin = saltPin(salt, pin);

    AES_KEY encKey, decKey;
    AES_set_decrypt_key(reinterpret_cast<const unsigned char*>(QCryptographicHash::hash(saltedPin, hashAlgo).data()), 128, &decKey);
    AES_set_encrypt_key(reinterpret_cast<const unsigned char*>(QCryptographicHash::hash(saltedPin, hashAlgo).data()), 128, &encKey);

    QString getCert = m_Http.openConnectionToString(m_Http.m_BaseUrlHttp,
                                                    "pair",
                                                    "devicename=roth&updateState=1&phrase=getservercert&salt=" +
                                                    salt.toHex() + "&clientcert=" + m_Im.getCertificate().toHex(),
                                                    false);
    m_Http.verifyResponseStatus(getCert);
    if (m_Http.getXmlString(getCert, "paired") != "1")
    {
        qDebug() << "Failed pairing at stage #1";
        return PairState::FAILED;
    }

    QByteArray encryptedChallenge = encrypt(generateRandomBytes(16), &encKey);
    QString challengeXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttp,
                                                         "pair",
                                                         "devicename=roth&updateState=1&clientchallenge=" +
                                                         encryptedChallenge.toHex(),
                                                         true);
    m_Http.verifyResponseStatus(challengeXml);
    if (m_Http.getXmlString(challengeXml, "paired") != "1")
    {
        qDebug() << "Failed pairing at stage #2";
        return PairState::FAILED;
    }

    QByteArray challengeResponseData = decrypt(
                QByteArray::fromHex(m_Http.getXmlString(challengeXml, "challengeresponse").toLatin1()),
                &decKey);
    QByteArray clientSecretData = generateRandomBytes(16);
    QByteArray challengeResponse;

    const ASN1_BIT_STRING *asnSignature;
    X509_get0_signature(&asnSignature, NULL, m_Cert);

    challengeResponse.append(challengeResponseData.data() + hashLength, 16);
    challengeResponse.append(reinterpret_cast<char*>(asnSignature->data), 256);
    challengeResponse.append(clientSecretData);

    QByteArray encryptedChallengeResponseHash = encrypt(QCryptographicHash::hash(challengeResponse, hashAlgo), &encKey);
    QString respXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttp,
                                                    "pair",
                                                    "devicename=roth&updateState=1&serverchallengeresp=" +
                                                    encryptedChallengeResponseHash.toHex(),
                                                    true);
    m_Http.verifyResponseStatus(respXml);
    if (m_Http.getXmlString(respXml, "paired") != "1")
    {
        qDebug() << "Failed pairing at stage #3";
        return PairState::FAILED;
    }

    QByteArray pairingSecret =
           QByteArray::fromHex(m_Http.getXmlString(challengeXml, "pairingsecret").toLatin1());

    if (!verifySignature(QByteArray(pairingSecret.data(), 16),
                         QByteArray(&pairingSecret.data()[16], 256)))
    {
        qDebug() << "MITM detected";
        return PairState::FAILED;
    }

    QByteArray clientPairingSecret;
    clientPairingSecret.append(clientSecretData);
    clientPairingSecret.append(signMessage(clientSecretData));

    QString secretRespXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttp,
                                                          "pair",
                                                          "devicename=roth&updateState=1&clientpairingsecret=" +
                                                          clientPairingSecret.toHex(),
                                                          true);
    m_Http.verifyResponseStatus(secretRespXml);
    if (m_Http.getXmlString(secretRespXml, "paired") != "1")
    {
        qDebug() << "Failed pairing at stage #4";
        return PairState::FAILED;
    }

    QString pairChallengeXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttps,
                                                             "pair",
                                                             "devicename=roth&updateState=1&phase=pairchallenge",
                                                             true);
    m_Http.verifyResponseStatus(pairChallengeXml);
    if (m_Http.getXmlString(pairChallengeXml, "paired") != "1")
    {
        qDebug() << "Failed pairing at stage #5";
        return PairState::FAILED;
    }

    return PairState::PAIRED;
}
