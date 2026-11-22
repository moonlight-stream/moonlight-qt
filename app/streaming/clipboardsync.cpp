#include "clipboardsync.h"
#include "clipboard_win32.h"
#include "backend/identitymanager.h"

#include <QDebug>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>

namespace {
struct Result {
    int status = 0;
    std::optional<DWORD> sequence;
    QByteArray payload;
};

Result exchange(QNetworkAccessManager& network, const QNetworkRequest& request,
                const QSslCertificate& pairedHost, const QByteArray* upload)
{
    QEventLoop loop;
    Result result;
    auto reply = upload ? network.post(request, *upload) : network.get(request);
    // This manager is private to one paired host. Validate each new TLS
    // connection before Qt allows any HTTP data through it.
    QObject::connect(reply, &QNetworkReply::encrypted, reply, [reply, pairedHost] {
        if (reply->sslConfiguration().peerCertificate() != pairedHost) reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::sslErrors, reply, [reply, pairedHost](const QList<QSslError>& errors) {
        if (reply->sslConfiguration().peerCertificate() == pairedHost) reply->ignoreSslErrors(errors);
    });
    reply->setReadBufferSize(64 * 1024);
    auto receive = [&] {
        if (reply->bytesAvailable() > static_cast<qint64>(win_clipboard::max_body) - result.payload.size()) {
            reply->abort();
        } else {
            result.payload.append(reply->readAll());
        }
    };
    QObject::connect(reply, &QIODevice::readyRead, &loop, receive);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer cancellation;
    QObject::connect(&cancellation, &QTimer::timeout, reply, [reply] {
        if (QThread::currentThread()->isInterruptionRequested()) reply->abort();
    });
    cancellation.start(50);
    QTimer::singleShot(120000, reply, &QNetworkReply::abort);
    if (!reply->isFinished()) loop.exec();
    if (reply->bytesAvailable()) receive();
    // Qt also reports HTTP 4xx/5xx as a network error.
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() == QNetworkReply::NoError || status >= 400) {
        result.status = status;
        bool valid = false;
        DWORD sequence = reply->rawHeader("X-Clipboard-Sequence").toUInt(&valid);
        if (valid) result.sequence = sequence;
    }
    delete reply;
    return result;
}
}

ClipboardSync::ClipboardSync(QUrl host, QSslCertificate certificate, QByteArray token, bool text, bool files)
    : m_Host(std::move(host)), m_Certificate(std::move(certificate)), m_Token(std::move(token)),
      m_Text(text), m_Files(files), m_LocalSequence(GetClipboardSequenceNumber())
{
    m_Host.setPath("/clipboard");
    if (!m_Token.isEmpty() && !m_Certificate.isNull() && (m_Text || m_Files)) start();
}

ClipboardSync::~ClipboardSync()
{
    stop();
}

void ClipboardSync::stop()
{
    if (isRunning()) {
        requestInterruption();
        wait();
    }
}

void ClipboardSync::run()
{
    const auto stagingRoot = std::filesystem::path(QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation).toStdWString()) / L"Clipboard";
    QNetworkAccessManager network;
    network.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    QNetworkRequest request;
    auto config = IdentityManager::get()->getSslConfig();
    config.setCaCertificates({m_Certificate});
    request.setSslConfiguration(config);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("X-Clipboard-Token", m_Token);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));

    DWORD local = m_LocalSequence;
    std::optional<DWORD> remote;
    int failures = 0;
    while (!isInterruptionRequested()) {
        QUrl url = m_Host;
        QUrlQuery query;
        query.addQueryItem("text", m_Text ? "1" : "0");
        query.addQueryItem("files", m_Files ? "1" : "0");
        QByteArray upload;
        DWORD copied = local;
        if (remote && GetClipboardSequenceNumber() != local) {
            std::string encoded;
            int status = win_clipboard::read(m_Text, m_Files, local, encoded, copied);
            if (status == 200) upload = QByteArray(encoded.data(), static_cast<int>(encoded.size()));
            else if (status == 204 || status == 413) local = copied;
        }
        if (upload.isEmpty()) {
            query.addQueryItem("since", remote ? QString::number(*remote) : QStringLiteral("init"));
        }
        url.setQuery(query);
        request.setUrl(url);
        if (isInterruptionRequested()) break;
        auto response = exchange(network, request, m_Certificate, upload.isEmpty() ? nullptr : &upload);
        if (isInterruptionRequested()) break;
        if (response.status == 404 || response.status == 401 || (response.status == 403 && failures >= 3)) {
            qWarning() << "Clipboard sync unavailable for this session, HTTP" << response.status;
            break;
        }
        if (response.status == 204 && response.sequence) {
            remote = response.sequence;
            if (!upload.isEmpty()) local = copied;
            failures = 0;
        } else if (response.status == 200 && remote && response.sequence && upload.isEmpty()) {
            const std::string_view payload(response.payload.constData(), response.payload.size());
            DWORD applied = local;
            int status = win_clipboard::write(m_Text, m_Files, payload, applied, stagingRoot, local);
            if (status == 204) local = applied;
            if (status != 409) remote = response.sequence;
            if (status != 204 && status != 409) qWarning() << "Cannot apply incoming clipboard data, status" << status;
            failures = 0;
        } else if (response.status == 400 || response.status == 413) {
            if (!upload.isEmpty()) local = copied;
            if (response.sequence) remote = response.sequence;
            failures = 0;
        } else {
            failures = (std::min)(failures + 1, 4);
        }
        const int delay = 250 << failures;
        for (int ms = 0; ms < delay && !isInterruptionRequested(); ms += 50) msleep(50);
    }
}
