#include "autoupdatechecker.h"
#include "portableupdateinstaller.h"
#include "path.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSysInfo>
#include <QTextStream>

// GitHub repository for update checks
#define GITHUB_OWNER "qiin2333"
#define GITHUB_REPO  "moonlight-qt"

AutoUpdateChecker::AutoUpdateChecker(QObject *parent) :
    QObject(parent)
{
    m_Nam = new QNetworkAccessManager(this);
    m_PortableUpdateInstaller = new PortableUpdateInstaller(this);

    // Never communicate over HTTP
    m_Nam->setStrictTransportSecurityEnabled(true);

    // Allow HTTP redirects
    m_Nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(m_Nam, &QNetworkAccessManager::finished,
            this, &AutoUpdateChecker::handleUpdateCheckRequestFinished);

    QString currentVersion(VERSION_STR);
    qDebug() << "Current Moonlight version:" << currentVersion;
    parseStringToVersionQuad(currentVersion, m_CurrentVersionQuad);

    // Should at least have a 1.0-style version number
    Q_ASSERT(m_CurrentVersionQuad.count() > 1);

    connect(m_PortableUpdateInstaller, &PortableUpdateInstaller::onPortableUpdateStatusChanged,
            this, &AutoUpdateChecker::onPortableUpdateStatusChanged);
    connect(m_PortableUpdateInstaller, &PortableUpdateInstaller::onPortableUpdateFailed,
            this, &AutoUpdateChecker::onPortableUpdateFailed);
}

bool AutoUpdateChecker::supportsInAppUpdate() const
{
    return m_PortableUpdateInstaller->supportsInAppUpdate();
}

void AutoUpdateChecker::installUpdate(QString url)
{
    const QString expectedDigest = url == m_UpdateDownloadUrl ? m_UpdateAssetDigest : QString();
    m_PortableUpdateInstaller->installUpdate(url, expectedDigest);
}

void AutoUpdateChecker::start()
{
    if (!m_Nam) {
        Q_ASSERT(m_Nam);
        return;
    }

#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN) || defined(STEAM_LINK) || defined(APP_IMAGE)
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(5, 15, 1) && !defined(QT_NO_BEARERMANAGEMENT)
    // HACK: Set network accessibility to work around QTBUG-80947 (introduced in Qt 5.14.0 and fixed in Qt 5.15.1)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    m_Nam->setNetworkAccessible(QNetworkAccessManager::Accessible);
    QT_WARNING_POP
#endif

    // Query GitHub Releases API for the latest release
    QUrl url(QString("https://api.github.com/repos/%1/%2/releases/latest")
                 .arg(GITHUB_OWNER, GITHUB_REPO));
    QNetworkRequest request(url);

    // GitHub API requires a User-Agent header
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("Moonlight/%1").arg(VERSION_STR));
    // Request JSON response
    request.setRawHeader("Accept", "application/vnd.github+json");

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#else
    request.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, true);
#endif
    m_Nam->get(request);
#endif
}

void AutoUpdateChecker::parseStringToVersionQuad(const QString& string, QVector<int>& version)
{
    version.clear();

    // Strip leading 'v' and ignore SemVer suffixes/build metadata:
    //   v6.2.82                  -> 6.2.82
    //   6.2.82+14.g13ca12da.dirty -> 6.2.82
    //   v6.2.82-14-g13ca12da      -> 6.2.82
    QString versionStr = string.trimmed();
    if (versionStr.startsWith('v') || versionStr.startsWith('V')) {
        versionStr = versionStr.mid(1);
    }

    int suffixIndex = versionStr.indexOf('+');
    int prereleaseIndex = versionStr.indexOf('-');
    if (suffixIndex < 0 || (prereleaseIndex >= 0 && prereleaseIndex < suffixIndex)) {
        suffixIndex = prereleaseIndex;
    }
    if (suffixIndex >= 0) {
        versionStr = versionStr.left(suffixIndex);
    }

    QStringList list = versionStr.split('.');
    for (const QString& component : std::as_const(list)) {
        bool ok = false;
        int value = component.toInt(&ok);
        if (!ok) {
            break;
        }
        version.append(value);
    }
}

QString AutoUpdateChecker::getPreferredAssetSuffix() const
{
#if defined(Q_OS_DARWIN)
    // CI 出的 DMG 现在带架构后缀（Moonlight-<版本>-arm64.dmg）。
    // QSysInfo::buildCpuArchitecture() 给的是 arm64 / x86_64，和 generate-dmg.sh
    // 里的 MOONLIGHT_ARCH 用词一致。
    //
    // 只是「优先」而不是「必须」：这个后缀是从某个版本才开始有的，旧 release 里是
    // Moonlight-<版本>.dmg。匹配不到就退回任意 .dmg，否则老版本的用户会看到
    // 「找不到更新包」。
    return QStringLiteral("-") + QSysInfo::buildCpuArchitecture() + QStringLiteral(".dmg");
#else
    return QString();
#endif
}

QString AutoUpdateChecker::getExpectedAssetSuffix() const
{
#if defined(Q_OS_WIN32)
    return isPortableInstall() ? QStringLiteral(".zip") : QStringLiteral(".exe");
#elif defined(Q_OS_DARWIN)
    return QStringLiteral(".dmg");
#elif defined(APP_IMAGE)
    return QStringLiteral(".AppImage");
#else
    return QString();
#endif
}

bool AutoUpdateChecker::isPortableInstall() const
{
#if defined(Q_OS_WIN32)
    return QFile::exists(QDir(Path::getPortableRootDir()).filePath("portable.dat"));
#else
    return false;
#endif
}

