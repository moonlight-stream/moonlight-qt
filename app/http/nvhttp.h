#pragma once

#include "identitymanager.h"

#include <Limelight.h>

#include <QUrl>
#include <QtNetwork/QNetworkAccessManager>

class GfeHttpResponseException : public std::exception
{
public:
    GfeHttpResponseException(int statusCode, QString message) :
        m_StatusCode(statusCode),
        m_StatusMessage(message)
    {

    }

    const char* what() const throw()
    {
        return m_StatusMessage.toLatin1();
    }

    const char* getStatusMessage() const
    {
        return m_StatusMessage.toLatin1();
    }

    int getStatusCode() const
    {
        return m_StatusCode;
    }

private:
    int m_StatusCode;
    QString m_StatusMessage;
};

class NvHTTP
{
public:
    NvHTTP(QString address);

    int
    getCurrentGame(QString serverInfo);

    QString
    getServerInfo();

    void
    verifyResponseStatus(QString xml);

    QString
    getXmlString(QString xml,
                 QString tagName);

    QByteArray
    getXmlStringFromHex(QString xml,
                        QString tagName);

    QString
    openConnectionToString(QUrl baseUrl,
                           QString command,
                           QString arguments,
                           bool enableTimeout);

    QVector<int>
    getServerVersionQuad(QString serverInfo);

    void
    quitApp();

    void
    resumeApp(PSTREAM_CONFIGURATION streamConfig);

    void
    launchApp(int appId,
              PSTREAM_CONFIGURATION streamConfig,
              bool sops,
              bool localAudio,
              int gamepadMask);

    QUrl m_BaseUrlHttp;
    QUrl m_BaseUrlHttps;
private:
    QNetworkReply*
    openConnection(QUrl baseUrl,
                   QString command,
                   QString arguments,
                   bool enableTimeout);

    QString m_Address;
    QNetworkAccessManager m_Nam;
};
