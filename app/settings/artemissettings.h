#pragma once

#include <QObject>
#include <QSettings>
#include <QQmlEngine>

/**
 * @brief Manages Artemis-specific settings and preferences
 * 
 * This class extends the base streaming preferences with Artemis-specific
 * features like clipboard sync, server commands, and OTP pairing.
 */
class ArtemisSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Clipboard sync settings
    Q_PROPERTY(bool clipboardSyncEnabled READ clipboardSyncEnabled WRITE setClipboardSyncEnabled NOTIFY clipboardSyncEnabledChanged)
    Q_PROPERTY(bool clipboardSyncBidirectional READ clipboardSyncBidirectional WRITE setClipboardSyncBidirectional NOTIFY clipboardSyncBidirectionalChanged)
    Q_PROPERTY(int clipboardSyncMaxSize READ clipboardSyncMaxSize WRITE setClipboardSyncMaxSize NOTIFY clipboardSyncMaxSizeChanged)

    // Server commands settings
    Q_PROPERTY(bool serverCommandsEnabled READ serverCommandsEnabled WRITE setServerCommandsEnabled NOTIFY serverCommandsEnabledChanged)
    Q_PROPERTY(bool showAdvancedCommands READ showAdvancedCommands WRITE setShowAdvancedCommands NOTIFY showAdvancedCommandsChanged)

    // OTP pairing settings
    Q_PROPERTY(bool otpPairingEnabled READ otpPairingEnabled WRITE setOtpPairingEnabled NOTIFY otpPairingEnabledChanged)
    Q_PROPERTY(int otpPairingTimeout READ otpPairingTimeout WRITE setOtpPairingTimeout NOTIFY otpPairingTimeoutChanged)

    // Client-side display settings
    Q_PROPERTY(bool fractionalRefreshRateEnabled READ fractionalRefreshRateEnabled WRITE setFractionalRefreshRateEnabled NOTIFY fractionalRefreshRateEnabledChanged)
    Q_PROPERTY(double customRefreshRate READ customRefreshRate WRITE setCustomRefreshRate NOTIFY customRefreshRateChanged)
    Q_PROPERTY(bool resolutionScalingEnabled READ resolutionScalingEnabled WRITE setResolutionScalingEnabled NOTIFY resolutionScalingEnabledChanged)
    Q_PROPERTY(double resolutionScaleFactor READ resolutionScaleFactor WRITE setResolutionScaleFactor NOTIFY resolutionScaleFactorChanged)
    Q_PROPERTY(bool virtualDisplayEnabled READ virtualDisplayEnabled WRITE setVirtualDisplayEnabled NOTIFY virtualDisplayEnabledChanged)

    // App ordering settings
    Q_PROPERTY(bool customAppOrderingEnabled READ customAppOrderingEnabled WRITE setCustomAppOrderingEnabled NOTIFY customAppOrderingEnabledChanged)
    Q_PROPERTY(QString appOrderingMode READ appOrderingMode WRITE setAppOrderingMode NOTIFY appOrderingModeChanged)

    // Permission viewing settings
    Q_PROPERTY(bool showServerPermissions READ showServerPermissions WRITE setShowServerPermissions NOTIFY showServerPermissionsChanged)

    // Input-only mode settings
    Q_PROPERTY(bool inputOnlyModeEnabled READ inputOnlyModeEnabled WRITE setInputOnlyModeEnabled NOTIFY inputOnlyModeEnabledChanged)

