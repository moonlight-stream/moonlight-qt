#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QCryptographicHash>

class NvComputer;
class NvHTTP;
class NvPairingManager;

/**
 * @brief Manages OTP (One-Time Password) pairing with Apollo servers
 * 
 * This class implements OTP pairing functionality as used in Artemis Android.
 * It extends the standard PIN pairing with SHA-256 hash authentication.
 * Based on PairingManager.java implementation.
 */
class OTPPairingManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit OTPPairingManager(QObject *parent = nullptr);
    ~OTPPairingManager();

    // OTP pairing operations (matches Android API)
    Q_INVOKABLE void startOTPPairing(NvComputer *computer, const QString &pin, const QString &passphrase = QString());
    Q_INVOKABLE void cancelPairing();
    
    // State queries
    Q_INVOKABLE bool isPairingInProgress() const;
    Q_INVOKABLE bool isOTPSupported(NvComputer *computer) const;

    // Settings
    Q_INVOKABLE void setPairingManager(NvPairingManager *pairingManager);

signals:
    void pairingStarted();
    void pairingCompleted(bool success, const QString &message);
    void pairingFailed(const QString &error);
    void pairingProgress(const QString &status);

private slots:
    void onStandardPairingCompleted(bool success, const QString &message);
    void onStandardPairingFailed(const QString &error);

private:
    // OTP hash generation (matches Android implementation)
    QString generateOTPHash(const QString &pin, const QString &salt, const QString &passphrase);
    
    // Pairing flow helpers
    void performOTPPairing(NvComputer *computer, const QString &pin, const QString &passphrase);
    bool validatePinFormat(const QString &pin);

private:
    static constexpr int OTP_PIN_LENGTH = 4; // Matches Android constant
    
    NvPairingManager *m_pairingManager;
    NvComputer *m_currentComputer;
    
    bool m_pairingInProgress;
    QString m_currentPin;
    QString m_currentPassphrase;
};