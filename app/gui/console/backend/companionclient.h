#pragma once

// CompanionClient — pont console → HostCompanion (voir ../../../../HostCompanion,
// docs/protocol.md : la SOURCE DE VÉRITÉ du protocole). Vit côté console (fork
// moonlight-qt) et n'est compilé QUE dans le build `embedded` (CONSOLE_UI), comme
// le reste de app/gui/console/. Aucune dépendance depuis le build vanilla (§4 du
// CLAUDE.md console : fichiers neufs, isolés, coutures minuscules).
//
// Découpage en tranches (cette classe grossit par tranches indépendantes) :
//   Tranche 1 (FAITE) : découverte mDNS de `_hostcompanion._tcp`, GET /v1/info
//     (capture du fingerprint cert, pinning TOFU), appairage par code à 6 chiffres
//     (/v1/pair/start + /v1/pair/confirm), persistance du token (QSettings).
//   Tranche 2 (CE FICHIER) : GET /v1/library (+ etag), helper média, WebSocket
//     /v1/events (reçoit LAUNCH_STATE/UPDATE_*/READY/GAME_*/LIBRARY_UPDATED) avec
//     reconnexion à backoff.
//   Tranche 3 (à venir) : recâblage du lancement — POST /v1/launch → attente de
//     READY en WS → ALORS seulement StreamSegue (remplace le launch Apollo direct
//     de ConsoleHome.launchApp, cf P0-2 de l'audit) + écran de saisie du PIN.

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QHostAddress>
#include <QVector>
#include <QVariant>
#include <QJsonArray>
#include <QSharedPointer>

#include <qmdnsengine/server.h>
#include <qmdnsengine/browser.h>
#include <qmdnsengine/service.h>
#include <qmdnsengine/resolver.h>

class QNetworkAccessManager;
class QNetworkRequest;
class QNetworkReply;
class QSslError;
class QWebSocket;
class QTimer;

class CompanionClient : public QObject
{
    Q_OBJECT

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString hostName READ hostName NOTIFY stateChanged)
    Q_PROPERTY(QString hostAddress READ hostAddress NOTIFY stateChanged)
    Q_PROPERTY(QString certFingerprint READ certFingerprint NOTIFY stateChanged)
    Q_PROPERTY(bool paired READ paired NOTIFY stateChanged)
    Q_PROPERTY(QString libraryEtag READ libraryEtag NOTIFY libraryChanged)
    Q_PROPERTY(bool eventsConnected READ eventsConnected NOTIFY eventsConnectedChanged)

public:
    enum State {
        Idle,         // rien en cours
        Discovering,  // browser mDNS actif, host pas encore trouvé/joignable
        Found,        // host découvert + /v1/info OK (fingerprint connu) — prêt à appairer
        Pairing,      // pair/start envoyé, en attente du code utilisateur
        Paired,       // token obtenu et persisté
        Error
    };
    Q_ENUM(State)

    explicit CompanionClient(QObject* parent = nullptr);
    ~CompanionClient() override;

    State state() const { return m_state; }
    QString hostName() const { return m_hostName; }
    QString hostAddress() const { return m_hostAddress.toString(); }
    QString certFingerprint() const { return m_certFingerprint; }
    bool paired() const { return !m_token.isEmpty(); }
    QString libraryEtag() const { return m_libraryEtag; }
    bool eventsConnected() const { return m_eventsConnected; }

    // --- Tranche 1 : découverte + appairage (invoqués depuis QML) ---

    // Démarre le browser mDNS (_hostcompanion._tcp.local.). À la découverte,
    // résout l'adresse puis appelle GET /v1/info (émet discovered + passe en Found).
    Q_INVOKABLE void startDiscovery();

    // Repli si mDNS échoue (Tailscale / IP directe) : cible un host explicitement
    // puis GET /v1/info.
    Q_INVOKABLE void connectToHost(const QString& address, quint16 port = 47920);

    // POST /v1/pair/start { deviceName }. Le host génère un code à 6 chiffres
    // affiché de SON côté (toast tray) ; émet pairingStarted(expiresInSec).
    Q_INVOKABLE void startPairing(const QString& deviceName);

    // POST /v1/pair/confirm { pairingId, code }. Succès → token persisté + paired().
    Q_INVOKABLE void confirmPairing(const QString& code);

    // Oublie le host appairé (token, fingerprint, adresse) — « Oublier ce PC ».
    Q_INVOKABLE void forgetHost();

    // --- Tranche 2 : bibliothèque + événements ---

    // GET /v1/library (Bearer + If-None-Match). 200 → maj du cache + libraryChanged ;
    // 304 → rien. Appelé automatiquement après appairage et sur LIBRARY_UPDATED.
    Q_INVOKABLE void refreshLibrary();

    // Snapshot courant : liste de GameInfo (QVariantMap) telle que protocol.md §4.
    Q_INVOKABLE QVariantList games() const;

    // URL d'un média (cover|bg|icon|screenshot-N). ⚠️ auth Bearer non posable par
    // QML Image → résolu en tranche 3 (QQuickImageProvider ou ?token= côté host).
    Q_INVOKABLE QString mediaUrl(const QString& gameId, const QString& kind) const;

    // (Dé)connecte le WebSocket /v1/events. connectEvents() est aussi appelé seul
    // dès que le host appairé est joignable (fetchInfo OK).
    Q_INVOKABLE void connectEvents();
    Q_INVOKABLE void disconnectEvents();

    // --- Tranche 3 : lancement (recâble le launch Apollo direct de ConsoleHome) ---

    // POST /v1/launch { gameId } → la suite arrive en WS (launchStateChanged, READY…).
    Q_INVOKABLE void launch(const QString& gameId);
    // POST /v1/launch/{id}/update { accept } — réponse au dialog de mise à jour.
    Q_INVOKABLE void respondUpdate(bool accept);
    // POST /v1/launch/{id}/cancel — abandon avant le stream (bouton B).
    Q_INVOKABLE void cancelLaunch();
    // POST /v1/quit — quitter proprement le jeu en cours (depuis Paramètres console).
    Q_INVOKABLE void quitGame();
    // POST /v1/pair/moonlight-pin { pin } — le Companion soumet le PIN à Apollo
    // (rend l'appairage Moonlight invisible une fois le Companion appairé).
    Q_INVOKABLE void submitMoonlightPin(const QString& pin);
    // Map nom d'app Apollo (carrousel) → gameId Companion (contrat : noms uniques, §4).
    Q_INVOKABLE QString gameIdForName(const QString& appName) const;

