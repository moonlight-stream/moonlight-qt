#include "companionclient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QAbstractSocket>
#include <QSslError>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QWebSocket>
#include <QDebug>

// Clés QSettings (même convention que ConsoleUi/* côté QML console).
static const char* kSettingsGroup   = "ConsoleUi/companion";
static const char* kKeyToken        = "token";
static const char* kKeyFingerprint  = "fingerprint";
static const char* kKeyHostName     = "hostName";
static const char* kKeyHostAddress  = "hostAddress";
static const char* kKeyHostPort     = "hostPort";

static const char* kServiceType     = "_hostcompanion._tcp.local.";

CompanionClient::CompanionClient(QObject* parent)
    : QObject(parent),
      m_nam(new QNetworkAccessManager(this))
{
    // TLS auto-signé du host → on gère la confiance nous-mêmes (TOFU) dans onSslErrors.
    connect(m_nam, &QNetworkAccessManager::sslErrors, this, &CompanionClient::onSslErrors);

    // Reconnexion WebSocket à backoff (protocol.md §3 : 1s → 2s → 5s, plafond 5s).
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_wantEvents && !m_eventsConnected) {
            connectEvents();
        }
    });

    loadPersisted();
}

CompanionClient::~CompanionClient()
{
    delete m_browser;
    delete m_resolver;
    // m_mdnsServer (QSharedPointer) et m_nam (enfant QObject) se libèrent seuls.
}

void CompanionClient::setState(State s)
{
    if (m_state == s) {
        return;
    }
    m_state = s;
    emit stateChanged();
}

QString CompanionClient::baseUrl() const
{
    return QStringLiteral("https://%1:%2").arg(m_hostAddress.toString()).arg(m_port);
}

void CompanionClient::loadPersisted()
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    m_token             = settings.value(kKeyToken).toString();
    m_pinnedFingerprint = settings.value(kKeyFingerprint).toString();
    m_hostName          = settings.value(kKeyHostName).toString();
    const QString addr  = settings.value(kKeyHostAddress).toString();
    m_port              = static_cast<quint16>(settings.value(kKeyHostPort, 47920).toUInt());
    settings.endGroup();

    if (!addr.isEmpty()) {
        m_hostAddress = QHostAddress(addr);
    }
    if (!m_pinnedFingerprint.isEmpty()) {
        m_certFingerprint = m_pinnedFingerprint;
    }
    if (paired()) {
        // On connaît déjà l'hôte : la (re)découverte servira juste à confirmer qu'il est joignable.
        setState(Found);
    }
}

void CompanionClient::persist()
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kKeyToken, m_token);
    settings.setValue(kKeyFingerprint, m_pinnedFingerprint);
    settings.setValue(kKeyHostName, m_hostName);
    settings.setValue(kKeyHostAddress, m_hostAddress.toString());
    settings.setValue(kKeyHostPort, m_port);
    settings.endGroup();
}

// ---------------------------------------------------------------------------
// Découverte mDNS (calquée sur ComputerManager + MdnsPendingComputer, backend/)
// ---------------------------------------------------------------------------

void CompanionClient::startDiscovery()
{
    if (m_browser) {
        return; // déjà en cours
    }
    m_mdnsServer = QSharedPointer<QMdnsEngine::Server>(new QMdnsEngine::Server());
    m_browser = new QMdnsEngine::Browser(m_mdnsServer.data(), kServiceType);
    connect(m_browser, &QMdnsEngine::Browser::serviceAdded, this, &CompanionClient::onServiceAdded);
    if (m_state == Idle) {
        setState(Discovering);
    }
    qInfo() << "CompanionClient: discovering" << kServiceType;
}

