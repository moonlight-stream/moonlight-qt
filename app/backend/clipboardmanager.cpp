#include "clipboardmanager.h"
#include "nvcomputer.h"
#include "nvhttp.h"
#include <QApplication>
#include <QMimeData>
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QCryptographicHash>

// Matches Android CLIPBOARD_IDENTIFIER constant
const QString ClipboardManager::CLIPBOARD_IDENTIFIER = "artemis_qt_clipboard_sync";

ClipboardManager::ClipboardManager(QObject *parent)
    : QObject(parent)
    , m_clipboard(QApplication::clipboard())
    , m_computer(nullptr)
    , m_http(nullptr)
    , m_smartSyncEnabled(false)
    , m_bidirectionalSync(true)
    , m_showToast(true)
    , m_hideContent(false)
    , m_maxClipboardSize(DEFAULT_MAX_SIZE)
    , m_syncInProgress(false)
{
    // Connect to clipboard changes (matches Android behavior)
    connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardManager::onClipboardChanged);
}

ClipboardManager::~ClipboardManager()
{
    disconnect();
}

void ClipboardManager::setConnection(NvComputer *computer, NvHTTP *http)
{
    m_computer = computer;
    m_http = http;
    
    qDebug() << "ClipboardManager: Connected to" << (computer ? computer->name : "null");
}

void ClipboardManager::disconnect()
{
    m_computer = nullptr;
    m_http = nullptr;
    m_syncInProgress = false;
    
    qDebug() << "ClipboardManager: Disconnected";
}

bool ClipboardManager::sendClipboard(bool force)
{
    if (!m_http || !m_computer) {
        qWarning() << "ClipboardManager: No connection available for clipboard sync";
        return false;
    }

    if (m_syncInProgress) {
        qDebug() << "ClipboardManager: Sync already in progress, skipping";
        return false;
    }

    QString clipboardText = getClipboardContent(force);
    if (clipboardText.isNull()) {
        qDebug() << "ClipboardManager: No clipboard content to send";
        return false;
    }

    return sendClipboardToServer(clipboardText);
}

bool ClipboardManager::getClipboard()
{
    if (!m_http || !m_computer) {
        qWarning() << "ClipboardManager: No connection available for clipboard sync";
        return false;
    }

    if (m_syncInProgress) {
        qDebug() << "ClipboardManager: Sync already in progress, skipping";
        return false;
    }

    QString clipboardContent = getClipboardFromServer();
    if (!clipboardContent.isNull()) {
        setClipboardContent(clipboardContent);
        return true;
    }

    return false;
}

void ClipboardManager::enableSmartSync(bool enabled)
{
    m_smartSyncEnabled = enabled;
    qDebug() << "ClipboardManager: Smart sync" << (enabled ? "enabled" : "disabled");
}

bool ClipboardManager::isSmartSyncEnabled() const
{
    return m_smartSyncEnabled;
}

void ClipboardManager::onStreamStarted()
{
    if (m_smartSyncEnabled) {
        qDebug() << "ClipboardManager: Stream started, uploading clipboard";
        sendClipboard(false);
    }
}

void ClipboardManager::onStreamResumed()
{
    if (m_smartSyncEnabled) {
        qDebug() << "ClipboardManager: Stream resumed, uploading clipboard";
        sendClipboard(false);
    }
}

void ClipboardManager::onFocusLost()
{
    if (m_smartSyncEnabled && m_bidirectionalSync) {
        qDebug() << "ClipboardManager: Focus lost, downloading clipboard";
        getClipboard();
    }
}

void ClipboardManager::setMaxClipboardSize(int maxSize)
{
    m_maxClipboardSize = maxSize;
}

int ClipboardManager::getMaxClipboardSize() const
{
    return m_maxClipboardSize;
}

void ClipboardManager::setBidirectionalSync(bool enabled)
{
    m_bidirectionalSync = enabled;
}

bool ClipboardManager::isBidirectionalSyncEnabled() const
{
    return m_bidirectionalSync;
}

void ClipboardManager::setShowToast(bool enabled)
{
    m_showToast = enabled;
}

bool ClipboardManager::shouldShowToast() const
{
    return m_showToast;
}

void ClipboardManager::setHideContent(bool enabled)
{
    m_hideContent = enabled;
}

bool ClipboardManager::shouldHideContent() const
{
    return m_hideContent;
}

void ClipboardManager::onClipboardChanged()
{
    if (m_syncInProgress) {
        return; // Ignore changes during sync
    }

    QString content = getClipboardContent(false);
    if (!content.isNull() && !isOwnClipboardChange(content)) {
        emit clipboardContentChanged();
        
        // Auto-upload if smart sync is enabled
        if (m_smartSyncEnabled) {
            sendClipboard(false);
        }
    }
}

