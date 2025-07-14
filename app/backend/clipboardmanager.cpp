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
    
    // Check and emit Apollo support status
    bool supported = isClipboardSyncSupported();
    emit apolloSupportChanged(supported);
    
    if (supported) {
        qDebug() << "ClipboardManager: Apollo server detected - clipboard sync available";
    } else {
        qDebug() << "ClipboardManager: Non-Apollo server - clipboard sync not available";
    }
}

void ClipboardManager::disconnect()
{
    m_computer = nullptr;
    m_http = nullptr;
    m_syncInProgress = false;
    
    emit apolloSupportChanged(false);
    
    qDebug() << "ClipboardManager: Disconnected";
}

bool ClipboardManager::sendClipboard(bool force)
{
    if (!m_http || !m_computer) {
        qWarning() << "ClipboardManager: No connection available for clipboard sync";
        return false;
    }

    if (!isClipboardSyncSupported()) {
        qWarning() << "ClipboardManager: Clipboard sync not supported (not an Apollo server)";
        emit clipboardSyncFailed("Clipboard sync only works with Apollo/Sunshine servers");
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

    if (!isClipboardSyncSupported()) {
        qWarning() << "ClipboardManager: Clipboard sync not supported (not an Apollo server)";
        emit clipboardSyncFailed("Clipboard sync only works with Apollo/Sunshine servers");
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

bool ClipboardManager::isClipboardSyncSupported() const
{
    if (!m_computer || !m_http) {
        return false;
    }

    // Clipboard sync only works with Apollo servers (not GeForce Experience)
    // Apollo servers can be detected by checking for specific server info fields
    try {
        QString serverInfo = m_http->getServerInfo(NvHTTP::NVLL_ERROR, true);
        
        // Apollo servers typically have different server info structure
        // Check for Apollo-specific fields or version patterns
        QString serverVersion = m_http->getXmlString(serverInfo, "appversion");
        QString serverName = m_http->getXmlString(serverInfo, "hostname");
        
        // Apollo/Sunshine servers typically don't have GFE version info
        QString gfeVersion = m_http->getXmlString(serverInfo, "GfeVersion");
        
        // If no GFE version but has app version, likely Apollo/Sunshine
        bool isApollo = gfeVersion.isEmpty() && !serverVersion.isEmpty();
        
        qDebug() << "ClipboardManager: Apollo detection - GFE:" << gfeVersion 
                 << "AppVer:" << serverVersion << "IsApollo:" << isApollo;
        
        return isApollo;
    }
    catch (const std::exception& e) {
        qWarning() << "ClipboardManager: Failed to detect Apollo server:" << e.what();
        return false;
    }
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

    // Use the new NvHTTP clipboard method
    bool success = m_http->sendClipboardContent(content);
    
    m_syncInProgress = false;
    
    if (success) {
        markAsOwnContent(content);
        emit clipboardSyncCompleted();
        
        if (m_showToast) {
            emit showToast("Clipboard uploaded to server");
        }
        
        qDebug() << "ClipboardManager: Successfully sent clipboard to server";
        return true;
    } else {
        emit clipboardSyncFailed("Failed to send clipboard to server");
        qWarning() << "ClipboardManager: Failed to send clipboard to server";
        return false;
    }
}

QString ClipboardManager::getClipboardFromServer()
{
    if (!m_http) {
        return QString();
    }

    emit clipboardSyncStarted();
    m_syncInProgress = true;

    // Use the new NvHTTP clipboard method
    QString content = m_http->getClipboardContent();
    
    m_syncInProgress = false;
    
    if (!content.isEmpty()) {
        emit clipboardSyncCompleted();
        
        if (m_showToast) {
            emit showToast("Clipboard downloaded from server");
        }
        
        qDebug() << "ClipboardManager: Successfully received clipboard from server";
        return content;
    } else {
        emit clipboardSyncFailed("Failed to get clipboard from server");
        qWarning() << "ClipboardManager: Failed to get clipboard from server";
        return QString();
    }
}