void CompanionClient::onServiceAdded(const QMdnsEngine::Service& service)
{
    // On ne gère qu'un host à la fois (v1, mono-console). Le premier annoncé gagne ;
    // si on est déjà résolu/appairé sur une adresse, on ne ré-écrase pas inutilement.
    if (m_resolver) {
        return;
    }
    m_mdnsHostname = service.hostname();
    m_port = service.port();
    qInfo() << "CompanionClient: found host" << m_mdnsHostname << "port" << m_port;

    // NB: signature qmdnsengine = Resolver(server, name, Cache* = nullptr, QObject* = nullptr).
    // On NE passe PAS `this` en 3e (ce serait un Cache*) ; on gère la durée de vie nous-mêmes.
    m_resolver = new QMdnsEngine::Resolver(m_mdnsServer.data(), m_mdnsHostname);
    connect(m_resolver, &QMdnsEngine::Resolver::resolved, this, &CompanionClient::onAddressResolved);
    // Repli si la résolution traîne : on tentera quand même /v1/info si on a une adresse.
    QTimer::singleShot(2500, this, [this]() {
        if (m_resolver) {
            m_resolver->deleteLater();
            m_resolver = nullptr;
        }
    });
}

void CompanionClient::onAddressResolved(const QHostAddress& address)
{
    if (address.isNull()) {
        return;
    }
    // Préférence IPv4 pour la simplicité du proto v1 ; on garde la première trouvée sinon.
    if (m_hostAddress.isNull() || address.protocol() == QAbstractSocket::IPv4Protocol) {
        m_hostAddress = address;
    }
    // On a de quoi joindre le host : interroger /v1/info (idempotent).
    fetchInfo();
}

void CompanionClient::connectToHost(const QString& address, quint16 port)
{
    m_hostAddress = QHostAddress(address);
    m_port = port;
    fetchInfo();
}

// ---------------------------------------------------------------------------
// REST
// ---------------------------------------------------------------------------

void CompanionClient::fetchInfo()
{
    if (m_hostAddress.isNull()) {
        return;
    }
    QNetworkRequest req(QUrl(baseUrl() + QStringLiteral("/v1/info")));
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "CompanionClient: /v1/info failed:" << reply->errorString();
            return; // on reste dans l'état courant ; la découverte pourra réessayer
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        m_hostName = obj.value(QStringLiteral("hostName")).toString(m_hostName);
        const QString jsonFp = obj.value(QStringLiteral("certFingerprint")).toString();
        if (!jsonFp.isEmpty() && !m_certFingerprint.isEmpty()
                && jsonFp.compare(m_certFingerprint, Qt::CaseInsensitive) != 0) {
            // Le fingerprint annoncé doit matcher le cert TLS réellement présenté.
            qWarning() << "CompanionClient: /v1/info fingerprint mismatch vs TLS cert";
        }
        if (m_certFingerprint.isEmpty()) {
            m_certFingerprint = jsonFp;
        }
        setState(paired() ? Paired : Found);
        emit discovered(m_hostName, m_hostAddress.toString());
        qInfo() << "CompanionClient: host" << m_hostName << "ready, fp=" << m_certFingerprint;

        if (paired()) {
            // Host connu + joignable → on (re)branche le flux temps réel et on resynchronise.
            refreshLibrary();
            connectEvents();
        }
    });
}

void CompanionClient::startPairing(const QString& deviceName)
{
    if (m_hostAddress.isNull()) {
        emit errorOccurred(QStringLiteral("Aucun host joignable"));
        return;
    }
    m_pendingDeviceName = deviceName;

    QNetworkRequest req(QUrl(baseUrl() + QStringLiteral("/v1/pair/start")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject body{ { QStringLiteral("deviceName"), deviceName } };

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(QStringLiteral("pair/start: ") + reply->errorString());
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        m_pairingId = obj.value(QStringLiteral("pairingId")).toString();
        const int expires = obj.value(QStringLiteral("expiresInSec")).toInt(120);
        setState(Pairing);
        emit pairingStarted(expires);
    });
}

void CompanionClient::confirmPairing(const QString& code)
{
    if (m_pairingId.isEmpty()) {
        emit errorOccurred(QStringLiteral("Aucun appairage en cours"));
        return;
    }
    QNetworkRequest req(QUrl(baseUrl() + QStringLiteral("/v1/pair/confirm")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject body{
        { QStringLiteral("pairingId"), m_pairingId },
        { QStringLiteral("code"), code },
    };

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();

        if (http == 200) {
            const QJsonObject obj = QJsonDocument::fromJson(payload).object();
            m_token = obj.value(QStringLiteral("token")).toString();
            m_hostName = obj.value(QStringLiteral("hostName")).toString(m_hostName);
            // Fige le pin TOFU sur le fingerprint vu pendant le handshake de cette session.
            m_pinnedFingerprint = m_certFingerprint;
            m_pairingId.clear();
            persist();
            setState(Paired);
            emit pairingSucceeded();
            // Enchaîne directement sur la bibliothèque + le flux d'événements.
            refreshLibrary();
            connectEvents();
            return;
        }

        // 403 PAIR_CODE_INVALID / 410 PAIR_EXPIRED → code d'erreur contractuel.
        QString errCode = QStringLiteral("PAIR_FAILED");
        const QJsonObject err = QJsonDocument::fromJson(payload).object()
                                    .value(QStringLiteral("error")).toObject();
        if (!err.isEmpty()) {
            errCode = err.value(QStringLiteral("code")).toString(errCode);
        }
        emit pairingFailed(errCode);
    });
}

