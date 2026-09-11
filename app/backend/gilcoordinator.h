#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

class QNetworkReply;
class QJsonObject;

class GilCoordinator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString profileName READ profileName NOTIFY profileNameChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY authenticatedChanged)
    Q_PROPERTY(bool developmentBuild READ developmentBuild CONSTANT)

public:
    explicit GilCoordinator(QObject* parent = nullptr);

    QString statusText() const { return m_StatusText; }
    QString profileName() const { return m_ProfileName; }
    bool busy() const { return m_Busy; }
    bool authenticated() const { return !m_AccessToken.isEmpty(); }
    bool developmentBuild() const;

    Q_INVOKABLE void startLogin();
    Q_INVOKABLE void skipLoginForDevelopment();
    Q_INVOKABLE void requestVm();
    Q_INVOKABLE void approvePairing(QString pin);
    Q_INVOKABLE void releaseLease();

signals:
    void statusTextChanged();
    void profileNameChanged();
    void busyChanged();
    void authenticatedChanged();
    void assignedHost(QString address, int port);
    void pairingApprovalFailed(QString message);
    void assignmentRevoked();

private slots:
    void pollLogin();
    void sendHeartbeat();

private:
    QByteArray devicePayload(bool includeName) const;
    QNetworkRequest requestFor(QString path, bool authenticatedRequest = false) const;
    void handleAuthenticationResponse(QNetworkReply* reply, const QJsonObject& response);
    void setBusy(bool busy);
    void setStatus(QString status);
    void fail(QString message);

    QNetworkAccessManager m_Network;
    QTimer m_LoginPollTimer;
    QTimer m_HeartbeatTimer;
    QUrl m_BaseUrl;
    QString m_DeviceId;
    QString m_LoginRequestId;
    QString m_AccessToken;
    QString m_LeaseId;
    QString m_StatusText;
    QString m_ProfileName;
    bool m_Busy;
    bool m_Quitting;
};
