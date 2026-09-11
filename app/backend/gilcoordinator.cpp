#include "gilcoordinator.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QSysInfo>
#include <QUuid>

namespace {
const char* DEFAULT_COORDINATOR_URL = "https://gilstreaming.gilservers.com:6766";

QJsonObject responseObject(QNetworkReply* reply)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return QJsonObject();
    }
    return document.object();
}
}

GilCoordinator::GilCoordinator(QObject* parent)
    : QObject(parent),
      m_Busy(false),
      m_Quitting(false)
{
    const QString configuredUrl = qEnvironmentVariable("GILSTREAMING_COORDINATOR_URL",
                                                        DEFAULT_COORDINATOR_URL);
    m_BaseUrl = QUrl(configuredUrl);

    QSettings settings;
    settings.beginGroup("coordinator");
    m_DeviceId = settings.value("deviceId").toString();
    if (m_DeviceId.isEmpty()) {
        m_DeviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue("deviceId", m_DeviceId);
    }
    settings.endGroup();

    m_LoginPollTimer.setInterval(2000);
    connect(&m_LoginPollTimer, &QTimer::timeout, this, &GilCoordinator::pollLogin);

    m_HeartbeatTimer.setInterval(30000);
    connect(&m_HeartbeatTimer, &QTimer::timeout, this, &GilCoordinator::sendHeartbeat);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]() {
        m_Quitting = true;
        releaseLease();
    });

    setStatus(tr("Sign in to request a gaming VM."));
}

bool GilCoordinator::developmentBuild() const
{
#ifdef QT_NO_DEBUG
    return false;
#else
    return true;
#endif
}

void GilCoordinator::startLogin()
{
    if (m_Busy) {
        return;
    }
    setBusy(true);
    setStatus(tr("Starting GILid sign-in…"));

    QNetworkReply* reply = m_Network.post(requestFor("/v1/auth/start"), devicePayload(true));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QJsonObject response = responseObject(reply);
        if (reply->error() != QNetworkReply::NoError) {
            fail(response.value("message").toString(tr("Could not start GILid sign-in.")));
            reply->deleteLater();
            return;
        }
        m_LoginRequestId = response.value("requestId").toString();
        const QUrl authorizeUrl(response.value("authorizeUrl").toString());
        if (m_LoginRequestId.isEmpty() || !authorizeUrl.isValid() || !QDesktopServices::openUrl(authorizeUrl)) {
            fail(tr("Could not open the GILid sign-in page."));
            reply->deleteLater();
            return;
        }
        setStatus(tr("Finish signing in with GILid in your browser…"));
        m_LoginPollTimer.start();
        reply->deleteLater();
    });
}

void GilCoordinator::skipLoginForDevelopment()
{
    if (!developmentBuild() || m_Busy) {
        return;
    }
    setBusy(true);
    setStatus(tr("Creating a development session…"));
    QNetworkReply* reply = m_Network.post(requestFor("/v1/auth/dev"), devicePayload(false));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleAuthenticationResponse(reply, responseObject(reply));
    });
}

void GilCoordinator::pollLogin()
{
    if (m_LoginRequestId.isEmpty()) {
        m_LoginPollTimer.stop();
        return;
    }
    QNetworkReply* reply = m_Network.get(requestFor("/v1/auth/status/" + m_LoginRequestId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QJsonObject response = responseObject(reply);
        if (reply->error() != QNetworkReply::NoError) {
            m_LoginPollTimer.stop();
            fail(response.value("message").toString(tr("GILid sign-in failed.")));
            reply->deleteLater();
            return;
        }
        if (response.value("state").toString() == "authenticated") {
            m_LoginPollTimer.stop();
            handleAuthenticationResponse(reply, response);
            return;
        }
        reply->deleteLater();
    });
}

void GilCoordinator::handleAuthenticationResponse(QNetworkReply* reply, const QJsonObject& response)
{
    if (reply->error() != QNetworkReply::NoError) {
        fail(response.value("message").toString(tr("Authentication failed.")));
        reply->deleteLater();
        return;
    }

    m_AccessToken = response.value("accessToken").toString();
    const QJsonObject profile = response.value("profile").toObject();
    m_ProfileName = profile.value("username").toString();
    if (m_ProfileName.isEmpty()) {
        m_ProfileName = profile.value("email").toString();
    }
    if (m_AccessToken.isEmpty()) {
        fail(tr("The coordinator returned an invalid login session."));
        reply->deleteLater();
        return;
    }

    emit authenticatedChanged();
    emit profileNameChanged();
    reply->deleteLater();
    requestVm();
}