void CompanionClient::forgetHost()
{
    disconnectEvents();
    m_token.clear();
    m_pinnedFingerprint.clear();
    m_pairingId.clear();
    m_libraryGames = QJsonArray();
    m_libraryEtag.clear();
    emit libraryChanged();
    persist();
    setState(m_hostAddress.isNull() ? Idle : Found);
}

// ---------------------------------------------------------------------------
// TLS / TOFU
// ---------------------------------------------------------------------------

// Coeur de la confiance TOFU, partagé par le REST (onSslErrors) et le WS.
// fingerprintHex = SHA-256 hex (minuscule) du cert DER présenté. *ignore vaut true
// si l'on doit accepter ce cert (premier contact, ou match du pin).
void CompanionClient::pinTofu(const QString& fingerprintHex, bool* ignore)
{
    *ignore = false;
    if (fingerprintHex.isEmpty()) {
        qWarning() << "CompanionClient: erreur TLS sans certificat — refus";
        return;
    }
    if (m_pinnedFingerprint.isEmpty()) {
        // Premier contact (pas encore appairé) : TOFU — on accepte et on mémorise.
        m_certFingerprint = fingerprintHex;
        *ignore = true;
        return;
    }
    if (fingerprintHex.compare(m_pinnedFingerprint, Qt::CaseInsensitive) == 0) {
        m_certFingerprint = fingerprintHex;
        *ignore = true;
        return;
    }
    // Mismatch : cert changé après appairage → on refuse (potentielle MITM).
    qWarning() << "CompanionClient: fingerprint TLS inattendu" << fingerprintHex
               << "attendu" << m_pinnedFingerprint << "— refus";
    emit errorOccurred(QStringLiteral("Certificat du host inattendu (appairage compromis ?)"));
}

void CompanionClient::onSslErrors(QNetworkReply* reply, const QList<QSslError>& errors)
{
    // Cert auto-signé attendu : on lit le SHA-256 du DER et on applique TOFU.
    QSslCertificate cert = reply->sslConfiguration().peerCertificate();
    if (cert.isNull() && !errors.isEmpty()) {
        cert = errors.first().certificate();
    }
    const QString fp = cert.isNull() ? QString()
        : QString::fromLatin1(cert.digest(QCryptographicHash::Sha256).toHex()).toLower();

    bool ignore = false;
    pinTofu(fp, &ignore);
    if (ignore) {
        reply->ignoreSslErrors();
    }
}

// ---------------------------------------------------------------------------
// Tranche 2 : bibliothèque (REST)
// ---------------------------------------------------------------------------

QNetworkRequest CompanionClient::authedRequest(const QString& path) const
{
    QNetworkRequest req(QUrl(baseUrl() + path));
    if (!m_token.isEmpty()) {
        req.setRawHeader("Authorization", QByteArray("Bearer ") + m_token.toUtf8());
    }
    return req;
}

void CompanionClient::refreshLibrary()
{
    if (m_hostAddress.isNull() || m_token.isEmpty()) {
        return;
    }
    QNetworkRequest req = authedRequest(QStringLiteral("/v1/library"));
    if (!m_libraryEtag.isEmpty()) {
        req.setRawHeader("If-None-Match", m_libraryEtag.toUtf8());
    }
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (http == 304) {
            return; // inchangé depuis notre etag
        }
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "CompanionClient: /v1/library failed:" << reply->errorString();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        m_libraryGames = obj.value(QStringLiteral("games")).toArray();
        // L'en-tête ETag (avec guillemets) sert le If-None-Match ; repli sur le corps.
        const QByteArray etagHeader = reply->rawHeader("ETag");
        m_libraryEtag = etagHeader.isEmpty()
            ? obj.value(QStringLiteral("etag")).toString()
            : QString::fromUtf8(etagHeader);
        emit libraryChanged();
        qInfo() << "CompanionClient: library updated," << m_libraryGames.size() << "games";
    });
}

