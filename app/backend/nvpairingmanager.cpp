#include "nvpairingmanager.h"
#include "utils.h"

#include <QRandomGenerator>

#include <openssl/bio.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

NvPairingManager::NvPairingManager(QString address) :
    m_Http(address)
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

QByteArray
NvPairingManager::getSignatureFromPemCert(QByteArray certificate)
{
    BIO* bio = BIO_new_mem_buf(certificate.data(), -1);
    THROW_BAD_ALLOC_IF_NULL(bio);

    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free_all(bio);

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
   ASN1_BIT_STRING *asnSignature;
#else
   const ASN1_BIT_STRING *asnSignature;
#endif

    X509_get0_signature(&asnSignature, NULL, cert);

    QByteArray signature(reinterpret_cast<char*>(asnSignature->data), asnSignature->length);

    X509_free(cert);

    return signature;
}

bool
NvPairingManager::verifySignature(QByteArray data, QByteArray signature, QByteArray serverCertificate)
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
    int result = EVP_DigestVerifyFinal(mdctx, reinterpret_cast<unsigned char*>(signature.data()), signature.length());

    EVP_PKEY_free(pubKey);
    EVP_MD_CTX_destroy(mdctx);
    X509_free(cert);

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

    QByteArray signature((int)signatureLength, 0);
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
NvPairingManager::pair(QString appVersion, QString pin)
{
    int serverMajorVersion = NvHTTP::parseQuad(appVersion).at(0);
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
                                                    salt.toHex() + "&clientcert=" + IdentityManager::get()->getCertificate().toHex(),
                                                    false);
    NvHTTP::verifyResponseStatus(getCert);
    if (NvHTTP::getXmlString(getCert, "paired") != "1")
    {
        qDebug() << "Failed pairing at stage #1";
        return PairState::FAILED;
    }

    QByteArray serverCert = NvHTTP::getXmlStringFromHex(getCert, "plaincert");
    if (serverCert == nullptr)
    {
        qDebug() << "Server likely already pairing";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, true);
        return PairState::ALREADY_IN_PROGRESS;
    }

    QByteArray randomChallenge = generateRandomBytes(16);
    QByteArray encryptedChallenge = encrypt(randomChallenge, &encKey);
    QString challengeXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttp,
                                                         "pair",
                                                         "devicename=roth&updateState=1&clientchallenge=" +
                                                         encryptedChallenge.toHex(),
                                                         true);
    NvHTTP::verifyResponseStatus(challengeXml);
    if (NvHTTP::getXmlString(challengeXml, "paired") != "1")
    {
        qDebug() << "Failed pairing at stage #2";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, true);
        return PairState::FAILED;
    }

    QByteArray challengeResponseData = decrypt(m_Http.getXmlStringFromHex(challengeXml, "challengeresponse"), &decKey);
    QByteArray clientSecretData = generateRandomBytes(16);
    QByteArray challengeResponse;
    QByteArray serverResponse(challengeResponseData.data(), hashLength);

 #if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    ASN1_BIT_STRING *asnSignature;
 #else
    const ASN1_BIT_STRING *asnSignature;
 #endif

    X509_get0_signature(&asnSignature, NULL, m_Cert);

    challengeResponse.append(challengeResponseData.data() + hashLength, 16);
    challengeResponse.append(reinterpret_cast<char*>(asnSignature->data), asnSignature->length);
    challengeResponse.append(clientSecretData);

    QByteArray encryptedChallengeResponseHash = encrypt(QCryptographicHash::hash(challengeResponse, hashAlgo), &encKey);
    QString respXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttp,
                                                    "pair",
                                                    "devicename=roth&updateState=1&serverchallengeresp=" +
                                                    encryptedChallengeResponseHash.toHex(),
                                                    true);
    NvHTTP::verifyResponseStatus(respXml);
    if (NvHTTP::getXmlString(respXml, "paired") != "1")
    {
        qDebug() << "Failed pairing at stage #3";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, true);
        return PairState::FAILED;
    }

    QByteArray pairingSecret = NvHTTP::getXmlStringFromHex(respXml, "pairingsecret");
    QByteArray serverSecret = QByteArray(pairingSecret.data(), 16);
    QByteArray serverSignature = QByteArray(&pairingSecret.data()[16], 256);

    if (!verifySignature(serverSecret,
                         serverSignature,
                         serverCert))
    {
        qDebug() << "MITM detected";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, true);
        return PairState::FAILED;
    }

    QByteArray expectedResponseData;
    expectedResponseData.append(randomChallenge);
    expectedResponseData.append(getSignatureFromPemCert(serverCert));
    expectedResponseData.append(serverSecret);
    if (QCryptographicHash::hash(expectedResponseData, hashAlgo) != serverResponse)
    {
        qDebug() << "Incorrect PIN";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, true);
        return PairState::PIN_WRONG;
    }

    QByteArray clientPairingSecret;
    clientPairingSecret.append(clientSecretData);
    clientPairingSecret.append(signMessage(clientSecretData));

    QString secretRespXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttp,
                                                          "pair",
                                                          "devicename=roth&updateState=1&clientpairingsecret=" +
                                                          clientPairingSecret.toHex(),
                                                          true);
    NvHTTP::verifyResponseStatus(secretRespXml);
    if (NvHTTP::getXmlString(secretRespXml, "paired") != "1")
    {
        qDebug() << "Failed pairing at stage #4";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, true);
        return PairState::FAILED;
    }

    QString pairChallengeXml = m_Http.openConnectionToString(m_Http.m_BaseUrlHttps,
                                                             "pair",
                                                             "devicename=roth&updateState=1&phrase=pairchallenge",
                                                             true);
    NvHTTP::verifyResponseStatus(pairChallengeXml);
    if (NvHTTP::getXmlString(pairChallengeXml, "paired") != "1")
    {
        qDebug() << "Failed pairing at stage #5";
        m_Http.openConnectionToString(m_Http.m_BaseUrlHttp, "unpair", nullptr, true);
        return PairState::FAILED;
    }

    return PairState::PAIRED;
}