QString AutoUpdateChecker::getExpectedAssetPrefix() const
{
#if defined(Q_OS_WIN32)
    if (isPortableInstall()) {
        return QStringLiteral("MoonlightPortable-%1-").arg(getCurrentBuildArch());
    }

    return QStringLiteral("MoonlightSetup-");
#else
    return QString();
#endif
}

QString AutoUpdateChecker::getCurrentBuildArch() const
{
    QString buildArch = QSysInfo::buildCpuArchitecture();

    if (buildArch == "x86_64") {
        return QStringLiteral("x64");
    }
    else if (buildArch == "i386") {
        return QStringLiteral("x86");
    }

    return buildArch.toLower();
}

int AutoUpdateChecker::compareVersion(const QVector<int>& version1, const QVector<int>& version2) {
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
        if (jsonDoc.isNull()) {
            qWarning() << "GitHub release response malformed:" << error.errorString();
            return;
        }

        if (!jsonDoc.isObject()) {
            qWarning() << "GitHub release response is not a JSON object";
            return;
        }

        QJsonObject releaseObj = jsonDoc.object();

        // GitHub Releases API response format:
        // {
        //   "tag_name": "v6.3.0",
        //   "name": "Release 6.3.0",
        //   "html_url": "https://github.com/owner/repo/releases/tag/v6.3.0",
        //   "prerelease": false,
        //   "draft": false,
        //   "assets": [
        //     {
        //       "name": "MoonlightSetup-x64-6.3.0.exe",
        //       "browser_download_url": "https://github.com/..."
        //     }
        //   ]
        // }

        // Skip pre-releases and drafts
        if (releaseObj["prerelease"].toBool(false) || releaseObj["draft"].toBool(false)) {
            qDebug() << "Latest GitHub release is a pre-release or draft, skipping";
            return;
        }

        if (!releaseObj.contains("tag_name") || !releaseObj["tag_name"].isString()) {
            qWarning() << "GitHub release missing tag_name";
            return;
        }

        QString tagName = releaseObj["tag_name"].toString();
        qDebug() << "Latest GitHub release tag:" << tagName;

        // Parse version from tag (strip 'v' prefix if present)
        QVector<int> latestVersionQuad;
        parseStringToVersionQuad(tagName, latestVersionQuad);

        int res = compareVersion(m_CurrentVersionQuad, latestVersionQuad);
        if (res < 0) {
            // Current version is older than latest release
            qDebug() << "Update available:" << tagName;

            // Try to find a platform-specific download URL from assets
            QString downloadUrl;
            QString assetDigest;
            QString expectedPrefix = getExpectedAssetPrefix();
            QString expectedSuffix = getExpectedAssetSuffix();

            QString preferredSuffix = getPreferredAssetSuffix();

            if (!expectedSuffix.isEmpty() && releaseObj.contains("assets") && releaseObj["assets"].isArray()) {
                QJsonArray assets = releaseObj["assets"].toArray();

                // 后备候选：后缀对得上但不带本机架构后缀的那个（旧 release 的命名）
                QString fallbackUrl;
                QString fallbackName;
                QString fallbackDigest;

                for (const auto& asset : std::as_const(assets)) {
                    if (asset.isObject()) {
                        QJsonObject assetObj = asset.toObject();
                        QString assetName = assetObj["name"].toString();
                        bool prefixMatches = expectedPrefix.isEmpty() ||
                                             assetName.startsWith(expectedPrefix, Qt::CaseInsensitive);
                        bool suffixMatches = assetName.endsWith(expectedSuffix, Qt::CaseInsensitive);

                        if (!prefixMatches || !suffixMatches) {
                            continue;
                        }

                        if (!preferredSuffix.isEmpty() &&
                                assetName.endsWith(preferredSuffix, Qt::CaseInsensitive)) {
                            downloadUrl = assetObj["browser_download_url"].toString();
                            assetDigest = assetObj["digest"].toString();
                            qDebug() << "Found matching asset for this architecture:" << assetName;
                            break;
                        }

                        // 后备只认「没带架构后缀」的旧命名。带了别的架构后缀的资产
                        // 绝对不能当后备 —— 只发了 arm64 包的 release 会把 arm64 的
                        // DMG 喂给 Intel 客户端。这种情况下宁可让 downloadUrl 留空，
                        // 退回打开 release 页面让用户自己看。
                        bool isOtherArchAsset =
                                assetName.endsWith(QStringLiteral("-arm64.dmg"), Qt::CaseInsensitive) ||
                                assetName.endsWith(QStringLiteral("-x86_64.dmg"), Qt::CaseInsensitive);

                        if (fallbackUrl.isEmpty() && !isOtherArchAsset) {
                            fallbackUrl = assetObj["browser_download_url"].toString();
                            fallbackName = assetName;
                            fallbackDigest = assetObj["digest"].toString();
                        }
                    }
                }

                if (downloadUrl.isEmpty() && !fallbackUrl.isEmpty()) {
                    downloadUrl = fallbackUrl;
                    assetDigest = fallbackDigest;
                    qDebug() << "Found matching asset:" << fallbackName;
                }
            }

            // Fall back to the release page URL if no matching asset found
            if (downloadUrl.isEmpty()) {
                downloadUrl = releaseObj["html_url"].toString();
            }

            m_UpdateDownloadUrl = downloadUrl;
            m_UpdateAssetDigest = assetDigest;

            emit onUpdateAvailable(tagName, downloadUrl);
        }
        else if (res > 0) {
            qDebug() << "Current version is newer than latest release";
        }
        else {
            qDebug() << "Current version matches latest release";
        }
    }
    else {
        qWarning() << "Update checking failed:" << reply->error() << reply->errorString();
        reply->deleteLater();
    }
}
