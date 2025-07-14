#include "otppairingmanager.h"
#include "nvcomputer.h"
#include "nvhttp.h"
#include "nvpairingmanager.h"
#include <QDebug>
#include <QCryptographicHash>

OTPPairingManager::OTPPairingManager(QObject *parent)
    : QObject(parent)
    , m_pairingManager(nullptr)
    , m_currentComputer(nullptr)
    , m_pairingInProgress(false)
{
    qDebug() << "OTPPairingManager: Initialized";
}

OTPPairingManager::~OTPPairingManager()
{
    cancelPairing();
    qDebug() << "OTPPairingManager: Destroyed";
}

void OTPPairingManager::startOTPPairing(NvComputer *computer, const QString &pin, const QString &passphrase)
{
    if (!computer) {
        emit pairingFailed("No computer specified");
        return;
    }

    if (!isOTPSupported(computer)) {
        emit pairingFailed("OTP pairing is only available with Apollo servers");
        return;
    }

    if (!validatePinFormat(pin)) {
        emit pairingFailed("PIN must be exactly 4 digits");
        return;
    }

    if (m_pairingInProgress) {
        emit pairingFailed("Pairing already in progress");
        return;
    }

    m_currentComputer = computer;
    m_currentPin = pin;
    m_currentPassphrase = passphrase;
    m_pairingInProgress = true;

    emit pairingStarted();
    emit pairingProgress("Starting OTP pairing...");

    qDebug() << "OTPPairingManager: Starting OTP pairing with" << computer->name;

    // Perform the actual OTP pairing
    performOTPPairing(computer, pin, passphrase);
}

void OTPPairingManager::cancelPairing()
{
    if (!m_pairingInProgress) {
        return;
    }

    m_pairingInProgress = false;
    m_currentComputer = nullptr;
    m_currentPin.clear();
    m_currentPassphrase.clear();

    qDebug() << "OTPPairingManager: Pairing cancelled";
}

bool OTPPairingManager::isPairingInProgress() const
{
    return m_pairingInProgress;
}

bool OTPPairingManager::isOTPSupported(NvComputer *computer) const
{
    if (!computer) {
        return false;
    }

    // OTP pairing is only available with Apollo servers
    // Check server info or version to determine if it's Apollo
    return computer->serverInfo.contains("Apollo", Qt::CaseInsensitive) ||
           computer->serverInfo.contains("Sunshine", Qt::CaseInsensitive);
}

void OTPPairingManager::setPairingManager(NvPairingManager *pairingManager)
{
    if (m_pairingManager) {
        // Disconnect from old pairing manager
        disconnect(m_pairingManager, nullptr, this, nullptr);
    }

    m_pairingManager = pairingManager;

    if (m_pairingManager) {
        // Connect to pairing manager signals
        connect(m_pairingManager, &NvPairingManager::pairingCompleted,
                this, &OTPPairingManager::onStandardPairingCompleted);
        connect(m_pairingManager, &NvPairingManager::pairingFailed,
                this, &OTPPairingManager::onStandardPairingFailed);
    }
}

void OTPPairingManager::onStandardPairingCompleted(bool success, const QString &message)
{
    if (!m_pairingInProgress) {
        return;
    }

    m_pairingInProgress = false;

    if (success) {
        emit pairingCompleted(true, "OTP pairing completed successfully");
        qDebug() << "OTPPairingManager: OTP pairing completed successfully";
    } else {
        emit pairingFailed("OTP pairing failed: " + message);
        qDebug() << "OTPPairingManager: OTP pairing failed:" << message;
    }

    // Clean up
    m_currentComputer = nullptr;
    m_currentPin.clear();
    m_currentPassphrase.clear();
}

void OTPPairingManager::onStandardPairingFailed(const QString &error)
{
    if (!m_pairingInProgress) {
        return;
    }

    m_pairingInProgress = false;
    emit pairingFailed("OTP pairing failed: " + error);

    qDebug() << "OTPPairingManager: OTP pairing failed:" << error;

    // Clean up
    m_currentComputer = nullptr;
    m_currentPin.clear();
    m_currentPassphrase.clear();
}

QString OTPPairingManager::generateOTPHash(const QString &pin, const QString &salt, const QString &passphrase)
{
    // Matches Android implementation:
    // MessageDigest digest = MessageDigest.getInstance("SHA-256");
    // String plainText = pin + saltStr + passphrase;
    // byte[] hash = digest.digest(plainText.getBytes());

    QString plainText = pin + salt + passphrase;
    
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(plainText.toUtf8());
    
    QByteArray result = hash.result();
    
    // Convert to hex string (uppercase to match Android)
    QString hexString = result.toHex().toUpper();
    
    qDebug() << "OTPPairingManager: Generated OTP hash for PIN:" << pin << "Salt:" << salt;
    
    return hexString;
}

void OTPPairingManager::performOTPPairing(NvComputer *computer, const QString &pin, const QString &passphrase)
{
    if (!m_pairingManager) {
        emit pairingFailed("No pairing manager available");
        return;
    }

    emit pairingProgress("Generating OTP authentication...");

    // The actual OTP pairing logic would need to be integrated with the existing
    // NvPairingManager. The Android implementation modifies the standard pairing
    // request by adding the &otpauth= parameter.
    
    // For now, we'll use the standard pairing manager and modify its behavior
    // This would require extending NvPairingManager to support OTP parameters
    
    // TODO: Implement the actual OTP pairing protocol
    // This requires:
    // 1. Getting the salt from the initial pairing request
    // 2. Generating the OTP hash using generateOTPHash()
    // 3. Adding &otpauth=<hash> to the pairing request
    // 4. Sending the modified request to the server
    
    emit pairingProgress("Connecting to server...");
    
    // For now, emit an error since we need to implement the actual protocol
    emit pairingFailed("OTP pairing protocol not yet implemented. Please use standard PIN pairing.");
    
    qDebug() << "OTPPairingManager: OTP pairing protocol needs to be implemented";
}

bool OTPPairingManager::validatePinFormat(const QString &pin)
{
    // PIN must be exactly 4 digits (matches Android implementation)
    if (pin.length() != OTP_PIN_LENGTH) {
        return false;
    }

    // Check that all characters are digits
    for (const QChar &c : pin) {
        if (!c.isDigit()) {
            return false;
        }
    }

    return true;
}