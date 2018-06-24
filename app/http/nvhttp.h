#pragma once

#include "identitymanager.h"

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

class NvComputer
{
public:
    NvComputer() {}

    enum PairState
    {
        PS_UNKNOWN,
        PS_PAIRED,
        PS_NOT_PAIRED
    };

    enum ComputerState
    {
        CS_UNKNOWN,
        CS_ONLINE,
        CS_OFFLINE
    };

    QString m_Name;
    QString m_Uuid;
    QString m_MacAddress;
    QString m_LocalAddress;
    QString m_RemoteAddress;
    int m_RunningGameId;
    PairState m_PairState;
    ComputerState m_State;
};

class NvHTTP
{
public:
    NvHTTP(QString address, IdentityManager im);

    NvComputer
    getComputerInfo();

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
    IdentityManager m_Im;
};
