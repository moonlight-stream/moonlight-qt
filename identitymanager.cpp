#include "identitymanager.h"
#include "utils.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QSslCertificate>
#include <QSslKey>

#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/pkcs12.h>

IdentityManager::IdentityManager(QDir directory)
{
    QFile uniqueIdFile(directory.filePath("uniqueid"));
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

        // Make sure it really loads
        if (!QSslKey(m_CachedPrivateKey, QSsl::Rsa).isNull() && !QSslCertificate(m_CachedPemCert).isNull())
        {
            qDebug() << "Loaded cached identity key pair from disk";
            return;
        }

        qDebug() << "Regenerating corrupted local key pair";
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
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 60 * 60 * 24 * 365 * 20); // 20 yrs
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
    m_CachedPrivateKey = QByteArray(mem->data, mem->length);
    QTextStream(&privateKeyFile) << m_CachedPrivateKey;

    BIO_get_mem_ptr(biocert, &mem);
    m_CachedPemCert = QByteArray(mem->data, mem->length);
    QTextStream(&certificateFile) << m_CachedPemCert;

    X509_free(cert);
    EVP_PKEY_free(pk);
    BN_free(bne);
    BIO_free(biokey);
    BIO_free(biocert);

    // Ensure we can actually consume the keys we just wrote
    assert(!QSslCertificate(m_CachedPemCert).isNull());
    assert(!QSslKey(m_CachedPrivateKey, QSsl::Rsa).isNull());

    qDebug() << "Wrote new identity credentials to disk";
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
