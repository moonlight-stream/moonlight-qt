#include "nvpairingmanager.h"
#include "utils.h"

#include <stdexcept>

#include <openssl/bio.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

#define REQUEST_TIMEOUT_MS 5000

NvPairingManager::NvPairingManager(QString address) :
    m_Http(address, QSslCertificate())
{
    QByteArray cert = IdentityManager::get()->getCertificate();
    BIO *bio = BIO_new_mem_buf(cert.data(), -1);
    THROW_BAD_ALLOC_IF_NULL(bio);

    m_Cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free_all(bio);
    if (m_Cert == nullptr)
    {
        throw std::runtime_error("Unable to load certificate");
    }

    QByteArray pk = IdentityManager::get()->getPrivateKey();
    bio = BIO_new_mem_buf(pk.data(), -1);
    THROW_BAD_ALLOC_IF_NULL(bio);

    m_PrivateKey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free_all(bio);
    if (m_PrivateKey == nullptr)
    {
        throw std::runtime_error("Unable to load private key");
    }
}

NvPairingManager::~NvPairingManager()
{
    X509_free(m_Cert);
    EVP_PKEY_free(m_PrivateKey);
}

QByteArray
NvPairingManager::generateRandomBytes(int length)
{
    char* data = static_cast<char*>(alloca(length));
    RAND_bytes(reinterpret_cast<unsigned char*>(data), length);
    return QByteArray(data, length);
}

QByteArray
NvPairingManager::encrypt(const QByteArray& plaintext, AES_KEY* key)
{
    QByteArray ciphertext(plaintext.size(), 0);

    for (int i = 0; i < plaintext.size(); i += 16)
    {
        AES_encrypt(reinterpret_cast<unsigned char*>(const_cast<char*>(&plaintext.data()[i])),
                    reinterpret_cast<unsigned char*>(&ciphertext.data()[i]),
                    key);
    }

    return ciphertext;
}

QByteArray
NvPairingManager::decrypt(const QByteArray& ciphertext, AES_KEY* key)
{
    QByteArray plaintext(ciphertext.size(), 0);

    for (int i = 0; i < plaintext.size(); i += 16)
    {
        AES_decrypt(reinterpret_cast<unsigned char*>(const_cast<char*>(&ciphertext.data()[i])),
                    reinterpret_cast<unsigned char*>(&plaintext.data()[i]),
                    key);
    }

    return plaintext;
}

QByteArray
NvPairingManager::getSignatureFromPemCert(const QByteArray& certificate)
{
    BIO* bio = BIO_new_mem_buf(certificate.data(), -1);
    THROW_BAD_ALLOC_IF_NULL(bio);

    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free_all(bio);

#if (OPENSSL_VERSION_NUMBER < 0x10002000L)
    ASN1_BIT_STRING *asnSignature = cert->signature;
#elif (OPENSSL_VERSION_NUMBER < 0x10100000L)
    ASN1_BIT_STRING *asnSignature;
    X509_get0_signature(&asnSignature, NULL, cert);
#else
    const ASN1_BIT_STRING *asnSignature;
    X509_get0_signature(&asnSignature, NULL, cert);
#endif

    QByteArray signature(reinterpret_cast<char*>(asnSignature->data), asnSignature->length);

    X509_free(cert);

    return signature;
}

bool
NvPairingManager::verifySignature(const QByteArray& data, const QByteArray& signature, const QByteArray& serverCertificate)
{
    BIO* bio = BIO_new_mem_buf(serverCertificate.data(), -1);
    THROW_BAD_ALLOC_IF_NULL(bio);

    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free_all(bio);

    EVP_PKEY* pubKey = X509_get_pubkey(cert);
    THROW_BAD_ALLOC_IF_NULL(pubKey);

    EVP_MD_CTX* mdctx = EVP_MD_CTX_create();
    THROW_BAD_ALLOC_IF_NULL(mdctx);

    EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, pubKey);
    EVP_DigestVerifyUpdate(mdctx, data.data(), data.length());
    int result = EVP_DigestVerifyFinal(mdctx, reinterpret_cast<unsigned char*>(const_cast<char*>(signature.data())), signature.length());

    EVP_PKEY_free(pubKey);
    EVP_MD_CTX_destroy(mdctx);
    X509_free(cert);

    return result > 0;
}

QByteArray
NvPairingManager::signMessage(const QByteArray& message)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    THROW_BAD_ALLOC_IF_NULL(ctx);

    const EVP_MD *md = EVP_get_digestbyname("SHA256");
    THROW_BAD_ALLOC_IF_NULL(md);

    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestSignInit(ctx, NULL, md, NULL, m_PrivateKey);
    EVP_DigestSignUpdate(ctx, reinterpret_cast<unsigned char*>(const_cast<char*>(message.data())), message.length());

    size_t signatureLength = 0;
    EVP_DigestSignFinal(ctx, NULL, &signatureLength);

    QByteArray signature((int)signatureLength, 0);
    EVP_DigestSignFinal(ctx, reinterpret_cast<unsigned char*>(signature.data()), &signatureLength);

    EVP_MD_CTX_destroy(ctx);

    return signature;
}

QByteArray
NvPairingManager::saltPin(const QByteArray& salt, QString pin)
{
    return QByteArray().append(salt).append(pin.toLatin1());
}

