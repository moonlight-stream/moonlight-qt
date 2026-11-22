#include "clipboardsync.h"
#include "clipboard_win32.h"
#include "backend/identitymanager.h"

#include <Limelight.h>
#include <QDebug>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QStandardPaths>
#include <QTimer>
#include <QUdpSocket>
#include <QUrlQuery>
#include <thread>

namespace {
qint64 monotonicMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Result {
    int status = 0;
    std::optional<DWORD> sequence;
    std::optional<quint64> nextOffset;
    QByteArray payload;
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
    QByteArray conflict;
};

Result exchange(QNetworkAccessManager& network, const QNetworkRequest& request,
                const QSslCertificate& pairedHost, const QByteArray* upload,
                const std::function<bool()>& cancelled = {}, int timeoutMs = 120000)
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
    QObject::connect(&cancellation, &QTimer::timeout, reply, [reply, &cancelled] {
        if (QThread::currentThread()->isInterruptionRequested() || (cancelled && cancelled())) reply->abort();
    });
    cancellation.start(50);
    QTimer::singleShot(timeoutMs, reply, &QNetworkReply::abort);
    if (!reply->isFinished()) loop.exec();
    if (reply->bytesAvailable()) receive();
    // Qt also reports HTTP 4xx/5xx as a network error.
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.networkError = reply->error();
    if (result.networkError == QNetworkReply::NoError || status >= 400) {
        result.status = status;
        bool valid = false;
        DWORD sequence = reply->rawHeader("X-Clipboard-Sequence").toUInt(&valid);
        if (valid) result.sequence = sequence;
        const auto offset = reply->rawHeader("X-Clipboard-Offset").toULongLong(&valid);
        if (valid) result.nextOffset = offset;
        if (status == 409) {
            const auto cause = reply->rawHeader("X-Clipboard-Conflict");
            if (cause == "busy" || cause == "sequence" || cause == "offset" || cause == "desktop" || cause == "shell") {
                result.conflict = cause;
            }
        }
    }
    delete reply;
    return result;
}

bool waitForUpload(qint64 deadline, const std::function<bool()>& cancelled)
{
    while (!cancelled()) {
        const qint64 remaining = deadline - monotonicMs();
        if (remaining <= 0) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds((std::min<qint64>)(remaining, 25)));
    }
    return false;
}

bool directlyConnectedEthernet(const QUrl& host)
{
    const QHostAddress peer(host.host());
    if (peer.isLoopback()) return true;
    if (peer.isNull()) return false;
    QUdpSocket route;
    route.connectToHost(peer, static_cast<quint16>(host.port(443)));
    const auto local = route.localAddress();
    for (const auto& nic : QNetworkInterface::allInterfaces()) {
        if (nic.type() != QNetworkInterface::Ethernet) continue;
        for (const auto& entry : nic.addressEntries()) {
            if (entry.ip() == local && entry.prefixLength() > 0 && peer.isInSubnet(local, entry.prefixLength())) {
                return true;
            }
        }
    }
    return false;
}

// One request in flight; allow at most a short burst, including the first request after a stalled ACK.
class ClipboardUploadPacer
{
public:
    explicit ClipboardUploadPacer(const QUrl& host)
        : m_Ceiling(directlyConnectedEthernet(host) ? 16 * 1024 * 1024 : m_Rate)
    {
        LiGetEstimatedRttInfo(&m_BaseRtt, nullptr);
    }

    DWORD nextSize(quint64 remaining)
    {
        const auto now = monotonicMs();
        if (now - m_LastSample >= 500) {
            m_LastSample = now;
            uint32_t rtt = 0, variance = 0;
            if (LiGetEstimatedRttInfo(&rtt, &variance)) {
                if (!m_BaseRtt || rtt < m_BaseRtt) m_BaseRtt = rtt;
                // Back off as soon as the streaming control packets start queueing.
                if (rtt - m_BaseRtt > (std::max)(12U, m_BaseRtt / 3) || variance > (std::max)(12U, m_BaseRtt / 3)) {
                    m_Rate = (std::max)(128 * 1024, m_Rate / 2);
                } else {
                    m_Rate = (std::min)(m_Ceiling, m_Rate + m_Rate / 4);
                }
            }
        }
        return static_cast<DWORD>((std::min<quint64>)(remaining, (std::min)(256 * 1024, m_Rate / 32)));
    }