public:
    static ArtemisSettings* instance();
    static ArtemisSettings* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    Q_INVOKABLE void save();
    Q_INVOKABLE void load();
    Q_INVOKABLE void resetToDefaults();

    // Clipboard sync getters/setters
    bool clipboardSyncEnabled() const { return m_clipboardSyncEnabled; }
    void setClipboardSyncEnabled(bool enabled);

    bool clipboardSyncBidirectional() const { return m_clipboardSyncBidirectional; }
    void setClipboardSyncBidirectional(bool bidirectional);

    int clipboardSyncMaxSize() const { return m_clipboardSyncMaxSize; }
    void setClipboardSyncMaxSize(int maxSize);

    // Server commands getters/setters
    bool serverCommandsEnabled() const { return m_serverCommandsEnabled; }
    void setServerCommandsEnabled(bool enabled);

    bool showAdvancedCommands() const { return m_showAdvancedCommands; }
    void setShowAdvancedCommands(bool show);

    // OTP pairing getters/setters
    bool otpPairingEnabled() const { return m_otpPairingEnabled; }
    void setOtpPairingEnabled(bool enabled);

    int otpPairingTimeout() const { return m_otpPairingTimeout; }
    void setOtpPairingTimeout(int timeout);

    // Client-side display getters/setters
    bool fractionalRefreshRateEnabled() const { return m_fractionalRefreshRateEnabled; }
    void setFractionalRefreshRateEnabled(bool enabled);

    double customRefreshRate() const { return m_customRefreshRate; }
    void setCustomRefreshRate(double rate);

    bool resolutionScalingEnabled() const { return m_resolutionScalingEnabled; }
    void setResolutionScalingEnabled(bool enabled);

    double resolutionScaleFactor() const { return m_resolutionScaleFactor; }
    void setResolutionScaleFactor(double factor);

    bool virtualDisplayEnabled() const { return m_virtualDisplayEnabled; }
    void setVirtualDisplayEnabled(bool enabled);

    // App ordering getters/setters
    bool customAppOrderingEnabled() const { return m_customAppOrderingEnabled; }
    void setCustomAppOrderingEnabled(bool enabled);

    QString appOrderingMode() const { return m_appOrderingMode; }
    void setAppOrderingMode(const QString &mode);

    // Permission viewing getters/setters
    bool showServerPermissions() const { return m_showServerPermissions; }
    void setShowServerPermissions(bool show);

    // Input-only mode getters/setters
    bool inputOnlyModeEnabled() const { return m_inputOnlyModeEnabled; }
    void setInputOnlyModeEnabled(bool enabled);

signals:
    // Clipboard sync signals
    void clipboardSyncEnabledChanged();
    void clipboardSyncBidirectionalChanged();
    void clipboardSyncMaxSizeChanged();

    // Server commands signals
    void serverCommandsEnabledChanged();
    void showAdvancedCommandsChanged();

    // OTP pairing signals
    void otpPairingEnabledChanged();
    void otpPairingTimeoutChanged();

    // Client-side display signals
    void fractionalRefreshRateEnabledChanged();
    void customRefreshRateChanged();
    void resolutionScalingEnabledChanged();
    void resolutionScaleFactorChanged();
    void virtualDisplayEnabledChanged();

    // App ordering signals
    void customAppOrderingEnabledChanged();
    void appOrderingModeChanged();

    // Permission viewing signals
    void showServerPermissionsChanged();

    // Input-only mode signals
    void inputOnlyModeEnabledChanged();

private:
    explicit ArtemisSettings(QObject *parent = nullptr);

    void loadDefaults();

private:
    static ArtemisSettings* s_instance;
    QSettings *m_settings;

    // Clipboard sync settings
    bool m_clipboardSyncEnabled;
    bool m_clipboardSyncBidirectional;
    int m_clipboardSyncMaxSize;

    // Server commands settings
    bool m_serverCommandsEnabled;
    bool m_showAdvancedCommands;

    // OTP pairing settings
    bool m_otpPairingEnabled;
    int m_otpPairingTimeout;

    // Client-side display settings
    bool m_fractionalRefreshRateEnabled;
    double m_customRefreshRate;
    bool m_resolutionScalingEnabled;
    double m_resolutionScaleFactor;
    bool m_virtualDisplayEnabled;

    // App ordering settings
    bool m_customAppOrderingEnabled;
    QString m_appOrderingMode;

    // Permission viewing settings
    bool m_showServerPermissions;

    // Input-only mode settings
    bool m_inputOnlyModeEnabled;
};