signals:
    void stateChanged();
    void discovered(const QString& hostName, const QString& address);
    void pairingStarted(int expiresInSec);
    void pairingSucceeded();                  // confirm réussi (≠ propriété `paired`)
    void pairingFailed(const QString& code);  // PAIR_CODE_INVALID | PAIR_EXPIRED
    void errorOccurred(const QString& message);

    void libraryChanged();
    void eventsConnectedChanged();

    // Événements poussés par /v1/events (payloads de protocol.md §3).
    void launchStateChanged(const QString& sessionId, const QString& gameId, const QString& state);
    void updateRequired(const QString& sessionId, const QString& gameId, double sizeBytes);
    void updateProgress(const QString& sessionId, int pct, double bytesDone, double bytesTotal);
    void launchStarted(const QString& sessionId);   // POST /v1/launch accepté
    void launchReady(const QString& sessionId, const QString& apolloAppId);
    void launchFailed(const QString& sessionId, const QString& code, const QString& message);
    void gameStarted(const QString& gameId);
    void gameStopped(const QString& gameId, int playtimeSessionSec);

private slots:
    void onServiceAdded(const QMdnsEngine::Service& service);
    void onAddressResolved(const QHostAddress& address);
    void onSslErrors(QNetworkReply* reply, const QList<QSslError>& errors);
    void onWsConnected();
    void onWsDisconnected();
    void onWsTextMessage(const QString& message);

private:
    void setState(State s);
    void fetchInfo();                         // GET /v1/info
    QString baseUrl() const;                  // https://<host>:<port>
    QNetworkRequest authedRequest(const QString& path) const; // + Authorization: Bearer
    void pinTofu(const QString& fingerprintHex, bool* ignore); // applique la confiance TOFU
    void scheduleReconnect();
    void loadPersisted();
    void persist();

    // Réseau (TLS auto-signé → pinning TOFU sur le SHA-256 du cert DER)
    QNetworkAccessManager* m_nam = nullptr;
    QString m_pinnedFingerprint;              // vide tant que pas appairé (TOFU non figé)

    // Découverte mDNS — calquée sur ComputerManager/MdnsPendingComputer (backend/)
    QSharedPointer<QMdnsEngine::Server> m_mdnsServer;
    QMdnsEngine::Browser* m_browser = nullptr;
    QMdnsEngine::Resolver* m_resolver = nullptr;

    // WebSocket /v1/events + reconnexion à backoff (1s → 2s → 5s, plafond 5s)
    QWebSocket* m_ws = nullptr;
    QTimer* m_reconnectTimer = nullptr;
    bool m_eventsConnected = false;
    bool m_wantEvents = false;                // on souhaite rester connecté (pilote la reco)
    int m_reconnectDelayMs = 1000;

    // Host courant
    State m_state = Idle;
    QString m_hostName;
    QByteArray m_mdnsHostname;                 // nom mDNS à résoudre (xxx.local.)
    QHostAddress m_hostAddress;
    quint16 m_port = 47920;
    QString m_certFingerprint;                 // fingerprint vu lors du dernier /v1/info

    // Appairage
    QString m_pairingId;
    QString m_pendingDeviceName;
    QString m_token;                           // Bearer, persisté (QSettings)

    // Lancement (session courante côté host)
    QString m_launchSessionId;

    // Bibliothèque (snapshot + etag)
    QJsonArray m_libraryGames;
    QString m_libraryEtag;
};
