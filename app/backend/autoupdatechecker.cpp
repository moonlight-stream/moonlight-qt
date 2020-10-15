#include "autoupdatechecker.h"

#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

AutoUpdateChecker::AutoUpdateChecker(QObject *parent) :
    QObject(parent),
    m_Nam(this)
{
    // Never communicate over HTTP
    m_Nam.setStrictTransportSecurityEnabled(true);

    // Allow HTTP redirects
    m_Nam.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(&m_Nam, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(handleUpdateCheckRequestFinished(QNetworkReply*)));

    QString currentVersion(VERSION_STR);
    qDebug() << "Current Moonlight version:" << currentVersion;
    parseStringToVersionQuad(currentVersion, m_CurrentVersionQuad);

    // Should at least have a 1.0-style version number
    Q_ASSERT(m_CurrentVersionQuad.count() > 1);
}

void AutoUpdateChecker::start()
{
#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN) || defined(STEAM_LINK) || defined(APP_IMAGE) // Only run update checker on platforms without auto-update
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(5, 15, 1) && !defined(QT_NO_BEARERMANAGEMENT)
    // HACK: Set network accessibility to work around QTBUG-80947 (introduced in Qt 5.14.0 and fixed in Qt 5.15.1)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    m_Nam.setNetworkAccessible(QNetworkAccessManager::Accessible);
    QT_WARNING_POP
#endif

    // We'll get a callback when this is finished
    QUrl url("https://moonlight-stream.org/updates/qt.json");
    m_Nam.get(QNetworkRequest(url));
#endif
}

void AutoUpdateChecker::parseStringToVersionQuad(QString& string, QVector<int>& version)
{
    QStringList list = string.split('.');
    for (const QString& component : list) {
        version.append(component.toInt());
    }
}

QString AutoUpdateChecker::getPlatform()
{
#if defined(STEAM_LINK)
    return QStringLiteral("steamlink");
#elif defined(APP_IMAGE)
    return QStringLiteral("appimage");
#else
    return QSysInfo::productType();
#endif
}

void AutoUpdateChecker::handleUpdateCheckRequestFinished(QNetworkReply* reply)
{
    Q_ASSERT(reply->isFinished());

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
            qWarning() << "Update manifest malformed:" << error.errorString();
            return;
        }

        QJsonArray array = jsonDoc.array();
        if (array.isEmpty()) {
            qWarning() << "Update manifest doesn't contain an array";
            return;
        }

        for (QJsonValueRef updateEntry : array) {
            if (updateEntry.isObject()) {
                QJsonObject updateObj = updateEntry.toObject();
                if (!updateObj.contains("platform") ||
                        !updateObj.contains("arch") ||
                        !updateObj.contains("version") ||
                        !updateObj.contains("browser_url")) {
                    qWarning() << "Update manifest entry missing vital field";
                    continue;
                }

                if (!updateObj["platform"].isString() ||
                        !updateObj["arch"].isString() ||
                        !updateObj["version"].isString() ||
                        !updateObj["browser_url"].isString()) {
                    qWarning() << "Update manifest entry has unexpected vital field type";
                    continue;
                }

                if (updateObj["arch"] == QSysInfo::buildCpuArchitecture() &&
                        updateObj["platform"] == getPlatform()) {
                    qDebug() << "Found update manifest match for current platform";

                    QString latestVersion = updateObj["version"].toString();
                    qDebug() << "Latest version of Moonlight for this platform is:" << latestVersion;

                    QVector<int> latestVersionQuad;

                    parseStringToVersionQuad(latestVersion, latestVersionQuad);

                    for (int i = 0;; i++) {
                        int latestVer = 0;
                        int currentVer = 0;

                        // Treat missing decimal places as 0
                        if (i < latestVersionQuad.count()) {
                            latestVer = latestVersionQuad[i];
                        }
                        if (i < m_CurrentVersionQuad.count()) {
                            currentVer = m_CurrentVersionQuad[i];
                        }
                        if (i >= latestVersionQuad.count() && i >= m_CurrentVersionQuad.count()) {
                            break;
                        }

                        if (currentVer < latestVer) {
                            qDebug() << "Update available";

                            emit onUpdateAvailable(updateObj["version"].toString(),
                                                   updateObj["browser_url"].toString());
                            return;
                        }
                        else if (currentVer > latestVer) {
                            qDebug() << "Update manifest version lower than current version";
                            return;
                        }
                    }

                    qDebug() << "Update manifest version equal to current version";

                    return;
                }
            }
            else {
                qWarning() << "Update manifest contained unrecognized entry:" << updateEntry.toString();
            }
        }

        qWarning() << "No entry in update manifest found for current platform: "
                   << QSysInfo::buildCpuArchitecture() << getPlatform();
    }
    else {
        qWarning() << "Update checking failed with error: " << reply->error();
        reply->deleteLater();
    }
}