QVariantList CompanionClient::games() const
{
    return m_libraryGames.toVariantList();
}

QString CompanionClient::mediaUrl(const QString& gameId, const QString& kind) const
{
    return baseUrl() + QStringLiteral("/v1/media/") + gameId + QLatin1Char('/') + kind;
}

// ---------------------------------------------------------------------------
// Tranche 2 : WebSocket /v1/events (+ reconnexion à backoff)
// ---------------------------------------------------------------------------

void CompanionClient::connectEvents()
{
    if (m_hostAddress.isNull() || m_token.isEmpty()) {
        return;
    }
    m_wantEvents = true;
    if (m_ws && (m_ws->state() == QAbstractSocket::ConnectedState
                 || m_ws->state() == QAbstractSocket::ConnectingState)) {
        return; // déjà (en cours de) connecté
    }
    if (!m_ws) {
        m_ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
        connect(m_ws, &QWebSocket::connected, this, &CompanionClient::onWsConnected);
        connect(m_ws, &QWebSocket::disconnected, this, &CompanionClient::onWsDisconnected);
        connect(m_ws, &QWebSocket::textMessageReceived, this, &CompanionClient::onWsTextMessage);
        // Même confiance TOFU que le REST.
        connect(m_ws, &QWebSocket::sslErrors, this, [this](const QList<QSslError>& errors) {
            QSslCertificate cert = m_ws->sslConfiguration().peerCertificate();
            if (cert.isNull() && !errors.isEmpty()) {
                cert = errors.first().certificate();
            }
            const QString fp = cert.isNull() ? QString()
                : QString::fromLatin1(cert.digest(QCryptographicHash::Sha256).toHex()).toLower();
            bool ignore = false;
            pinTofu(fp, &ignore);
            if (ignore) {
                m_ws->ignoreSslErrors();
            }
        });
    }
    // Token en query (protocol.md §1/§3 : les WS navigateur n'envoient pas d'en-têtes).
    QUrl url(QStringLiteral("wss://%1:%2/v1/events").arg(m_hostAddress.toString()).arg(m_port));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("token"), m_token);
    url.setQuery(q);
    m_ws->open(url);
}

void CompanionClient::disconnectEvents()
{
    m_wantEvents = false;
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    if (m_ws) {
        m_ws->close();
    }
}

void CompanionClient::scheduleReconnect()
{
    if (!m_wantEvents) {
        return;
    }
    m_reconnectTimer->start(m_reconnectDelayMs);
    m_reconnectDelayMs = qMin(m_reconnectDelayMs * 2, 5000); // backoff 1s→2s→5s
}

void CompanionClient::onWsConnected()
{
    m_eventsConnected = true;
    m_reconnectDelayMs = 1000; // reset du backoff
    emit eventsConnectedChanged();
    qInfo() << "CompanionClient: events connected";
}

void CompanionClient::onWsDisconnected()
{
    const bool was = m_eventsConnected;
    m_eventsConnected = false;
    if (was) {
        emit eventsConnectedChanged();
    }
    qInfo() << "CompanionClient: events disconnected";
    scheduleReconnect();
}

