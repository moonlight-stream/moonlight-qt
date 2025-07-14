#pragma once

#include <QObject>
#include <QClipboard>
#include <QTimer>
#include <QQmlEngine>
#include <QCryptographicHash>

class NvComputer;
class NvHTTP;

/**
 * @brief Manages clipboard synchronization between client and server
 * 
 * This class handles bidirectional clipboard sync with Apollo/Sunshine servers
 * using the /actions/clipboard HTTP endpoint. Based on Artemis Android implementation.
 */
class ClipboardManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit ClipboardManager(QObject *parent = nullptr);
    ~ClipboardManager();

    // Connection management
    Q_INVOKABLE void setConnection(NvComputer *computer, NvHTTP *http);
    Q_INVOKABLE void disconnect();

    // Manual sync operations (matches Android API)
    Q_INVOKABLE bool sendClipboard(bool force = false);
    Q_INVOKABLE bool getClipboard();

    // Smart sync control (matches Android settings)
    Q_INVOKABLE void enableSmartSync(bool enabled);
    Q_INVOKABLE bool isSmartSyncEnabled() const;

    // Auto-sync triggers (matches Android behavior)
    Q_INVOKABLE void onStreamStarted();
    Q_INVOKABLE void onStreamResumed();
    Q_INVOKABLE void onFocusLost();

    // Settings (matches Android preferences)
    Q_INVOKABLE void setMaxClipboardSize(int maxSize);
    Q_INVOKABLE int getMaxClipboardSize() const;

    Q_INVOKABLE void setBidirectionalSync(bool enabled);
    Q_INVOKABLE bool isBidirectionalSyncEnabled() const;

    Q_INVOKABLE void setShowToast(bool enabled);
    Q_INVOKABLE bool shouldShowToast() const;

    Q_INVOKABLE void setHideContent(bool enabled);
    Q_INVOKABLE bool shouldHideContent() const;

signals:
    void clipboardSyncStarted();
    void clipboardSyncCompleted(bool success, const QString &message);
    void clipboardSyncFailed(const QString &error);
    void clipboardContentChanged();
    void showToast(const QString &message);

private slots:
    void onClipboardChanged();

private:
    // Core clipboard operations (matches Android implementation)
    QString getClipboardContent(bool force = false);
    void setClipboardContent(const QString &content);
    
    // Loop prevention (matches Android CLIPBOARD_IDENTIFIER logic)
    bool isOwnClipboardChange(const QString &content);
    void markAsOwnContent(const QString &content);
    QString generateContentHash(const QString &content);
    
    // HTTP operations (matches Android NvHTTP methods)
    bool sendClipboardToServer(const QString &content);
    QString getClipboardFromServer();

private:
    static constexpr int DEFAULT_MAX_SIZE = 1048576; // 1MB (matches Android)
    static const QString CLIPBOARD_IDENTIFIER; // Matches Android constant

    QClipboard *m_clipboard;
    
    NvComputer *m_computer;
    NvHTTP *m_http;
    
    // Settings (matches Android preferences)
    bool m_smartSyncEnabled;
    bool m_bidirectionalSync;
    bool m_showToast;
    bool m_hideContent;
    int m_maxClipboardSize;
    
    // State tracking (matches Android implementation)
    QString m_lastSentContent;
    QString m_lastReceivedContent;
    QStringList m_ownContentHashes;
    bool m_syncInProgress;
};