    bool wait(DWORD bytes, const std::function<bool()>& cancelled)
    {
        const auto due = m_LastSend + (static_cast<qint64>(bytes) * 1000 + m_Rate - 1) / m_Rate;
        if (!waitForUpload(due, cancelled)) return false;
        m_LastSend = monotonicMs();
        return true;
    }

private:
    int m_Rate = 2 * 1024 * 1024; // bytes per second
    const int m_Ceiling;
    uint32_t m_BaseRtt = 0;
    qint64 m_LastSend = monotonicMs();
    qint64 m_LastSample = m_LastSend;
};

Result uploadFileBatch(QNetworkAccessManager& network, QNetworkRequest request, const QUrl& host,
                       const QSslCertificate& pairedHost, const std::vector<win_clipboard::detail::source_file>& files,
                       DWORD expectedHostSequence, const std::function<bool()>& cancelled,
                       const std::function<void(const ClipboardUploadProgress&)>& update)
{
    constexpr quint64 MaxBatchBytes = 16ULL * 1024 * 1024 * 1024;
    quint64 acknowledged = 0;
    size_t index = 0;
    quint64 offset = 0;
    auto failure = [&](const char* operation, Result result, const char* reason = "response") {
        qWarning() << "Clipboard file upload failed" << operation << reason << "HTTP" << result.status
                   << "network error" << result.networkError << "conflict" << result.conflict
                   << "file index" << index << "file offset" << offset << "acknowledged bytes" << acknowledged;
        // Only a valid commit response may release the pending paste.
        result.sequence.reset();
        return result;
    };
    QJsonArray manifest;
    ClipboardUploadProgress progress;
    quint64 total = 0;
    for (const auto& file : files) {
        if (file.size > MaxBatchBytes - total) return failure("manifest", {413}, "size limit");
        total += file.size;
        const auto name = QString::fromUtf8(file.name.data(), static_cast<int>(file.name.size()));
        progress.names.append(name);
        manifest.append(QJsonObject{{"name", name},
                                    {"size", static_cast<double>(file.size)}});
    }
    progress.total = total;
    progress.fileSize = files.front().size;
    update(progress);

    QUrl url = host;
    url.setPath("/clipboard/batch");
    auto post = [&](const QUrlQuery& query, const QByteArray& body, bool cleanup = false) {
        url.setQuery(query);
        request.setUrl(url);
        if (!cleanup && cancelled()) {
            Result result;
            result.networkError = QNetworkReply::OperationCanceledError;
            return result;
        }
        return exchange(network, request, pairedHost, &body, cleanup ? std::function<bool()>{} : cancelled,
                        cleanup ? 3000 : 120000);
    };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    auto begin = post(QUrlQuery("op=begin"), QJsonDocument(manifest).toJson(QJsonDocument::Compact));
    if (begin.status != 200) return failure("begin", begin);
    const auto& id = begin.payload;
    if (id.size() != 32 || !std::all_of(id.begin(), id.end(), [](char ch) {
            return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
        })) return failure("begin", begin, "invalid batch identifier");

    auto batchQuery = [&](const QString& op) {
        QUrlQuery query;
        query.addQueryItem("op", op);
        query.addQueryItem("id", QString::fromLatin1(id));
        return query;
    };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    auto send = [&]() -> Result {
        ClipboardUploadPacer pacer(host);
        QByteArray chunk;
        for (index = 0; index < files.size(); ++index) {
            const auto& file = files[index];
            offset = 0;
            progress.index = static_cast<int>(index);
            progress.fileBytes = 0;
            progress.fileSize = file.size;
            update(progress);
            while (offset < file.size) {
                const DWORD length = pacer.nextSize(file.size - offset);
                if (!pacer.wait(length, cancelled)) return failure("chunk", {}, "cancelled");
                chunk.resize(length);
                DWORD count = 0;
                if (!ReadFile(file.handle.get(), chunk.data(), length, &count, nullptr) || count != length) {
                    return failure("chunk", {}, "local file read");
                }
                auto query = batchQuery("chunk");
                query.addQueryItem("file", QString::number(index));
                query.addQueryItem("offset", QString::number(offset));
                auto result = post(query, chunk);
                if (result.status != 204) return failure("chunk", result);
                if (result.nextOffset != offset + length) return failure("chunk", result, "unexpected acknowledgement offset");
                offset += length;
                acknowledged += length;
                progress.fileBytes = offset;
                progress.acknowledged = acknowledged;
                update(progress);
            }
        }
        auto query = batchQuery("commit");
        query.addQueryItem("since", QString::number(expectedHostSequence));
        progress.committing = true;
        update(progress);
        // Keep the original revision guard on every attempt. Older hosts return an ambiguous
        // 409, and a newer remote clipboard must always win.
        for (int attempt = 0; ; ++attempt) {
            auto result = post(query, {});
            if (result.status == 204 && result.sequence) return result;
            if (result.status != 409 || result.conflict != "busy" || attempt >= 5) return failure("commit", result);
            if (!waitForUpload(monotonicMs() + (100 << attempt), cancelled)) return failure("commit", {}, "cancelled");
        }
    };
    auto result = send();
    if (result.status != 204 || !result.sequence) {
        auto cleanup = post(batchQuery("abort"), {}, true);
        if (cleanup.status != 204 && cleanup.status != 404) failure("abort", cleanup);
    }
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

bool ClipboardSync::requestPaste()
{
    if (!isRunning()) return false;
    const DWORD sequence = GetClipboardSequenceNumber();
    const bool hasFiles = IsClipboardFormatAvailable(CF_HDROP);
    if (!(hasFiles ? m_Files : m_Text && IsClipboardFormatAvailable(CF_UNICODETEXT))) return false;

    if (m_PasteCount && (m_PasteSequence == sequence || m_PasteSequence == Ready)) {
        ++m_PasteCount;
        return true;
    }
    if (m_ConfirmedSequence == sequence) return false;
    m_PasteCount = 1;
    m_PasteActivity = monotonicMs();
    m_PasteSequence = sequence;
    return true;
}

void ClipboardSync::cancelPaste()
{
    ++m_CancelGeneration;
    m_PasteSequence = NoPaste;
    m_PasteCount = 0;
    std::lock_guard lock(m_UploadMutex);
    m_Upload = {};
}

ClipboardUploadProgress ClipboardSync::fileUploadProgress() const
{
    if (!pastePending()) return {};
    std::lock_guard lock(m_UploadMutex);
    return m_Upload;
}

int ClipboardSync::takeReadyPastes()
{
    if (!m_PasteCount) return 0;
    const auto state = m_PasteSequence.load();
    if (state != m_BatchSequence && monotonicMs() - m_PasteActivity.load() > 15000) {
        qWarning() << "Clipboard paste timed out";
        cancelPaste();
        return 0;
    }
    if (state != NoPaste && state != Ready && isRunning()) return 0;
    const int count = state == Ready ? m_PasteCount : 0;
    cancelPaste();
    return count;
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
        const DWORD current = GetClipboardSequenceNumber();
        auto paste = m_PasteSequence.load();
        if (paste <= UINT32_MAX && current != paste) {
            m_PasteSequence.compare_exchange_strong(paste, NoPaste);
        }
        const bool explicitPaste = paste == current;
        if (explicitPaste && m_ConfirmedSequence == current) {
            m_PasteSequence.compare_exchange_strong(paste, Ready);
        } else if (remote && (current != local || explicitPaste)) {
            std::string encoded;
            std::vector<win_clipboard::detail::source_file> largeFiles;
            int status = win_clipboard::read(m_Text, m_Files, explicitPaste ? std::nullopt : std::optional<DWORD>(local),
                                             encoded, copied, explicitPaste ? &largeFiles : nullptr);
            if (!largeFiles.empty() && copied == paste) {
                // Each chunk has its own deadline. A progressing multi-gigabyte batch can take minutes.
                m_BatchSequence = copied;
                const auto generation = m_CancelGeneration.load();
                const auto cancelled = [&] {
                    return isInterruptionRequested() || m_CancelGeneration != generation ||
                           m_PasteSequence != copied || GetClipboardSequenceNumber() != copied;
                };
                const auto result = uploadFileBatch(network, request, m_Host, m_Certificate, largeFiles, *remote, cancelled,
                                                    [this, generation](const ClipboardUploadProgress& progress) {
                                                        std::lock_guard lock(m_UploadMutex);
                                                        if (m_CancelGeneration == generation) m_Upload = progress;
                                                    });
                local = copied;
                if (result.status == 204 && result.sequence) {
                    remote = result.sequence;
                    m_PasteActivity = monotonicMs();
                    m_ConfirmedSequence = copied;
                    if (m_CancelGeneration == generation) m_PasteSequence.compare_exchange_strong(paste, Ready);
                } else if (m_CancelGeneration == generation) {
                    m_PasteSequence.compare_exchange_strong(paste, NoPaste);
                }
                m_BatchSequence = NoPaste;
                continue;
            }
            if (status == 200 && (!explicitPaste || (copied == paste && m_PasteSequence == paste))) {
                upload = QByteArray(encoded.data(), static_cast<int>(encoded.size()));
                // A remote copy between the last poll and Ctrl+V must win over an older local clipboard.
                if (explicitPaste) query.addQueryItem("since", QString::number(*remote));
            } else if (status == 204 || status == 413) {
                local = copied;
                if (explicitPaste) {
                    qWarning() << "Cannot transfer local clipboard for paste, status" << status;
                    m_PasteSequence.compare_exchange_strong(paste, NoPaste);
                }
            }
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
            if (!upload.isEmpty()) {
                local = copied;
                m_ConfirmedSequence = copied;
                uint64_t awaiting = copied;
                m_PasteSequence.compare_exchange_strong(awaiting, Ready);
            }
            failures = 0;
        } else if (response.status == 200 && remote && response.sequence && upload.isEmpty()) {
            const std::string_view payload(response.payload.constData(), response.payload.size());
            DWORD applied = local;
            int status = win_clipboard::write(m_Text, m_Files, payload, applied, stagingRoot, local);
            if (status == 204) {
                // If a paste was waiting on the replaced revision, paste the host's newer copy.
                uint64_t awaiting = local;
                local = applied;
                m_ConfirmedSequence = applied;
                m_PasteSequence.compare_exchange_strong(awaiting, Ready);
            }
            if (status != 409) remote = response.sequence;
            if (status != 204 && status != 409) qWarning() << "Cannot apply incoming clipboard data, status" << status;
            failures = 0;
        } else if (response.status == 400 || response.status == 413) {
            if (!upload.isEmpty()) local = copied;
            if (explicitPaste) m_PasteSequence.compare_exchange_strong(paste, NoPaste);
            if (response.sequence) remote = response.sequence;
            failures = 0;
        } else if (explicitPaste && !upload.isEmpty() && response.status == 409 &&
                   response.sequence && *response.sequence != 0 && response.sequence != remote) {
            // The guarded POST left the newer host clipboard intact.
            local = copied;
            m_PasteSequence.compare_exchange_strong(paste, Ready);
            failures = 0;
        } else {
            failures = (std::min)(failures + 1, 4);
        }
        const int delay = 250 << failures;
        for (int ms = 0; ms < delay && !isInterruptionRequested(); ms += 50) {
            if (m_PasteSequence <= UINT32_MAX && !explicitPaste && !failures) break;
            msleep(50);
        }
    }
}
