#pragma once

#include <QUrl>
#include <QtNetwork/QNetworkAccessManager>

class NvHTTP
{
public:
    NvHTTP(QString address);

private:
    QNetworkReply*
    openConnection(QUrl baseUrl,
                   QString command,
                   QString arguments,
                   bool enableTimeout);

    QUrl m_BaseUrlHttp;
    QUrl m_BaseUrlHttps;
    QNetworkAccessManager m_Nam;
};