void CompanionClient::onWsTextMessage(const QString& message)
{
    const QJsonObject env = QJsonDocument::fromJson(message.toUtf8()).object();
    const QString type = env.value(QStringLiteral("type")).toString();
    const QJsonObject p = env.value(QStringLiteral("payload")).toObject();

    if (type == QLatin1String("LAUNCH_STATE")) {
        const QString st = p.value("state").toString();
        emit launchStateChanged(p.value("sessionId").toString(),
                                p.value("gameId").toString(), st);
        if (st == QLatin1String("IDLE") || st == QLatin1String("ABORTED")) {
            m_launchSessionId.clear(); // session terminée côté host
        }
    } else if (type == QLatin1String("UPDATE_REQUIRED")) {
        emit updateRequired(p.value("sessionId").toString(),
                            p.value("gameId").toString(),
                            p.value("sizeBytes").toDouble());
    } else if (type == QLatin1String("UPDATE_PROGRESS")) {
        emit updateProgress(p.value("sessionId").toString(),
                            p.value("pct").toInt(),
                            p.value("bytesDone").toDouble(),
                            p.value("bytesTotal").toDouble());
    } else if (type == QLatin1String("READY")) {
        emit launchReady(p.value("sessionId").toString(),
                         p.value("apolloAppId").toString());
    } else if (type == QLatin1String("LAUNCH_ERROR")) {
        emit launchFailed(p.value("sessionId").toString(),
                          p.value("code").toString(),
                          p.value("message").toString());
        m_launchSessionId.clear();
    } else if (type == QLatin1String("GAME_STARTED")) {
        emit gameStarted(p.value("gameId").toString());
    } else if (type == QLatin1String("GAME_STOPPED")) {
        emit gameStopped(p.value("gameId").toString(),
                         p.value("playtimeSessionSec").toInt());
    } else if (type == QLatin1String("LIBRARY_UPDATED")) {
        // Le GET conditionnel (If-None-Match) déduplique : refetch systématique sûr.
        refreshLibrary();
    } else {
        qInfo() << "CompanionClient: unknown event type" << type;
    }
}

// ---------------------------------------------------------------------------
// Tranche 3 : lancement (REST) + auto-soumission du PIN Moonlight
// ---------------------------------------------------------------------------

void CompanionClient::launch(const QString& gameId)
{
    if (m_token.isEmpty()) {
        emit launchFailed(QString(), QStringLiteral("UNAUTHORIZED"),
                          QStringLiteral("non appairé au host"));
        return;
    }
    QNetworkRequest req = authedRequest(QStringLiteral("/v1/launch"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject body{ { QStringLiteral("gameId"), gameId } };

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (http == 200) {
            m_launchSessionId = obj.value(QStringLiteral("sessionId")).toString();
            emit launchStarted(m_launchSessionId);
            return;
        }
        const QString code = obj.value(QStringLiteral("error")).toObject()
                                 .value(QStringLiteral("code")).toString(QStringLiteral("LAUNCH_REJECTED"));
        emit launchFailed(QString(), code, QString());
    });
}

void CompanionClient::respondUpdate(bool accept)
{
    if (m_launchSessionId.isEmpty()) {
        return;
    }
    QNetworkRequest req = authedRequest(
        QStringLiteral("/v1/launch/") + m_launchSessionId + QStringLiteral("/update"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject body{ { QStringLiteral("accept"), accept } };
    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void CompanionClient::cancelLaunch()
{
    if (m_launchSessionId.isEmpty()) {
        return;
    }
    QNetworkRequest req = authedRequest(
        QStringLiteral("/v1/launch/") + m_launchSessionId + QStringLiteral("/cancel"));
    QNetworkReply* reply = m_nam->post(req, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_launchSessionId.clear();
    });
}

void CompanionClient::quitGame()
{
    if (m_token.isEmpty()) {
        return;
    }
    QNetworkRequest req = authedRequest(QStringLiteral("/v1/quit"));
    QNetworkReply* reply = m_nam->post(req, QByteArray());
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void CompanionClient::submitMoonlightPin(const QString& pin)
{
    if (m_token.isEmpty()) {
        return; // pas encore appairé Companion → le PIN suivra le chemin de repli
    }
    QNetworkRequest req = authedRequest(QStringLiteral("/v1/pair/moonlight-pin"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject body{ { QStringLiteral("pin"), pin } };
    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        reply->deleteLater();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (http != 204 && http != 200) {
            qWarning() << "CompanionClient: moonlight-pin refusé par le host:" << http;
        }
    });
}

QString CompanionClient::gameIdForName(const QString& appName) const
{
    for (const QJsonValue& v : m_libraryGames) {
        const QJsonObject g = v.toObject();
        if (g.value(QStringLiteral("name")).toString().compare(appName, Qt::CaseInsensitive) == 0) {
            return g.value(QStringLiteral("id")).toString();
        }
    }
    return QString();
}