void GilCoordinator::requestVm()
{
    if (m_AccessToken.isEmpty()) {
        fail(tr("Sign in before requesting a VM."));
        return;
    }
    setBusy(true);
    setStatus(tr("Requesting an available gaming VM…"));

    QNetworkReply* reply = m_Network.post(requestFor("/v1/leases", true), devicePayload(true));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QJsonObject response = responseObject(reply);
        if (reply->error() != QNetworkReply::NoError) {
            const QString code = response.value("code").toString();
            fail(code == "POOL_EXHAUSTED"
                     ? tr("All gaming VMs are currently in use. Try again shortly.")
                     : response.value("message").toString(tr("Could not reserve a gaming VM.")));
            reply->deleteLater();
            return;
        }

        const QJsonObject host = response.value("host").toObject();
        m_LeaseId = response.value("leaseId").toString();
        const QString address = host.value("address").toString();
        const int port = host.value("port").toInt();
        if (m_LeaseId.isEmpty() || address.isEmpty() || port < 1 || port > 65535) {
            fail(tr("The coordinator returned an invalid VM assignment."));
            reply->deleteLater();
            return;
        }

        setBusy(false);
        setStatus(tr("Connecting to %1…").arg(host.value("name").toString(address)));
        m_HeartbeatTimer.start();
        emit assignedHost(address, port);
        reply->deleteLater();
    });
}

void GilCoordinator::sendHeartbeat()
{
    if (m_LeaseId.isEmpty() || m_AccessToken.isEmpty()) {
        return;
    }
    QNetworkReply* reply = m_Network.post(
        requestFor("/v1/leases/" + m_LeaseId + "/heartbeat", true), QByteArray("{}"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            m_HeartbeatTimer.stop();
            setStatus(tr("The VM reservation was lost. Return to login and try again."));
            m_LeaseId.clear();
            emit assignmentRevoked();
        }
        reply->deleteLater();
    });
}

void GilCoordinator::approvePairing(QString pin)
{
    if (m_LeaseId.isEmpty() || m_AccessToken.isEmpty()) {
        emit pairingApprovalFailed(tr("The VM reservation is not active."));
        return;
    }

    QJsonObject payload{
        {"pin", pin},
        {"deviceName", QSysInfo::machineHostName().left(128)}
    };
    setStatus(tr("Pairing this device with the assigned VM…"));
    QNetworkReply* reply = m_Network.post(
        requestFor("/v1/leases/" + m_LeaseId + "/pair", true),
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QJsonObject response = responseObject(reply);
        if (reply->error() != QNetworkReply::NoError) {
            const QString message = response.value("message").toString(
                tr("The coordinator could not approve Sunshine pairing."));
            setStatus(message);
            emit pairingApprovalFailed(message);
        }
        else {
            setStatus(tr("Pairing approved. Loading your games…"));
        }
        reply->deleteLater();
    });
}

void GilCoordinator::releaseLease()
{
    if (m_LeaseId.isEmpty() || m_AccessToken.isEmpty()) {
        return;
    }
    QNetworkReply* reply = m_Network.deleteResource(
        requestFor("/v1/leases/" + m_LeaseId, true));
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    m_LeaseId.clear();
    m_HeartbeatTimer.stop();
    if (!m_Quitting) {
        emit assignmentRevoked();
    }
}

QByteArray GilCoordinator::devicePayload(bool includeName) const
{
    QJsonObject payload{{"deviceId", m_DeviceId}};
    if (includeName) {
        payload.insert("deviceName", QSysInfo::machineHostName());
    }
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

QNetworkRequest GilCoordinator::requestFor(QString path, bool authenticatedRequest) const
{
    QUrl url = m_BaseUrl;
    url.setPath(path);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    if (authenticatedRequest) {
        request.setRawHeader("Authorization", "Bearer " + m_AccessToken.toUtf8());
    }
    return request;
}

void GilCoordinator::setBusy(bool busy)
{
    if (m_Busy != busy) {
        m_Busy = busy;
        emit busyChanged();
    }
}

void GilCoordinator::setStatus(QString status)
{
    if (m_StatusText != status) {
        m_StatusText = status;
        emit statusTextChanged();
    }
}

void GilCoordinator::fail(QString message)
{
    setBusy(false);
    setStatus(message);
}
