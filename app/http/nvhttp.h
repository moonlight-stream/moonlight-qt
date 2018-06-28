#pragma once

#include "identitymanager.h"

#include <Limelight.h>

#include <QUrl>
#include <QtNetwork/QNetworkAccessManager>

class NvApp
{
public:
    bool operator==(const NvApp& other) const
    {
        return id == other.id;
    }

    bool isInitialized()
    {
        return id != 0 && !name.isNull();
    }

    int id;
    QString name;
    bool hdrSupported;
};

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
    explicit NvHTTP(QString address);

    static
    int
    getCurrentGame(QString serverInfo);

    QString
    getServerInfo();

    static
    void
    verifyResponseStatus(QString xml);

    static
    QString
    getXmlString(QString xml,
                 QString tagName);

    static
    QByteArray
    getXmlStringFromHex(QString xml,
                        QString tagName);

    QString
    openConnectionToString(QUrl baseUrl,
                           QString command,
                           QString arguments,
                           bool enableTimeout);

    static
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

    QVector<NvApp>
    getAppList();

    QImage
    getBoxArt(int appId);

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
