#include "identitymanager.h"
#include "utils.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QSslCertificate>
#include <QSslKey>
#include <QStandardPaths>

#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/x509.h>

IdentityManager* IdentityManager::s_Im = nullptr;

IdentityManager*
IdentityManager::get()
{
    // This will always be called first on the main thread,
    // so it's safe to initialize without locks.
    if (s_Im == nullptr) {
        s_Im = new IdentityManager();
    }

    return s_Im;
}

IdentityManager::IdentityManager()
{
    m_RootDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    if (!m_RootDirectory.exists())
    {
        m_RootDirectory.mkpath(".");
    }

    QFile uniqueIdFile(m_RootDirectory.filePath("uniqueid"));
    if (uniqueIdFile.open(QIODevice::ReadOnly))
    {
        m_CachedUniqueId = QTextStream(&uniqueIdFile).readAll();
        qDebug() << "Loaded cached unique ID: " << m_CachedUniqueId;
    }
    else
    {
        for (int i = 0; i < 16; i++)
        {
            int n = qrand() % 16;
            m_CachedUniqueId.append(QString::number(n, 16));
        }

        qDebug() << "Generated new unique ID: " << m_CachedUniqueId;

        uniqueIdFile.open(QIODevice::ReadWrite);
        QTextStream(&uniqueIdFile) << m_CachedUniqueId;
    }

    QFile certificateFile(m_RootDirectory.filePath("cert"));
    QFile privateKeyFile(m_RootDirectory.filePath("key"));

    if (certificateFile.open(QIODevice::ReadOnly) && privateKeyFile.open(QIODevice::ReadOnly))
    {
        // Not cached yet, but it's on disk
        m_CachedPemCert = certificateFile.readAll();
        m_CachedPrivateKey = privateKeyFile.readAll();

        qDebug() << "Loaded cached identity key pair from disk";
        return;
    }

    privateKeyFile.close();
    certificateFile.close();

    X509* cert = X509_new();
    THROW_BAD_ALLOC_IF_NULL(cert);

    EVP_PKEY* pk = EVP_PKEY_new();
    THROW_BAD_ALLOC_IF_NULL(pk);

    BIGNUM* bne = BN_new();
    THROW_BAD_ALLOC_IF_NULL(bne);

    RSA* rsa = RSA_new();
    THROW_BAD_ALLOC_IF_NULL(rsa);

    BN_set_word(bne, RSA_F4);
    RSA_generate_key_ex(rsa, 2048, bne, nullptr);

    EVP_PKEY_assign_RSA(pk, rsa);

    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 0);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 60 * 60 * 24 * 365 * 20); // 20 yrs
#else
    ASN1_TIME* before = ASN1_STRING_dup(X509_get0_notBefore(cert));
    THROW_BAD_ALLOC_IF_NULL(before);
    ASN1_TIME* after = ASN1_STRING_dup(X509_get0_notAfter(cert));
    THROW_BAD_ALLOC_IF_NULL(after);

    X509_gmtime_adj(before, 0);
    X509_gmtime_adj(after, 60 * 60 * 24 * 365 * 20); // 20 yrs

    X509_set1_notBefore(cert, before);
    X509_set1_notAfter(cert, after);

    ASN1_STRING_free(before);
    ASN1_STRING_free(after);
#endif

    X509_set_pubkey(cert, pk);

    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<unsigned char *>(const_cast<char*>("NVIDIA GameStream Client")), -1, -1, 0);
    X509_set_issuer_name(cert, name);

    X509_sign(cert, pk, EVP_sha1());

    BIO* biokey = BIO_new(BIO_s_mem());
    THROW_BAD_ALLOC_IF_NULL(biokey);
    PEM_write_bio_PrivateKey(biokey, pk, NULL, NULL, 0, NULL, NULL);

    BIO* biocert = BIO_new(BIO_s_mem());
    THROW_BAD_ALLOC_IF_NULL(biocert);
    PEM_write_bio_X509(biocert, cert);

    privateKeyFile.open(QIODevice::WriteOnly);
    certificateFile.open(QIODevice::WriteOnly);

    BUF_MEM* mem;
    BIO_get_mem_ptr(biokey, &mem);
    m_CachedPrivateKey = QByteArray(mem->data, (int)mem->length);
    QTextStream(&privateKeyFile) << m_CachedPrivateKey;

    BIO_get_mem_ptr(biocert, &mem);
    m_CachedPemCert = QByteArray(mem->data, (int)mem->length);
    QTextStream(&certificateFile) << m_CachedPemCert;

    X509_free(cert);
    EVP_PKEY_free(pk);
    BN_free(bne);
    BIO_free(biokey);
    BIO_free(biocert);

    qDebug() << "Wrote new identity credentials to disk";
}

QSslConfiguration
IdentityManager::getSslConfig()
{
    BIO* bio = BIO_new_mem_buf(m_CachedPrivateKey.data(), -1);
    THROW_BAD_ALLOC_IF_NULL(bio);

    EVP_PKEY* pk = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    bio = BIO_new(BIO_s_mem());
    THROW_BAD_ALLOC_IF_NULL(bio);

    // We must write out our PEM in the old PKCS1 format for SecureTransport
    // on macOS/iOS to be able to read it.
    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    PEM_write_bio_PrivateKey_traditional(bio, pk, nullptr, nullptr, 0, nullptr, 0);

    QSslCertificate cert = QSslCertificate(m_CachedPemCert);
    QSslKey key = QSslKey(QByteArray::fromRawData(mem->data, (int)mem->length), QSsl::Rsa);
    Q_ASSERT(!cert.isNull());
    Q_ASSERT(!key.isNull());

    BIO_free(bio);
    EVP_PKEY_free(pk);

    QSslConfiguration sslConfig(QSslConfiguration::defaultConfiguration());
    sslConfig.setLocalCertificate(cert);
    sslConfig.setPrivateKey(key);

    return sslConfig;
}

QString
IdentityManager::getUniqueId()
{
    return m_CachedUniqueId;
}

QByteArray
IdentityManager::getCertificate()
{
    return m_CachedPemCert;
}

QByteArray
IdentityManager::getPrivateKey()
{
    return m_CachedPrivateKey;
}
