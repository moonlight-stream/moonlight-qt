#include "autoupdatechecker.h"

#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

AutoUpdateChecker::AutoUpdateChecker(QObject *parent) :
    QObject(parent)
{
    m_Nam = new QNetworkAccessManager(this);

    // Never communicate over HTTP
    m_Nam->setStrictTransportSecurityEnabled(true);

    // Allow HTTP redirects
    m_Nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(m_Nam, &QNetworkAccessManager::finished,
            this, &AutoUpdateChecker::handleUpdateCheckRequestFinished);

    QString currentVersion(VERSION_STR);
    qDebug() << "Current Vibelight version:" << currentVersion;
    parseStringToVersionQuad(currentVersion, m_CurrentVersionQuad);

    // Should at least have a 1.0-style version number
    Q_ASSERT(m_CurrentVersionQuad.count() > 1);
}

void AutoUpdateChecker::start()
{
    if (!m_Nam) {
        Q_ASSERT(m_Nam);
        return;
    }

#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN) || defined(STEAM_LINK) || defined(APP_IMAGE) // Only run update checker on platforms without auto-update
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(5, 15, 1) && !defined(QT_NO_BEARERMANAGEMENT)
    // HACK: Set network accessibility to work around QTBUG-80947 (introduced in Qt 5.14.0 and fixed in Qt 5.15.1)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    m_Nam->setNetworkAccessible(QNetworkAccessManager::Accessible);
    QT_WARNING_POP
#endif

    // We'll get a callback when this is finished. Vibelight checks the latest
    // published release of its own GitHub repository instead of upstream Moonlight.
    QUrl url("https://api.github.com/repos/nyteghost/moonlight-qt/releases/latest");
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Vibelight-UpdateChecker"); // Required by the GitHub API
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#else
    request.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, true);
#endif
    m_Nam->get(request);
#endif
}

void AutoUpdateChecker::parseStringToVersionQuad(QString& string, QVector<int>& version)
{
    // Accept release tags such as "v6.2.1" or "6.2.1-vibelight.1" by dropping a leading
    // "v" and ignoring anything after the numeric part.
    QString numeric = string.trimmed();
    if (numeric.startsWith('v') || numeric.startsWith('V')) {
        numeric.remove(0, 1);
    }
    int end = 0;
    while (end < numeric.size() && (numeric[end].isDigit() || numeric[end] == '.')) {
        end++;
    }
    numeric.truncate(end);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList list = numeric.split('.', Qt::SkipEmptyParts);
#else
    QStringList list = numeric.split('.', QString::SkipEmptyParts);
#endif
    for (const QString& component : std::as_const(list)) {
        version.append(component.toInt());
    }
}

QString AutoUpdateChecker::getPlatform()
{
#if defined(STEAM_LINK)
    return QStringLiteral("steamlink");
#elif defined(APP_IMAGE)
    return QStringLiteral("appimage");
#elif defined(Q_OS_DARWIN) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt 6 changed this from 'osx' to 'macos'. Use the old one
    // to be consistent (and not require another entry in the manifest).
    return QStringLiteral("osx");
#else
    return QSysInfo::productType();
#endif
}

int AutoUpdateChecker::compareVersion(QVector<int>& version1, QVector<int>& version2) {
    for (int i = 0;; i++) {
        int v1Val = 0;
        int v2Val = 0;

        // Treat missing decimal places as 0
        if (i < version1.count()) {
            v1Val = version1[i];
        }
        if (i < version2.count()) {
            v2Val = version2[i];
        }
        if (i >= version1.count() && i >= version2.count()) {
            // Equal versions
            return 0;
        }

        if (v1Val < v2Val) {
            return -1;
        }
        else if (v1Val > v2Val) {
            return 1;
        }
    }
}

void AutoUpdateChecker::handleUpdateCheckRequestFinished(QNetworkReply* reply)
{
    Q_ASSERT(reply->isFinished());

    // Delete the QNetworkAccessManager to free resources and
    // prevent the bearer plugin from polling in the background.
    m_Nam->deleteLater();
    m_Nam = nullptr;

    if (reply->error() == QNetworkReply::NoError) {
        QTextStream stream(reply);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);
#else
        stream.setCodec("UTF-8");
#endif

        // Read all data and queue the reply for deletion
        QString jsonString = stream.readAll();
        reply->deleteLater();

        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) {
            qWarning() << "GitHub release response malformed:" << error.errorString();
            return;
        }

        QJsonObject release = jsonDoc.object();
        if (!release["tag_name"].isString() || !release["html_url"].isString()) {
            qWarning() << "GitHub release response missing tag_name or html_url";
            return;
        }

        QString latestVersion = release["tag_name"].toString();
        qDebug() << "Latest Vibelight release is:" << latestVersion;

        QVector<int> latestVersionQuad;
        parseStringToVersionQuad(latestVersion, latestVersionQuad);
        if (latestVersionQuad.isEmpty()) {
            qWarning() << "Unable to parse release tag as a version:" << latestVersion;
            return;
        }

        int res = compareVersion(m_CurrentVersionQuad, latestVersionQuad);
        if (res < 0) {
            // m_CurrentVersionQuad < latestVersionQuad
            qDebug() << "Update available";
            QString displayVersion = latestVersion;
            if (displayVersion.startsWith('v') || displayVersion.startsWith('V')) {
                displayVersion.remove(0, 1);
            }
            emit onUpdateAvailable(displayVersion, release["html_url"].toString());
        }
        else if (res > 0) {
            qDebug() << "Latest release is older than the current version";
        }
        else {
            qDebug() << "Latest release matches the current version";
        }
    }
    else if (reply->error() == QNetworkReply::ContentNotFoundError) {
        // The repository has no published releases yet
        qDebug() << "No Vibelight releases published yet";
        reply->deleteLater();
    }
    else {
        qWarning() << "Update checking failed with error:" << reply->error();
        reply->deleteLater();
    }
}
