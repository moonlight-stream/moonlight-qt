#include "identitymanager.h"

#include <QFile>

#include <openssl/pem.h>
#include <openssl/conf.h>
#include <openssl/pkcs12.h>

IdentityManager::IdentityManager(QDir directory)
    : m_RootDirectory(directory),
      m_CachedPrivateKey(nullptr),
      m_CachedPemCert(nullptr),
      m_CachedUniqueId(nullptr)
{
}

QString
IdentityManager::getUniqueId()
{
    if (m_CachedUniqueId == nullptr)
    {
        QFile uniqueIdFile(m_RootDirectory.filePath("uniqueid"));
        if (uniqueIdFile.open(QIODevice::ReadOnly))
        {
            m_CachedUniqueId = QTextStream(uniqueIdFile.open(QIODevice::ReadOnly)).readAll();
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
            QTextStream(uniqueIdFile) << m_CachedUniqueId;
        }
    }

    return m_CachedUniqueId;
}

void
IdentityManager::loadKeyPair()
{
    if (m_CachedPemCert != nullptr && m_CachedPrivateKey != nullptr)
    {
        // Already have cached data
        return;
    }

    QFile certificateFile(m_RootDirectory.filePath("cert"));
    QFile privateKeyFile(m_RootDirectory.filePath("key"));

    if (certificateFile.open(QIODevice::ReadOnly) && privateKeyFile.open(QIODevice::ReadOnly))
    {
        // Not cached yet, but it's on disk
        m_CachedPemCert = certificateFile.readAll();
        m_CachedPrivateKey = privateKeyFile.readAll();

        qDebug() << "Loaded cached identity credentials from disk";
        return;
    }

    X509* cert = X509_new();
    if (cert == nullptr)
    {
        throw new std::bad_alloc();
    }

    EVP_PKEY* pk = EVP_PKEY_new();
    if (pk == nullptr)
    {
        X509_free(cert);
        throw new std::bad_alloc();
    }

    EVP_PKEY_assign_RSA(pk, RSA_generate_key(2048, RSA_F4, nullptr, nullptr));

    X509_set_version(cert, 2);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get0_notAfter(cert), 60 * 60 * 24 * 365 * 20); // 20 yrs
    X509_set_pubkey(cert, pk);

    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<unsigned char *>("NVIDIA GameStream Client"), -1, -1, 0);
    X509_set_issuer_name(x, name);

    privateKeyFile.open(QIODevice::ReadWrite);
    PEM_write_PrivateKey(privateKeyFile.handle(), pk, nullptr, nullptr, 0, nullptr, nullptr);

    certificateFile.open(QIODevice::ReadWrite);
    PEM_write_X509(certificateFile.handle(), cert);

    qDebug() << "Wrote new identity credentials to disk";
}

QByteArray
IdentityManager::getCertificate()
{
    loadKeyPair();
    return m_CachedPemCert;
}

QByteArray
IdentityManager::getPrivateKey()
{
    loadKeyPair();
    return m_CachedPrivateKey;
}
