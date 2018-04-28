#include "nvhttp.h"

#include <QUuid>
#include <QtNetwork/QNetworkReply>

NvHTTP::NvHTTP(QString address)
{
    m_BaseUrlHttp.setScheme("http");
    m_BaseUrlHttps.setScheme("https");
    m_BaseUrlHttp.setHost(address);
    m_BaseUrlHttps.setHost(address);
    m_BaseUrlHttp.setPort(47989);
    m_BaseUrlHttps.setPort(47984);
}

QNetworkReply*
NvHTTP::openConnection(QUrl baseUrl,
                       QString command,
                       QString arguments,
                       bool enableTimeout)
{
    QUrl url(baseUrl);

    url.setPath(command +
                "?uniqueid=" + "0" +
                "&uuid=" + QUuid::createUuid().toString() +
                ((arguments != nullptr) ? (arguments + "&") : ""));

    QNetworkReply* reply = m_Nam.get(QNetworkRequest(url));
    reply->ignoreSslErrors(QList<QSslError>{ QSslError::SelfSignedCertificate });

    return reply;
}