QString ClipboardManager::getClipboardContent(bool force)
{
    if (!m_clipboard->mimeData()) {
        return QString();
    }

    const QMimeData *mimeData = m_clipboard->mimeData();
    if (!mimeData->hasText()) {
        return QString();
    }

    QString text = mimeData->text();
    
    // Check size limit
    if (text.size() > m_maxClipboardSize) {
        qWarning() << "ClipboardManager: Clipboard content too large:" << text.size() << "bytes";
        return QString();
    }

    // Check if this is our own content (loop prevention)
    if (!force && isOwnClipboardChange(text)) {
        qDebug() << "ClipboardManager: Ignoring own clipboard change";
        return QString();
    }

    return text;
}

void ClipboardManager::setClipboardContent(const QString &content)
{
    if (content.isEmpty()) {
        return;
    }

    m_syncInProgress = true;
    
    // Mark as our own content to prevent loops
    markAsOwnContent(content);
    
    // Set clipboard content
    QMimeData *mimeData = new QMimeData();
    mimeData->setText(content);
    
    if (m_hideContent) {
        // Mark as sensitive (matches Android behavior)
        mimeData->setData("application/x-qt-windows-mime;value=\"Clipboard Viewer Format\"", QByteArray());
    }
    
    m_clipboard->setMimeData(mimeData);
    m_lastReceivedContent = content;
    
    m_syncInProgress = false;
    
    qDebug() << "ClipboardManager: Set clipboard content (" << content.size() << " chars)";
}

bool ClipboardManager::isOwnClipboardChange(const QString &content)
{
    QString hash = generateContentHash(content);
    return m_ownContentHashes.contains(hash) || content == m_lastReceivedContent;
}

void ClipboardManager::markAsOwnContent(const QString &content)
{
    QString hash = generateContentHash(content);
    m_ownContentHashes.append(hash);
    
    // Keep only last 10 hashes to prevent memory growth
    if (m_ownContentHashes.size() > 10) {
        m_ownContentHashes.removeFirst();
    }
}

QString ClipboardManager::generateContentHash(const QString &content)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(content.toUtf8());
    return hash.result().toHex();
}

bool ClipboardManager::sendClipboardToServer(const QString &content)
{
    if (!m_http || content.isEmpty()) {
        return false;
    }

    emit clipboardSyncStarted();
    m_syncInProgress = true;

    // Matches Android NvHTTP.sendClipboard() implementation
    QNetworkRequest request;
    request.setUrl(QUrl(QString("https://%1:%2/actions/clipboard?type=text")
                       .arg(m_computer->activeAddress)
                       .arg(m_computer->httpsPort)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
    
    // Add authentication headers if needed
    if (m_http) {
        // TODO: Add proper authentication headers from NvHTTP
    }

    QNetworkReply *reply = m_http->sendClipboardContent(content);
    if (!reply) {
        m_syncInProgress = false;
        emit clipboardSyncFailed("Failed to create network request");
        return false;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, content]() {
        m_syncInProgress = false;
        
        if (reply->error() == QNetworkReply::NoError) {
            // Matches Android: empty response means success
            QString response = reply->readAll();
            bool success = response.isEmpty();
            
            if (success) {
                m_lastSentContent = content;
                markAsOwnContent(content);
                
                emit clipboardSyncCompleted(true, "Clipboard sent successfully");
                if (m_showToast) {
                    emit showToast("Clipboard sent successfully");
                }
            } else {
                emit clipboardSyncFailed("Server returned error");
                if (m_showToast) {
                    emit showToast("Failed to send clipboard");
                }
            }
        } else {
            QString error = QString("Network error: %1").arg(reply->errorString());
            emit clipboardSyncFailed(error);
            if (m_showToast) {
                emit showToast("Failed to send clipboard: " + reply->errorString());
            }
        }
        
        reply->deleteLater();
    });

    return true;
}

QString ClipboardManager::getClipboardFromServer()
{
    if (!m_http) {
        return QString();
    }

    emit clipboardSyncStarted();
    m_syncInProgress = true;

    // Matches Android NvHTTP.getClipboard() implementation
    QNetworkRequest request;
    request.setUrl(QUrl(QString("https://%1:%2/actions/clipboard?type=text")
                       .arg(m_computer->activeAddress)
                       .arg(m_computer->httpsPort)));
    
    // Add authentication headers if needed
    if (m_http) {
        // TODO: Add proper authentication headers from NvHTTP
    }

    QNetworkReply *reply = m_http->getClipboardContent();
    if (!reply) {
        m_syncInProgress = false;
        emit clipboardSyncFailed("Failed to create network request");
        return QString();
    }

    QString result;
    connect(reply, &QNetworkReply::finished, this, [this, reply, &result]() {
        m_syncInProgress = false;
        
        if (reply->error() == QNetworkReply::NoError) {
            result = reply->readAll();
            
            if (!result.isEmpty()) {
                emit clipboardSyncCompleted(true, "Clipboard received successfully");
                if (m_showToast) {
                    emit showToast("Clipboard received successfully");
                }
            }
        } else {
            QString error = QString("Network error: %1").arg(reply->errorString());
            emit clipboardSyncFailed(error);
            if (m_showToast) {
                emit showToast("Failed to get clipboard: " + reply->errorString());
            }
        }
        
        reply->deleteLater();
    });

    // Wait for the request to complete (synchronous for now)
    // TODO: Make this properly asynchronous
    
    return result;
}