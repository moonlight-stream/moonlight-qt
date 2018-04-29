#pragma once

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
        return m_StatusMessage.toStdString().c_str();
    }

    const char* getStatusMessage()
    {
        return m_StatusMessage.toStdString().c_str();
    }

    int getStatusCode()
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
    NvHTTP(QString address);

private:
    QNetworkReply*
    openConnection(QUrl baseUrl,
                   QString command,
                   QString arguments,
                   bool enableTimeout);

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

    QString
    openConnectionToString(QUrl baseUrl,
                           QString command,
                           QString arguments,
                           bool enableTimeout);

    QString m_Address;
    QUrl m_BaseUrlHttp;
    QUrl m_BaseUrlHttps;
    QNetworkAccessManager m_Nam;
};