NvPairingManager::PairState
NvPairingManager::pair(QString appVersion, QString pin, QSslCertificate& serverCert)
{
    int serverMajorVersion = NvHTTP::parseQuad(appVersion).at(0);
    qInfo() << "Pairing with server generation:" << serverMajorVersion;

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
                                                    salt.toHex() + "&clientcert=" + IdentityManager::get()->getCertificate().toHex(),
                                                    0);
    NvHTTP::verifyResponseStatus(getCert);
    if (NvHTTP::getXmlString(getCert, "paired") != "1")
    {
        qCritical() << "Failed pairing at stage #1";
        return PairState::FAILED;
    }

    QByteArray serverCertStr = NvHTTP::getXmlStringFromHex(getCert, "plaincert");
    if (serverCertStr == nullptr)
    {
        qCritical() << "Server likely already pairing";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, REQUEST_TIMEOUT_MS);
        return PairState::ALREADY_IN_PROGRESS;
    }

    serverCert = QSslCertificate(serverCertStr);
    if (serverCert.isNull()) {
        Q_ASSERT(!serverCert.isNull());

        qCritical() << "Failed to parse plaincert";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, REQUEST_TIMEOUT_MS);
        return PairState::FAILED;
    }

    // Pin this cert for TLS
    m_Http.setServerCert(serverCert);

    QByteArray randomChallenge = generateRandomBytes(16);
    QByteArray encryptedChallenge = encrypt(randomChallenge, &encKey);
    QString challengeXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttp,
                                                         "pair",
                                                         "devicename=roth&updateState=1&clientchallenge=" +
                                                         encryptedChallenge.toHex(),
                                                         REQUEST_TIMEOUT_MS);
    NvHTTP::verifyResponseStatus(challengeXml);
    if (NvHTTP::getXmlString(challengeXml, "paired") != "1")
    {
        qCritical() << "Failed pairing at stage #2";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, REQUEST_TIMEOUT_MS);
        return PairState::FAILED;
    }

    QByteArray challengeResponseData = decrypt(m_Http.getXmlStringFromHex(challengeXml, "challengeresponse"), &decKey);
    QByteArray clientSecretData = generateRandomBytes(16);
    QByteArray challengeResponse;
    QByteArray serverResponse(challengeResponseData.data(), hashLength);

#if (OPENSSL_VERSION_NUMBER < 0x10002000L)
    ASN1_BIT_STRING *asnSignature = m_Cert->signature;
#elif (OPENSSL_VERSION_NUMBER < 0x10100000L)
    ASN1_BIT_STRING *asnSignature;
    X509_get0_signature(&asnSignature, NULL, m_Cert);
#else
    const ASN1_BIT_STRING *asnSignature;
    X509_get0_signature(&asnSignature, NULL, m_Cert);
#endif

    challengeResponse.append(challengeResponseData.data() + hashLength, 16);
    challengeResponse.append(reinterpret_cast<char*>(asnSignature->data), asnSignature->length);
    challengeResponse.append(clientSecretData);

    QByteArray paddedHash = QCryptographicHash::hash(challengeResponse, hashAlgo);
    paddedHash.resize(32);
    QByteArray encryptedChallengeResponseHash = encrypt(paddedHash, &encKey);
    QString respXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttp,
                                                    "pair",
                                                    "devicename=roth&updateState=1&serverchallengeresp=" +
                                                    encryptedChallengeResponseHash.toHex(),
                                                    REQUEST_TIMEOUT_MS);
    NvHTTP::verifyResponseStatus(respXml);
    if (NvHTTP::getXmlString(respXml, "paired") != "1")
    {
        qCritical() << "Failed pairing at stage #3";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, REQUEST_TIMEOUT_MS);
        return PairState::FAILED;
    }

    QByteArray pairingSecret = NvHTTP::getXmlStringFromHex(respXml, "pairingsecret");
    QByteArray serverSecret = QByteArray(pairingSecret.data(), 16);
    QByteArray serverSignature = QByteArray(&pairingSecret.data()[16], 256);

    if (!verifySignature(serverSecret,
                         serverSignature,
                         serverCertStr))
    {
        qCritical() << "MITM detected";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, REQUEST_TIMEOUT_MS);
        return PairState::FAILED;
    }

    QByteArray expectedResponseData;
    expectedResponseData.append(randomChallenge);
    expectedResponseData.append(getSignatureFromPemCert(serverCertStr));
    expectedResponseData.append(serverSecret);
    if (QCryptographicHash::hash(expectedResponseData, hashAlgo) != serverResponse)
    {
        qCritical() << "Incorrect PIN";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, REQUEST_TIMEOUT_MS);
        return PairState::PIN_WRONG;
    }

    QByteArray clientPairingSecret;
    clientPairingSecret.append(clientSecretData);
    clientPairingSecret.append(signMessage(clientSecretData));

    QString secretRespXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttp,
                                                          "pair",
                                                          "devicename=roth&updateState=1&clientpairingsecret=" +
                                                          clientPairingSecret.toHex(),
                                                          REQUEST_TIMEOUT_MS);
    NvHTTP::verifyResponseStatus(secretRespXml);
    if (NvHTTP::getXmlString(secretRespXml, "paired") != "1")
    {
        qCritical() << "Failed pairing at stage #4";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, REQUEST_TIMEOUT_MS);
        return PairState::FAILED;
    }

    QString pairChallengeXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttps,
                                                             "pair",
                                                             "devicename=roth&updateState=1&phrase=pairchallenge",
                                                             REQUEST_TIMEOUT_MS);
    NvHTTP::verifyResponseStatus(pairChallengeXml);
    if (NvHTTP::getXmlString(pairChallengeXml, "paired") != "1")
    {
        qCritical() << "Failed pairing at stage #5";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, REQUEST_TIMEOUT_MS);
        return PairState::FAILED;
    }

    return PairState::PAIRED;
}
