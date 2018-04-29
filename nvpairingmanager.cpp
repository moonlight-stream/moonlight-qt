#include "nvpairingmanager.h"

#include <QRandomGenerator>

#include <openssl/aes.h>

NvPairingManager::NvPairingManager(QString address) :
    m_Http(address)
{

}

QString
NvPairingManager::generatePinString()
{
    return QString::number(QRandomGenerator::global()->bounded(10000));
}

QByteArray
NvPairingManager::generateRandomBytes(int length)
{
    QByteArray array(length);

    for (int i = 0; i < length; i++)
    {
        array.data()[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }

    return array;
}

QByteArray
NvPairingManager::saltPin(QByteArray salt, QString pin)
{
    return QByteArray().append(salt).append(pin.toStdString().c_str());
}

NvPairingManager::PairState
NvPairingManager::pair(QString serverInfo, QString pin)
{
    int serverMajorVersion = m_Http.getServerVersionQuad(serverInfo).at(0);
    qDebug() << "Pairing with server generation: " << serverMajorVersion;

    QCryptographicHash::Algorithm hashAlgo;
    if (serverMajorVersion >= 7)
    {
        // Gen 7+ uses SHA-256 hashing
        hashAlgo = QCryptographicHash::Sha256;
    }
    else
    {
        // Prior to Gen 7 uses SHA-1 hashing
        hashAlgo = QCryptographicHash::Sha1;
    }

    QByteArray salt = generateRandomBytes(16);
    QByteArray saltedPin = saltPin(salt, pin);

    AES_KEY encKey, decKey;
    AES_set_decrypt_key(QCryptographicHash::hash(saltedPin, hashAlgo).data(), 128, &decKey);
    AES_set_encrypt_key(QCryptographicHash::hash(saltedPin, hashAlgo).data(), 128, &encKey);

    QString getCert = m_Http.openConnectionToString(m_Http.m_BaseUrlHttp,
                                                    "pair",
                                                    "&devicename=roth&updateState=1&phrase=getservercert&salt=" +
                                                    salt.toHex() + "&clientcert=" + )
}
