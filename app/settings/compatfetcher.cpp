#include "compatfetcher.h"
#include "path.h"

#include <QNetworkReply>
#include <QSettings>

#define COMPAT_VERSION "v1"
#define COMPAT_KEY "latestsupportedversion-"

CompatFetcher::CompatFetcher(QObject *parent) :
    QObject(parent),
    m_Nam(this)
{
    // Never communicate over HTTP
    m_Nam.setStrictTransportSecurityEnabled(true);

    // Allow HTTP redirects
    m_Nam.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(&m_Nam, &QNetworkAccessManager::finished,
            this, &CompatFetcher::handleCompatInfoFetched);
}

void CompatFetcher::start()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(5, 15, 1) && !defined(QT_NO_BEARERMANAGEMENT)
    // HACK: Set network accessibility to work around QTBUG-80947 (introduced in Qt 5.14.0 and fixed in Qt 5.15.1)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    m_Nam.setNetworkAccessible(QNetworkAccessManager::Accessible);
    QT_WARNING_POP
#endif

    QUrl url("https://moonlight-stream.org/compatibility/" COMPAT_VERSION);
    QNetworkRequest request(url);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#else
    request.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, true);
#endif

    // We'll get a callback when this is finished
    m_Nam.get(request);
}

bool CompatFetcher::isGfeVersionSupported(QString gfeVersion)
{
    QSettings settings;

    if (gfeVersion.isEmpty()) {
        // If we don't have a GFE version, just allow it
        return true;
    }

    QString latestSupportedVersion = settings.value(COMPAT_KEY COMPAT_VERSION).toString();
    if (latestSupportedVersion.isEmpty()) {
        // We don't have compat data yet, so just assume it's supported
        return true;
    }

    QStringList latestSupportedVersionQuad = latestSupportedVersion.split('.');
    QStringList gfeVersionQuad = gfeVersion.split('.');

    for (int i = 0;; i++) {
        int actualVerVal = 0;
        int latestSupportedVal = 0;

        // Treat missing decimal places as 0
        if (i < gfeVersionQuad.count()) {
            actualVerVal = gfeVersionQuad[i].toInt();
        }
        if (i < latestSupportedVersionQuad.count()) {
            latestSupportedVal = latestSupportedVersionQuad[i].toInt();
        }

        if (i >= gfeVersionQuad.count() && i >= latestSupportedVersionQuad.count()) {
            // Equal versions - this is fine
            return true;
        }

        if (actualVerVal < latestSupportedVal) {
            // Actual version is lower than latest supported - this is fine
            return true;
        }
        else if (actualVerVal > latestSupportedVal) {
            // Actual version is greater than latest supported - this is bad
            return false;
        }
    }
}

void CompatFetcher::handleCompatInfoFetched(QNetworkReply* reply)
{
    Q_ASSERT(reply->isFinished());

    if (reply->error() == QNetworkReply::NoError) {
        // Queue the reply for deletion
        reply->deleteLater();

        QString version = QString(reply->readAll()).trimmed();

        QSettings settings;
        settings.setValue(COMPAT_KEY COMPAT_VERSION, version);

        qInfo() << "Latest supported GFE server:" << version;
    }
    else {
        qWarning() << "Failed to download latest compatibility data:" << reply->error();
        reply->deleteLater();
    }
}
