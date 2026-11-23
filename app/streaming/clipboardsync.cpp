#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <mstcpip.h>

#include "clipboardsync.h"
#include "filetransferprocess.h"
#include "settings/streamingpreferences.h"
#include <QLocalSocket>
#include "clipboarddownload.h"
#include "clipboardvirtualfiles.h"
#include "clipboard_tree.h"
#include "explorerpaste.h"
#include "desktoptransferprogress.h"
#include "clipboarduploadrate.h"
#include "clipboard_win32.h"
#include "transferfilehash.h"
#include "backend/identitymanager.h"

#include <Limelight.h>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstring>
#include <functional>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSettings>
#include <QScopedValueRollback>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QThreadPool>
#include <QUrlQuery>
#include <QUuid>
#include <thread>
#include <deque>
#include <mutex>

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
    QByteArray task;
};

QNetworkReply* startExchange(QNetworkAccessManager& network, const QNetworkRequest& request,
                             const QSslCertificate& pairedHost, const QByteArray* upload)
{
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
    return reply;
}

Result collectReply(QNetworkReply* reply, const std::function<bool()>& cancelled, int timeoutMs, size_t maxResponse)
{
    QEventLoop loop;
    Result result;
    auto receive = [&] {
        if (reply->bytesAvailable() > static_cast<qint64>(maxResponse) - result.payload.size()) {
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
    if (timeoutMs > 0) QTimer::singleShot(timeoutMs, reply, &QNetworkReply::abort);
    if (reply->bytesAvailable()) receive();
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
        result.task = reply->rawHeader("X-Clipboard-Task");
        if (status == 409) {
            const auto cause = reply->rawHeader("X-Clipboard-Conflict");
            if (cause == "busy" || cause == "sequence" || cause == "offset" || cause == "desktop" || cause == "shell" || cause == "file") {
                result.conflict = cause;
            }
        }
    }
    delete reply;
    return result;
}

Result exchange(QNetworkAccessManager& network, const QNetworkRequest& request,
                const QSslCertificate& pairedHost, const QByteArray* upload,
                const std::function<bool()>& cancelled = {}, int timeoutMs = 120000, size_t maxResponse = win_clipboard::max_body)
{
    return collectReply(startExchange(network, request, pairedHost, upload), cancelled, timeoutMs, maxResponse);
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

QNetworkRequest chunkRequest(const QNetworkRequest& baseRequest, DWORD revision, const char* op, const QString& lease, int file, quint64 offset) {
    QUrlQuery query;
    query.addQueryItem("download", op);
    query.addQueryItem("lease", lease);
    query.addQueryItem("files", "1");
    query.addQueryItem("since", QString::number(revision));
    const bool chunk = qstrcmp(op, "chunk") == 0;
    if (chunk) {
        query.addQueryItem("file", QString::number(file));
        query.addQueryItem("offset", QString::number(offset));
    }
    auto endpoint = baseRequest.url();
    endpoint.setQuery(query);
    auto req = baseRequest;
    req.setUrl(endpoint);
    return req;
}


int receiveSnapshot(QNetworkAccessManager& network, const QNetworkRequest& baseRequest,
                    const QSslCertificate& certificate, const QByteArray& manifest, DWORD revision,
                    const std::filesystem::path& destination, DWORD local,
                    const std::function<bool()>& cancelled, const std::function<bool(const ClipboardUploadProgress&)>& report,
                    ClipboardVirtualFiles::Paths* paths, bool saveToFolder)
{
    // Keep a small window of independent range reads in flight. The receiver
    // consumes them in offset order and validates each chunk before writing.
    std::deque<std::pair<quint64, QNetworkReply*>> pending;
    size_t window = 4;
    const auto discardPending = [&] {
        for (const auto& entry : pending) {
            entry.second->abort();
            delete entry.second;
        }
        pending.clear();
    };
    const auto fetch = [&](const char* op, const QString& lease, int file, quint64 offset, quint64 size) {
        using clipboard_download::ChunkBytes;
        if (qstrcmp(op, "finish") == 0) {
            discardPending();
            return exchange(network, chunkRequest(baseRequest, revision, op, lease, file, offset), certificate, nullptr, {}, 3000);
        }
        auto start = [&](quint64 pos) {
            auto reply = startExchange(network, chunkRequest(baseRequest, revision, op, lease, file, pos), certificate, nullptr);
            reply->setReadBufferSize(ChunkBytes);
            return reply;
        };
        auto next = pending.empty() ? offset : pending.back().first + ChunkBytes;
        while (pending.size() < window && next < size) {
            pending.emplace_back(next, start(next));
            next += ChunkBytes;
        }
        auto reply = pending.front().second;
        pending.pop_front();
        for (int retry = 0; ; ++retry) {
            auto result = collectReply(reply, cancelled, 30000, ChunkBytes);
            const bool transient = !result.status || result.status == 408 || result.status == 429 ||
                                   result.status == 502 || result.status == 503 || result.status == 504;
            if (!transient || cancelled() || retry == 2) {
                if (result.status != 200 && !cancelled())
                    qWarning() << "Clipboard download chunk failed, HTTP" << result.status
                               << "network error" << result.networkError << "file" << file << "offset" << offset;
                return result;
            }
            // Some hosts or proxies cannot serve overlapping reads reliably.
            // Retry only the uncommitted offset, then keep this batch serial.
            window = 1;
            discardPending();
            if (!waitForUpload(monotonicMs() + (100 << retry), cancelled)) return result;
            reply = start(offset);
        }
    };
    DWORD unused = 0;
    return clipboard_download::receive(manifest, destination, revision, local, unused, fetch,
                                       cancelled, report, paths, saveToFolder, [&](int itemCount, const QList<FileConflict>& conflicts) {
        return ClipboardTransferProcess::askConflicts(false, itemCount, conflicts, cancelled);
    });
}

quint64 directEthernetSpeed(const QUrl& host)
{
    addrinfo hints{};
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* address = nullptr;
    if (getaddrinfo(host.host().toUtf8().constData(), nullptr, &hints, &address) != 0) return 0;
    SOCKADDR_INET peer{};
    if (address->ai_addrlen <= sizeof(peer)) std::memcpy(&peer, address->ai_addr, address->ai_addrlen);
    freeaddrinfo(address);

    MIB_IPFORWARD_ROW2 route{};
    SOCKADDR_INET source{};
    if (GetBestRoute2(nullptr, 0, nullptr, &peer, 0, &route, &source) != NO_ERROR ||
        !INETADDR_ISANY(reinterpret_cast<const SOCKADDR*>(&route.NextHop))) return 0;
    MIB_IF_ROW2 nic{};
    nic.InterfaceLuid = route.InterfaceLuid;
    if (GetIfEntry2(&nic) != NO_ERROR || nic.Type != IF_TYPE_ETHERNET_CSMACD ||
        !nic.InterfaceAndOperStatusFlags.HardwareInterface || nic.OperStatus != IfOperStatusUp ||
        nic.PhysicalMediumType != NdisPhysicalMedium802_3 ||
        nic.TransmitLinkSpeed == UINT64_MAX) return 0;
    return nic.TransmitLinkSpeed;
}

// One request in flight; allow at most a short burst, including the first request after a stalled ACK.
class ClipboardUploadPacer
{
public:
    explicit ClipboardUploadPacer(const QUrl& host)
        : m_Rate(directEthernetSpeed(host))
    {
        sampleRtt();
    }

    DWORD nextSize(quint64 remaining)
    {
        const auto now = monotonicMs();
        if (now - m_LastSample >= 250) {
            m_LastSample = now;
            sampleRtt();
        }
        return static_cast<DWORD>((std::min<quint64>)(remaining, m_Rate.chunkBytes()));
    }

    bool wait(DWORD bytes, const std::function<bool()>& cancelled)
    {
        const int rate = m_Rate.bytesPerSecond();
        if (rate) {
            const auto due = m_LastSend + (static_cast<qint64>(bytes) * 1000 + rate - 1) / rate;
            if (!waitForUpload(due, cancelled)) return false;
        } else if (cancelled()) return false;
        m_LastSend = monotonicMs();
        return true;
    }

    void acknowledge(DWORD bytes)
    {
        m_Rate.acknowledge(static_cast<int>(bytes), monotonicMs() - m_LastSend);
    }

private:
    void sampleRtt()
    {
        uint32_t rtt = 0, variance = 0;
        if (LiGetEstimatedRttInfo(&rtt, &variance)) m_Rate.sample(rtt, variance);
        else m_Rate.sample(0, 0);
    }

    ClipboardUploadRate m_Rate;
    qint64 m_LastSend = monotonicMs();
    qint64 m_LastSample = m_LastSend;
};

// Only a random generation is persisted. An offline Cancel must still make the next
// authenticated paste start fresh; neither filenames nor clipboard contents are stored.
QString discardSetting(const QSslCertificate& host)
{
    return QStringLiteral("clipboardUploadDiscard/%1/").arg(QString::fromLatin1(host.digest(QCryptographicHash::Sha256).toHex()));
}

bool fingerprintSources(const std::vector<win_clipboard::detail::source_file>& files, QJsonArray& manifest,
                        const std::function<bool()>& cancelled)
{
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& file = files[i];
        if (file.directory) continue;
        if (!transfer_file::seek(file.handle.get(), 0)) return false;
        const auto digest = transfer_file::sha256(file.handle.get(), file.size, cancelled);
        if (digest.isEmpty()) return false;
        auto item = manifest.at(i).toObject();
        item.insert(QStringLiteral("sha256"), QString::fromLatin1(digest.toHex()));
        manifest[i] = item;
    }
    return true;
}

bool readResumeResponse(const QByteArray& body, const std::vector<win_clipboard::detail::source_file>& files,
                        QByteArray& id, std::vector<quint64>& offsets, std::vector<bool>& skipped)
{
    const auto response = QJsonDocument::fromJson(body).object();
    id = response.value(QStringLiteral("id")).toString().toLatin1();
    const auto values = response.value(QStringLiteral("offsets")).toArray();
    if (size_t(values.size()) != files.size()) return false;
    for (size_t i = 0; i < files.size(); ++i) {
        const auto value = values.at(i);
        const auto offset = value.toInteger(-1);
        if (!value.isDouble() || offset < 0 || quint64(offset) > files[i].size) return false;
        offsets[i] = quint64(offset);
    }
    for (const auto index : response.value("skipped").toArray()) {
        const int i = index.toInt(-1);
        if (i < 0 || size_t(i) >= files.size() || offsets[i] != files[i].size) return false;
        skipped[i] = true;
    }
    return true;
}

std::optional<QJsonArray> resolveRemoteConflicts(const QByteArray& payload, const QJsonArray& manifest,
                                                const std::function<bool()>& cancelled)
{
    const auto response = QJsonDocument::fromJson(payload).object();
    // Earlier hosts and collisions discovered during delivery report one file.
    const auto entries = response.contains("conflicts") ? response.value("conflicts").toArray() : QJsonArray{response};
    if (entries.isEmpty() || entries.size() > manifest.size()) return {};
    QList<FileConflict> files;
    QSet<int> seen;
    QString folder;
    for (const auto value : entries) {
        const auto entry = value.toObject();
        const int index = entry.value("file").toInt(-1);
        const auto path = entry.value("path").toString();
        const QFileInfo info(path);
        if (index < 0 || index >= manifest.size() || seen.contains(index) || !info.isAbsolute()) return {};
        const auto suffix = '/' + manifest.at(index).toObject().value("name").toString();
        const auto fullPath = QDir::fromNativeSeparators(path);
        if (!fullPath.endsWith(suffix, Qt::CaseInsensitive)) return {};
        const auto root = fullPath.chopped(suffix.size());
        if (!folder.isEmpty() && root.compare(folder, Qt::CaseInsensitive) != 0) return {};
        folder = root;
        seen.insert(index);
        const auto reason = entry.value("reason").toString();
        QString unavailable;
        if (reason == "permission")
            unavailable = QCoreApplication::translate("ClipboardTransfer", "Enable “Browse and transfer files” and “Allow file deletion” in Sunshine to replace remote files.");
        else if (reason == "folder")
            unavailable = QCoreApplication::translate("FileTransferEngine", "A folder occupies this name.");
        else if (reason == "unavailable")
            unavailable = QCoreApplication::translate("FileTransferEngine", "The file changed or is in use: %1").arg(path);
        else if (!reason.isEmpty() || entry.value("version").toString().isEmpty()) return {};
        files.append({path, unavailable});
    }
    const auto choices = ClipboardTransferProcess::askConflicts(true, int(manifest.size()), files, cancelled);
    if (!choices || choices->size() != entries.size()) return {};
    QJsonArray decisions;
    for (int i = 0; i < entries.size(); ++i) {
        const auto entry = entries.at(i).toObject();
        QString resolution;
        switch (choices->at(i)) {
        case FileConflictChoice::Skip: resolution = QStringLiteral("skip"); break;
        case FileConflictChoice::KeepBoth: resolution = QStringLiteral("keep-both"); break;
        case FileConflictChoice::Replace:
            if (!files[i].replaceUnavailable.isEmpty()) return {};
            resolution = entry.value("version").toString();
            break;
        }
        decisions.append(QJsonObject{{"file", entry.value("file")}, {"resolution", resolution}});
    }
    return decisions;
}

Result uploadFileBatch(QNetworkAccessManager& network, QNetworkRequest request, const QUrl& host,
                       const QSslCertificate& pairedHost, const std::vector<win_clipboard::detail::source_file>& files,
                       DWORD expectedHostSequence, const std::function<bool()>& cancelled,
                       const std::function<void(const ClipboardUploadProgress&)>& update,
                       bool resumable = false, const std::function<bool()>& discardRequested = {},
                       const QString& task = {}, const std::function<bool()>& allowCommit = {},
                       const QString& sourceKey = {}, QJsonArray* cachedManifest = nullptr)
{
    quint64 acknowledged = 0;
    size_t index = 0;
    quint64 offset = 0;
    auto failure = [&](const char* operation, Result result, const char* reason = "response") {
        qWarning() << "Clipboard file upload failed" << operation << reason << "HTTP" << result.status
                   << "network error" << result.networkError << "conflict" << result.conflict
                   << "file index" << index << "file offset" << offset << "acknowledged bytes" << acknowledged;
        // Only a valid commit response may release the pending paste.
        result.sequence.reset();
        if (result.status == 204) result.status = 502;
        return result;
    };
    QJsonArray manifest;
    ClipboardUploadProgress progress;
    quint64 total = 0;
    for (const auto& file : files) {
        total += file.size;
        const auto name = QString::fromUtf8(file.name.data(), static_cast<int>(file.name.size()));
        progress.names.append(name);
        manifest.append(QJsonObject{{"name", name},
                                    {"size", static_cast<qint64>(file.size)}});
    }
    progress.startedAtMs = monotonicMs();
    progress.preparing = resumable;
    progress.total = total;
    progress.fileSize = files.front().size;
    update(progress);

    if (resumable) {
        if (cachedManifest && !cachedManifest->isEmpty()) manifest = *cachedManifest;
        else {
            if (!fingerprintSources(files, manifest, cancelled))
                return failure("manifest", {}, cancelled() ? "cancelled" : "local file read");
            if (cachedManifest) *cachedManifest = manifest;
        }
    }

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
    if (cancelled()) return failure("begin", {}, "cancelled");
    // Once begin is on the wire, receive the host-generated ID so a cancellation can
    // clean up the staging files. The stream's stop request still interrupts this wait.
    const auto manifestBody = QJsonDocument(manifest).toJson(QJsonDocument::Compact);
    if (resumable && task.isEmpty()) {
        QSettings settings;
        QStringList keys{discardSetting(pairedHost)};
        if (!sourceKey.isEmpty()) keys.append(keys.first() + sourceKey);
        for (const auto& key : keys) {
            const auto pending = settings.value(key + "requested").toByteArray();
            if (pending.isEmpty() || pending == settings.value(key + "done").toByteArray()) continue;
            auto reset = post(QUrlQuery("op=discard"), manifestBody);
            if (reset.status != 204) return failure("discard", reset);
            settings.setValue(key + "done", pending);
        }
    }
    if (cancelled()) return failure("begin", {}, "cancelled");
    QUrlQuery beginQuery(!task.isEmpty() ? "op=prepare" : resumable ? "op=resume" : "op=begin");
    if (!task.isEmpty()) beginQuery.addQueryItem("task", task);
    url.setQuery(beginQuery);
    request.setUrl(url);
    auto body = manifestBody;
    Result begin;
    for (;;) {
        begin = exchange(network, request, pairedHost, &body, {}, 15000);
        if (task.isEmpty() || begin.conflict != "file") break;
        const auto decisions = resolveRemoteConflicts(begin.payload, manifest, cancelled);
        if (!decisions || cancelled()) return failure("prepare", begin);
        for (const auto value : *decisions) {
            const auto decision = value.toObject();
            const int i = decision.value("file").toInt();
            auto item = manifest.at(i).toObject();
            item.insert("resolution", decision.value("resolution"));
            manifest[i] = item;
        }
        body = QJsonDocument(manifest).toJson(QJsonDocument::Compact);
    }
    if (begin.status != 200) return failure("begin", begin);
    QByteArray id = begin.payload;
    std::vector<quint64> offsets(files.size());
    std::vector<bool> skipped(files.size());
    if (resumable && !readResumeResponse(begin.payload, files, id, offsets, skipped)) {
        return failure("resume", begin, "invalid file offsets");
    }
    if (id.size() != 32 || !std::all_of(id.begin(), id.end(), [](char ch) {
            return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
        })) return failure("begin", begin, "invalid batch identifier");

    auto batchQuery = [&](const QString& op) {
        QUrlQuery query;
        query.addQueryItem("op", op);
        query.addQueryItem("id", QString::fromLatin1(id));
        if (!task.isEmpty()) query.addQueryItem("task", task);
        return query;
    };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    auto send = [&]() -> Result {
        if (cancelled()) return failure("begin", {}, "cancelled after acknowledgement");
        for (size_t i = files.size(); i-- > 0;) {
            if (skipped[i]) {
                progress.names.removeAt(i);
                progress.total -= files[i].size;
            } else acknowledged += offsets[i];
        }
        progress.resumedBytes = acknowledged;
        progress.acknowledged = acknowledged;
        progress.preparing = false;
        ClipboardUploadPacer pacer(host);
        QByteArray chunk;
        int progressIndex = 0;
        for (index = 0; index < files.size(); ++index) {
            if (skipped[index]) continue;
            const auto& file = files[index];
            offset = offsets[index];
            if (resumable && !file.directory && !transfer_file::seek(file.handle.get(), offset)) return failure("resume", {}, "local file seek");
            progress.index = progressIndex++;
            progress.fileBytes = offset;
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
                // Qt may resend a POST if the connection closes after the host wrote the
                // chunk but before its ACK. Only accept that exact end offset on this lease.
                const bool alreadyWritten = resumable && result.status == 409 && result.conflict == "offset" &&
                                            result.nextOffset == offset + length;
                if (result.status != 204 && !alreadyWritten) return failure("chunk", result);
                if (result.nextOffset != offset + length) return failure("chunk", result, "unexpected acknowledgement offset");
                pacer.acknowledge(length);
                offset += length;
                acknowledged += length;
                progress.fileBytes = offset;
                progress.acknowledged = acknowledged;
                update(progress);
            }
        }
        if (allowCommit && !allowCommit()) return failure("commit", {}, "cancelled before publication");
        auto query = batchQuery(task.isEmpty() ? "commit" : "deliver");
        if (task.isEmpty()) query.addQueryItem("since", QString::number(expectedHostSequence));
        progress.committing = true;
        update(progress);
        // Keep the original revision guard on every attempt. Older hosts return an ambiguous
        // 409, and a newer remote clipboard must always win.
        QByteArray deliveryDecision;
        int busyRetries = 0;
        for (;;) {
            auto result = post(query, deliveryDecision);
            if (result.status == 204 && (!task.isEmpty() || result.sequence)) return result;
            if (!task.isEmpty() && result.conflict == "file") {
                const auto decisions = resolveRemoteConflicts(result.payload, manifest, cancelled);
                if (!decisions || decisions->size() != 1 || cancelled()) return failure("deliver", result);
                deliveryDecision = QJsonDocument(decisions->first().toObject()).toJson(QJsonDocument::Compact);
                busyRetries = 0;
                continue;
            }
            if (result.status != 409 || result.conflict != "busy" || busyRetries >= 5) return failure("commit", result);
            if (!waitForUpload(monotonicMs() + (100 << busyRetries), cancelled)) return failure("commit", {}, "cancelled");
            ++busyRetries;
        }
    };
    auto result = send();
    const bool discard = !resumable || (discardRequested && discardRequested());
    if (discard && (result.status != 204 || (task.isEmpty() && !result.sequence))) {
        auto cleanup = post(batchQuery("abort"), {}, true);
        if (cleanup.status != 204 && cleanup.status != 404) failure("abort", cleanup);
    }
    return result;
}
}

struct ClipboardSync::FileQueue {
    using State = ClipboardUploadProgress::State;
    static constexpr int Binding = -1;
    static constexpr int Uncertain = -2;
    struct Job {
        std::vector<win_clipboard::detail::source_file> files;
        ClipboardUploadProgress progress;
        DWORD sequence = 0;
        QString sourceKey;
        QString token;
        QString previous;
        QJsonArray manifest;
        std::atomic_bool cancelled{false};
        // Only an explicit 422 authorizes the ordinary clipboard fallback.
        std::atomic_int targetStatus{Binding};
        bool automaticPaste = true;
        bool published = false;
        bool dispatched = false;
        bool uploadStarted = false;
        DWORD publishedSequence = 0;
        qint64 nextClipboardCheck = 0;
    };

    mutable std::mutex mutex;
    std::deque<std::shared_ptr<Job>> jobs;
    ClipboardUploadProgress download;
    std::atomic_bool downloadCancelled{false};
    qint64 readyUntil = 0;
    DWORD readySequence = 0;
    QThreadPool controls;
    QNetworkRequest request;
    quint64 nextId = 0;
    quint64 inputGeneration = 0;
    std::atomic_bool stopping{false};
    QSet<QString> selections;

    static QString message(const char* english) {
        return QCoreApplication::translate("ClipboardTransfer", english);
    }

    auto firstPending() const {
        return std::find_if(jobs.begin(), jobs.end(), [](const auto& j) { return j->progress.state != State::Failed; });
    }

    auto publishedJob() const {
        return std::find_if(jobs.begin(), jobs.end(), [](const auto& j) { return j->published; });
    }

    static QString sourceKey(const Job& job) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        for (const auto& file : job.files) {
            BY_HANDLE_FILE_INFORMATION info{};
            if (!file.directory && !GetFileInformationByHandle(file.handle.get(), &info)) return {};
            const DWORD fields[] = {info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow,
                info.ftLastWriteTime.dwHighDateTime, info.ftLastWriteTime.dwLowDateTime, info.nFileSizeHigh,
                info.nFileSizeLow, static_cast<DWORD>(file.name.size())};
            hash.addData(QByteArrayView(reinterpret_cast<const char*>(fields), sizeof(fields)));
            hash.addData(QByteArrayView(file.name.data(), static_cast<qsizetype>(file.name.size())));
        }
        return QString::fromLatin1(hash.result().toHex()) + "/";
    }

    static void forget(const QSslCertificate& cert, const Job& job) {
        if (job.token.isEmpty() || job.sourceKey.isEmpty()) return;
        QSettings settings;
        const auto key = discardSetting(cert) + job.sourceKey + "task";
        if (settings.value(key).toString() == job.token) settings.remove(key);
    }

    void control(ClipboardSync* sync, const std::shared_ptr<Job>& job, bool discard) {
        const auto deadline = monotonicMs() + 3000;
        const auto generation = inputGeneration;
        const auto token = job->token;
        const auto previous = job->previous;
        controls.start([this, sync, job, discard, deadline, generation, token, previous] {
            if (stopping || (!discard && job->cancelled)) return;
            if (!discard) {
                std::lock_guard lock(mutex);
                if (generation != inputGeneration) {
                    job->targetStatus = Uncertain;
                    job->progress.state = State::NeedsPaste;
                    return;
                }
            }
            auto endpoint = sync->m_Host;
            endpoint.setPath("/clipboard/batch");
            QUrlQuery query;
            query.addQueryItem("op", discard ? "discard" : "target");
            query.addQueryItem("task", token);
            if (!discard && !previous.isEmpty()) query.addQueryItem("previous", previous);
            endpoint.setQuery(query);
            auto req = request;
            req.setUrl(endpoint);
            QNetworkAccessManager net;
            net.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
            const QByteArray empty;
            const int timeout = discard ? 3000 : int((std::max<qint64>)(0, deadline - monotonicMs()));
            const auto result = timeout ? exchange(net, req, sync->m_Certificate, &empty,
                [this] { return stopping.load(); }, timeout) : Result{};
            if (discard) return;
            std::lock_guard lock(mutex);
            int status = result.status;
            if (status == 204) {
                const auto chosen = QString::fromLatin1(result.task);
                if (chosen == token || (!previous.isEmpty() && chosen == previous)) job->token = chosen;
                else if (!chosen.isEmpty() || !previous.isEmpty()) status = 502;
                if (job->cancelled) {
                    control(sync, job, true);
                    return;
                }
                if (!job->sourceKey.isEmpty() && status == 204) {
                    QSettings settings;
                    const auto key = discardSetting(sync->m_Certificate) + job->sourceKey + "task";
                    if (settings.value(key).toString() == token) settings.setValue(key, job->token);
                }
            }
            if (generation != inputGeneration && (status == 204 || status == 422)) {
                if (status == 204 && job->token != previous) control(sync, job, true);
                job->targetStatus = Uncertain;
                job->progress.state = State::NeedsPaste;
            } else job->targetStatus = status;
        });
    }

    // One byte stream at a time. Park a task that needs user input so another
    // task with a confirmed Explorer destination can use the connection.
    bool process(ClipboardSync& sync, QNetworkAccessManager& net, DWORD& local, std::optional<DWORD>& remote) {
        std::shared_ptr<Job> job;
        {
            std::lock_guard lock(mutex);
            const bool occupied = publishedJob() != jobs.end();
            for (const auto& next : jobs) {
                if (next->targetStatus == Binding && next->progress.state != State::Failed) break;
                if (next->progress.state == State::Failed || next->targetStatus == Uncertain) continue;
                if (next->published) {
                    if (next->dispatched || monotonicMs() < next->nextClipboardCheck) continue;
                } else if (next->targetStatus == 422) {
                    if (occupied) continue;
                    if (!next->automaticPaste && sync.m_Multitask) {
                        next->progress.state = State::NeedsPaste;
                        continue;
                    }
                    if (!remote) return false;
                }
                job = next;
                if (!job->published) {
                    job->progress.state = State::Uploading;
                    job->uploadStarted = true;
                }
                break;
            }
            // Unpublished files waiting for a new paste grant leave the host
            // clipboard available to the ordinary text synchronizer.
            if (!job) return occupied || std::any_of(jobs.begin(), jobs.end(), [](const auto& j) {
                return j->targetStatus == Binding && j->progress.state != State::Failed;
            });
        }
        const auto interrupted = [&] { return sync.isInterruptionRequested() || job->cancelled.load(); };
        if (job->published) {
            auto endpoint = sync.m_Host;
            // 'init' identifies the stream-start revision. Ask for the current
            // revision without fetching the contents of the user's new copy.
            endpoint.setQuery(QStringLiteral("since=%1&files=0&text=0").arg(job->publishedSequence));
            auto req = request;
            req.setUrl(endpoint);
            const auto result = exchange(net, req, sync.m_Certificate, nullptr, interrupted, 3000);
            std::lock_guard lock(mutex);
            job->nextClipboardCheck = monotonicMs() + 250;
            if (job->cancelled || job->dispatched) return true;
            if (result.status == 204 && result.sequence) {
                if (result.sequence != job->publishedSequence) {
                    job->published = false;
                    job->progress.error = message("Remote clipboard changed");
                    job->progress.state = State::Failed;
                    // Leave the new revision available to the ordinary clipboard GET.
                    return false;
                }
                if (job->progress.state == State::Queued) {
                    job->progress.state = job->automaticPaste ? State::WaitingForPaste : State::NeedsPaste;
                }
            } else if (result.status == 401 || result.status == 403 || result.status == 404) {
                job->published = false;
                job->progress.state = State::Failed;
                job->progress.error = QStringLiteral("HTTP %1").arg(result.status);
                return false;
            }
            return true;
        }
        bool parked = false;
        const auto allowPublication = [&] {
            while (!interrupted()) {
                {
                    std::lock_guard lock(mutex);
                    if (job->automaticPaste) return true;
                    job->progress.state = State::NeedsPaste;
                    if (sync.m_Multitask) {
                        parked = true;
                        return false;
                    }
                }
                QThread::msleep(25);
            }
            return false;
        };
        const bool direct = job->targetStatus == 204;
        Result result;
        if (direct || job->targetStatus == 422) {
            std::string encoded;
            const int legacy = direct || sync.m_Resumable ? 413 : win_clipboard::encode_files(job->files, encoded);
            if (legacy == 413) {
                const auto update = [&](const ClipboardUploadProgress& p) {
                    std::lock_guard lock(mutex);
                    const auto id = job->progress.taskId;
                    job->progress = p;
                    job->progress.taskId = id;
                };
                result = uploadFileBatch(net, request, sync.m_Host, sync.m_Certificate, job->files, remote.value_or(0),
                    interrupted, update, direct || sync.m_Resumable, [&] { return job->cancelled.load(); },
                    direct ? job->token : QString(), direct ? std::function<bool()>{} : allowPublication,
                    job->sourceKey, &job->manifest);
            } else if (legacy == 200 && allowPublication()) {
                auto endpoint = sync.m_Host;
                QUrlQuery query;
                query.addQueryItem("files", "1");
                query.addQueryItem("text", sync.m_Text ? "1" : "0");
                query.addQueryItem("since", QString::number(*remote));
                endpoint.setQuery(query);
                auto req = request;
                req.setUrl(endpoint);
                const auto payload = QByteArray::fromRawData(encoded.data(), static_cast<qsizetype>(encoded.size()));
                result = exchange(net, req, sync.m_Certificate, &payload, interrupted);
            } else result.status = legacy;
        } else result.status = job->targetStatus;

        std::lock_guard lock(mutex);
        // Do not auto-publish this selection after finishing a direct transfer.
        local = job->sequence;
        if (interrupted() || parked) return true;
        job->files.clear();
        if (result.status == 204 && (direct || result.sequence)) {
            if (direct) {
                forget(sync.m_Certificate, *job);
                jobs.erase(std::find(jobs.begin(), jobs.end(), job));
            } else {
                remote = result.sequence;
                sync.m_ConfirmedSequence = local;
                job->published = true;
                job->publishedSequence = *result.sequence;
                job->progress.acknowledged = job->progress.total;
                job->progress.state = job->automaticPaste ? State::WaitingForPaste : State::NeedsPaste;
            }
        } else {
            job->progress.state = State::Failed;
            job->progress.error = result.status ? QStringLiteral("HTTP %1").arg(result.status) :
                                                  message("Connection or source file unavailable");
        }
        return true;
    }
};

ClipboardSync::ClipboardSync(QUrl host, QSslCertificate certificate, QByteArray token, bool text, bool files,
                             bool resumable, bool multitask, bool images)
    : m_FileQueue(std::make_unique<FileQueue>()), m_Host(std::move(host)), m_Certificate(std::move(certificate)),
      m_Token(std::move(token)), m_Text(text), m_Images(images), m_Files(files), m_Resumable(files && resumable),
      m_Multitask(m_Resumable && multitask), m_LocalSequence(GetClipboardSequenceNumber())
{
    m_Host.setPath("/clipboard");
    m_FileQueue->controls.setMaxThreadCount(8);
    if (!m_Token.isEmpty() && !m_Certificate.isNull() && (m_Text || m_Images || m_Files)) {
        auto config = IdentityManager::get()->getSslConfig();
        config.setCaCertificates({m_Certificate});
        auto& request = m_FileQueue->request;
        request.setSslConfiguration(config);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setRawHeader("X-Clipboard-Token", m_Token);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
        start();
    }
}

ClipboardSync::~ClipboardSync()
{
    stop();
}

void ClipboardSync::stop()
{
    m_FileQueue->stopping = true;
    if (isRunning()) {
        requestInterruption();
        wait();
    }
    m_FileQueue->controls.waitForDone();
}

bool ClipboardSync::requestPaste()
{
    if (!isRunning()) return false;
    const DWORD sequence = GetClipboardSequenceNumber();
    const bool hasFiles = IsClipboardFormatAvailable(CF_HDROP);
    if (!(hasFiles ? m_Files : (m_Images && clipboard_image::available()) ||
                               (m_Text && IsClipboardFormatAvailable(CF_UNICODETEXT)))) return false;
    if (hasFiles) {
        if (m_IncomingSequence == sequence) return false;
        m_PasteSequence = NoPaste;
        m_PasteCount = 0;
        auto job = std::make_shared<FileQueue::Job>();
        auto& queue = *m_FileQueue;
        std::lock_guard lock(queue.mutex);
        job->progress.taskId = ++queue.nextId;
        if (queue.jobs.size() >= 32) {
            if (queue.jobs.size() == 32) {
                job->progress.state = FileQueue::State::Failed;
                job->progress.error = FileQueue::message("Transfer queue is full");
                queue.jobs.push_back(job);
            }
            return true;
        }
        std::string unused;
        std::vector<std::filesystem::path> paths;
        auto status = win_clipboard::read(false, true, {}, unused, job->sequence, &paths);
        if (status == 200) {
            status = win_clipboard::tree::walk(paths, true, false, [&](const auto&, const std::wstring& name,
                                               const BY_HANDLE_FILE_INFORMATION& info, auto& handle) {
                const bool directory = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                const auto size = directory ? 0 : (quint64(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
                job->files.push_back({directory ? nullptr : std::move(handle), win_clipboard::detail::to_utf8(name), size, directory});
            });
        }
        if (status != 200 || job->files.empty() || job->sequence != sequence || GetClipboardSequenceNumber() != sequence) {
            job->files.clear();
            job->progress.state = FileQueue::State::Failed;
            job->progress.error = status == 413 ? FileQueue::message("This selection is too large for the clipboard. Use the file transfer assistant.") :
                                                  FileQueue::message("Cannot open the selected files");
            queue.jobs.push_back(job);
            return true;
        }
        for (const auto& file : job->files) {
            if (file.size > INT64_MAX || file.size > UINT64_MAX - job->progress.total) {
                job->progress.state = FileQueue::State::Failed;
                job->progress.error = FileQueue::message("Cannot represent the file selection size");
                job->files.clear();
                queue.jobs.push_back(job);
                return true;
            }
            job->progress.names.append(QString::fromStdString(file.name));
            job->progress.total += file.size;
        }
        job->sourceKey = FileQueue::sourceKey(*job);
        job->progress.state = FileQueue::State::Queued;
        job->automaticPaste = queue.firstPending() == queue.jobs.end();
        if (m_Multitask) {
            QSettings settings;
            const auto key = discardSetting(m_Certificate) + job->sourceKey + "task";
            if (!job->sourceKey.isEmpty() && !queue.selections.contains(job->sourceKey)) {
                const auto candidate = settings.value(key).toByteArray();
                if (candidate.size() == 32 && QByteArray::fromHex(candidate).toHex() == candidate)
                    job->previous = QString::fromLatin1(candidate);
                queue.selections.insert(job->sourceKey);
            }
            job->token = QString::fromLatin1(QUuid::createUuid().toRfc4122().toHex());
            if (!job->sourceKey.isEmpty()) settings.setValue(key, job->token);
            queue.control(this, job, false);
        } else job->targetStatus = 422;
        queue.jobs.push_back(job);
        return true;
    }

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

void ClipboardSync::cancelFileTask(quint64 taskId)
{
    auto& queue = *m_FileQueue;
    std::lock_guard lock(queue.mutex);
    if (taskId && taskId == queue.download.taskId) {
        if (queue.download.state == FileQueue::State::Uploading) {
            if (queue.download.committing) return;
            queue.downloadCancelled = true;
        }
        queue.download = {};
        return;
    }
    const auto it = std::find_if(queue.jobs.begin(), queue.jobs.end(), [&](const auto& j) { return j->progress.taskId == taskId; });
    if (it == queue.jobs.end()) return;
    const auto job = *it;
    job->cancelled = true;
    FileQueue::forget(m_Certificate, *job);
    if (!job->token.isEmpty()) queue.control(this, job, true);
    if (m_Resumable && job->uploadStarted && job->targetStatus == 422 && !job->sourceKey.isEmpty()) {
        QSettings settings;
        settings.setValue(discardSetting(m_Certificate) + job->sourceKey + "requested", QUuid::createUuid().toRfc4122());
    }
    queue.jobs.erase(it);
}

bool ClipboardSync::pasteFileTask(quint64 taskId)
{
    auto& queue = *m_FileQueue;
    std::lock_guard lock(queue.mutex);
    const auto it = std::find_if(queue.jobs.begin(), queue.jobs.end(), [&](const auto& j) { return j->progress.taskId == taskId; });
    if (it == queue.jobs.end() || (*it)->progress.state != FileQueue::State::NeedsPaste) return false;
    const auto& job = *it;
    if (job->targetStatus == FileQueue::Uncertain) {
        FileQueue::forget(m_Certificate, *job);
        job->token = QString::fromLatin1(QUuid::createUuid().toRfc4122().toHex());
        job->targetStatus = FileQueue::Binding;
        if (!job->sourceKey.isEmpty()) {
            QSettings settings;
            settings.setValue(discardSetting(m_Certificate) + job->sourceKey + "task", job->token);
        }
        queue.control(this, job, false);
    }
    job->automaticPaste = true;
    job->progress.state = FileQueue::State::Queued;
    return true;
}

void ClipboardSync::cancelPaste(bool discardUpload)
{
    m_PasteCount = 0;
    m_PasteSequence = NoPaste;
    quint64 taskId = 0;
    {
        std::lock_guard lock(m_FileQueue->mutex);
        ++m_FileQueue->inputGeneration;
        for (const auto& job : m_FileQueue->jobs) {
            if (discardUpload) {
                taskId = job->progress.taskId;
                break;
            }
            job->automaticPaste = false;
            if (job->published && !job->dispatched) job->progress.state = FileQueue::State::NeedsPaste;
        }
    }
    if (taskId) cancelFileTask(taskId);
}

QList<ClipboardUploadProgress> ClipboardSync::fileUploadTasks() const
{
    std::lock_guard lock(m_FileQueue->mutex);
    QList<ClipboardUploadProgress> tasks;
    const auto& download = m_FileQueue->download;
    const bool finished = download.state == FileQueue::State::ReadyLocally || download.state == FileQueue::State::Saved;
    if ((!download.names.isEmpty() || download.skipped) && (!finished ||
        (monotonicMs() < m_FileQueue->readyUntil && GetClipboardSequenceNumber() == m_FileQueue->readySequence))) {
        tasks.append(download);
    }
    for (const auto& job : m_FileQueue->jobs) tasks.append(job->progress);
    return tasks;
}

ClipboardUploadProgress ClipboardSync::fileUploadProgress() const
{
    std::lock_guard lock(m_FileQueue->mutex);
    const auto it = m_FileQueue->firstPending();
    return it == m_FileQueue->jobs.end() ? ClipboardUploadProgress{} : (*it)->progress;
}

bool ClipboardSync::pastePending() const
{
    std::lock_guard lock(m_FileQueue->mutex);
    const auto it = m_FileQueue->firstPending();
    return m_PasteCount || (it != m_FileQueue->jobs.end() && !(*it)->dispatched);
}

int ClipboardSync::takeReadyPastes()
{
    {
        std::lock_guard lock(m_FileQueue->mutex);
        // Text uses the same authenticated worker. Time spent hashing or sending
        // an earlier file is not a timeout for a later text paste.
        if (m_PasteCount && m_FileQueue->firstPending() != m_FileQueue->jobs.end()) m_PasteActivity = monotonicMs();
        const auto it = m_FileQueue->publishedJob();
        if (it != m_FileQueue->jobs.end()) {
            const auto& job = *it;
            if (job->progress.state != FileQueue::State::WaitingForPaste || !job->automaticPaste || job->dispatched) return 0;
            job->dispatched = true;
            return 1;
        }
    }
    if (!m_PasteCount) return 0;
    const auto state = m_PasteSequence.load();
    if (state != NoPaste && state != Ready && isRunning() &&
        (state == m_SendingSequence || monotonicMs() - m_PasteActivity.load() < 15000)) return 0;
    const int count = state == Ready ? m_PasteCount : 0;
    m_PasteCount = 0;
    m_PasteSequence = NoPaste;
    return count;
}

void ClipboardSync::acknowledgePastesSent(bool delivered)
{
    std::lock_guard lock(m_FileQueue->mutex);
    const auto it = m_FileQueue->publishedJob();
    if (it == m_FileQueue->jobs.end() || !(*it)->dispatched) return;
    if (delivered) {
        // A newer local selection copied while the old host was waiting for the
        // paste key belongs to its own upcoming gesture.
        m_DeferredFileSequence = GetClipboardSequenceNumber();
        FileQueue::forget(m_Certificate, **it);
        m_FileQueue->jobs.erase(it);
    } else {
        (*it)->dispatched = false;
        (*it)->automaticPaste = false;
        (*it)->progress.state = FileQueue::State::NeedsPaste;
    }
}

void ClipboardSync::run()
{
    const auto stagingRoot = std::filesystem::path(QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation).toStdWString()) / L"Clipboard";
    QNetworkAccessManager network;
    network.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    auto request = m_FileQueue->request;
    // A large PNG can take minutes on a slow link. Time out stalled traffic.
    request.setTransferTimeout(120000);

    // Explorer can call the STA for metadata or stream reads between polls.
    const auto waitForEvents = [](int milliseconds) {
        QEventLoop loop;
        QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
        loop.exec();
    };

    DWORD local = m_LocalSequence;
    std::optional<DWORD> remote;
    const bool oleReady = SUCCEEDED(OleInitialize(nullptr));
    ClipboardVirtualFiles* offer = nullptr;
    explorer_paste::Shortcut shortcut;
    DesktopTransferProgress progressWindow;
    explorer_paste::Shortcut::Paste pasteToFolder;
    bool receiving = false;
    const auto clearOffer = [&] {
        shortcut.reset();
        pasteToFolder = {};
        if (offer) {
            offer->disconnect();
            offer->Release();
            offer = nullptr;
        }
    };
    bool refreshOffer = false;
    int failures = 0;
    while (!isInterruptionRequested()) {
        if (offer && OleIsCurrentClipboard(offer) != S_OK) clearOffer();
        if (offer && pasteToFolder) shortcut.update(local, pasteToFolder);
        if (m_FileQueue->process(*this, network, local, remote)) {
            waitForEvents(20);
            continue;
        }
        QUrl url = m_Host;
        QUrlQuery query;
        query.addQueryItem("text", m_Text ? "1" : "0");
        query.addQueryItem("images", m_Images ? "1" : "0");
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
            m_SendingSequence = current;
            std::string encoded;
            const bool syncFiles = m_Files && !m_Multitask && current != m_DeferredFileSequence;
            int status = win_clipboard::read(m_Text, syncFiles, explicitPaste ? std::nullopt : std::optional<DWORD>(local),
                                             encoded, copied, nullptr, m_Images);
            if (status == 200 && (!explicitPaste || (copied == paste && m_PasteSequence == paste))) {
                m_SendingSequence = copied;
                upload = QByteArray(encoded.data(), static_cast<int>(encoded.size()));
                // A copy on the host since our last poll must survive a concurrent local upload.
                query.addQueryItem("since", QString::number(*remote));
            } else if (status == 204 || status == 413) {
                local = copied;
                if (explicitPaste) {
                    qWarning() << "Cannot transfer local clipboard for paste, status" << status;
                    m_PasteSequence.compare_exchange_strong(paste, NoPaste);
                }
            }
        }
        // Poll only the revision while Explorer holds a virtual selection. An ordinary
        // manifest poll on older Sunshine builds closes its previous read lease.
        const bool probe = offer && remote && upload.isEmpty() && !refreshOffer;
        if (probe) query.clear();
        if (upload.isEmpty()) {
            m_SendingSequence = NoPaste;
            query.addQueryItem("since", remote ? QString::number(*remote) : QStringLiteral("init"));
            if (m_Files && !probe) {
                query.addQueryItem("download", "1");
                query.addQueryItem("metadataOnly", "1");
                query.addQueryItem("folders", "1");
            }
        }
        url.setQuery(query);
        request.setUrl(url);
        if (isInterruptionRequested()) break;
        auto response = exchange(network, request, m_Certificate, upload.isEmpty() ? nullptr : &upload, {}, 0,
                                 m_Images && upload.isEmpty() ? clipboard_image::max_packet : win_clipboard::max_body);
        if (isInterruptionRequested()) break;
        if (response.status == 404 || response.status == 401 || (response.status == 403 && failures >= 3)) {
            qWarning() << "Clipboard sync unavailable for this session, HTTP" << response.status;
            std::lock_guard lock(m_FileQueue->mutex);
            for (const auto& job : m_FileQueue->jobs) {
                job->progress.state = FileQueue::State::Failed;
                job->progress.error = QStringLiteral("HTTP %1").arg(response.status);
                job->files.clear();
            }
            break;
        }
        if (probe && response.status == 204 && response.sequence && response.sequence != remote) {
            refreshOffer = true;
            continue;
        }
        if (!probe && (response.status == 200 || response.status == 204)) refreshOffer = false;
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
            int status;
            if (m_Files && response.payload.startsWith('M')) {
                const DWORD revision = *response.sequence;
                const auto baseRequest = request;
                const auto manifest = QJsonDocument::fromJson(response.payload.mid(1)).object();
                const auto entries = manifest.value("files").toArray();
                std::vector<FILEDESCRIPTORW> descriptors;
                std::vector<size_t> roots;
                std::vector<clipboard_download::File> selection;
                quint64 total = 0;
                const bool validSelection = response.payload.size() <= qsizetype(win_clipboard::tree::max_manifest) &&
                                            clipboard_download::parseFiles(entries, selection, total) == 200;
                if (validSelection) {
                    const bool hasFolders = std::any_of(selection.begin(), selection.end(), [](const auto& f) { return f.directory; });
                    for (size_t i = 0; i < selection.size(); ++i) {
                        const auto& file = selection[i];
                        if (hasFolders) {
                            if (!file.name.has_parent_path()) roots.push_back(i);
                        } else {
                            FILEDESCRIPTORW descriptor{};
                            descriptor.dwFlags = FD_ATTRIBUTES | FD_FILESIZE | FD_UNICODE;
                            descriptor.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
                            descriptor.nFileSizeHigh = static_cast<DWORD>(file.size >> 32);
                            descriptor.nFileSizeLow = static_cast<DWORD>(file.size);
                            const auto& name = file.name.native(); // A validated basename fits cFileName.
                            std::copy(name.begin(), name.end(), descriptor.cFileName);
                            descriptors.push_back(descriptor);
                        }
                    }
                }
                // Release source handles immediately. Reopen and validate the same
                // remote selection when Explorer asks for its first file stream.
                if (!manifest.value("metadataOnly").toBool())
                    exchange(network, chunkRequest(baseRequest, revision, "finish", manifest.value("lease").toString(), 0, 0), m_Certificate, nullptr, {}, 3000);
                const auto offerId = manifest.value("offer").toString();
                auto download = [&, revision, baseRequest, entries, offerId](ClipboardVirtualFiles::Paths& paths, const std::filesystem::path& destination,
                                                                   HWND folder = nullptr, bool* detached = nullptr) {
                    if (receiving) { qWarning() << "Clipboard download already active"; return false; }
                    QScopedValueRollback<bool> guard(receiving, true);
                    const HRESULT ownership = offer ? OleIsCurrentClipboard(offer) : S_FALSE;
                    if (isInterruptionRequested() || ownership != S_OK || GetClipboardSequenceNumber() != local) {
                        qWarning() << "Clipboard download no longer current" << ownership << local << GetClipboardSequenceNumber();
                        return false;
                    }
                    auto endpoint = m_Host;
                    QUrlQuery query;
                    query.addQueryItem("download", offerId.isEmpty() ? "1" : "offer");
                    if (!offerId.isEmpty()) query.addQueryItem("lease", offerId);
                    query.addQueryItem("files", "1");
                    query.addQueryItem("folders", "1");
                    query.addQueryItem("since", QString::number(offerId.isEmpty() ? revision - 1U : revision));
                    endpoint.setQuery(query);
                    auto open = baseRequest;
                    open.setUrl(endpoint);
                    if (folder || !offerId.isEmpty()) open.setRawHeader("X-Clipboard-Independent", "1");
                    const auto snapshot = exchange(network, open, m_Certificate, nullptr, {}, 10000);
                    if (snapshot.status != 200 || snapshot.sequence != revision || !snapshot.payload.startsWith('M') ||
                        QJsonDocument::fromJson(snapshot.payload.mid(1)).object().value("files").toArray() != entries) {
                        qWarning() << "Clipboard snapshot rejected, HTTP" << snapshot.status << "expected revision" << revision
                                   << "received" << snapshot.sequence.value_or(0) << "network error" << snapshot.networkError;
                        return false;
                    }
                    if (folder && !snapshot.task.isEmpty()) {
                        endpoint.setPath("/clipboard/transfer");
                        endpoint.setQuery(QString{});
                        QJsonObject task {
                            {"host", endpoint.toString()}, {"certificate", QString::fromLatin1(m_Certificate.toPem())},
                            {"token", QString::fromLatin1(snapshot.task)}, {"manifest", QString::fromUtf8(snapshot.payload)},
                            {"revision", QString::number(revision)}, {"folder", QString::fromStdWString(destination.native())},
                            {"window", QString::number(reinterpret_cast<quintptr>(folder))}
                        };
                        *detached = ClipboardTransferProcess::launch(task);
                        if (!*detached) {
                            qWarning() << "Clipboard transfer process did not acknowledge the task";
                            auto cleanup = baseRequest;
                            cleanup.setRawHeader("X-Clipboard-Token", snapshot.task);
                            cleanup.setUrl(endpoint);
                            const auto lease = QJsonDocument::fromJson(snapshot.payload.mid(1)).object().value("lease").toString();
                            exchange(network, chunkRequest(cleanup, revision, "finish", lease, 0, 0), m_Certificate, nullptr, {}, 3000);
                        }
                        return *detached;
                    }
                    if (folder) progressWindow.show(folder);
                    auto& queue = *m_FileQueue;
                    const auto started = monotonicMs();
                    quint64 taskId;
                    {
                        std::lock_guard lock(queue.mutex);
                        queue.download = {};
                        queue.downloadCancelled = false;
                        taskId = ++queue.nextId;
                    }
                    const auto cancelled = [&] { return isInterruptionRequested() || queue.downloadCancelled ||
                                                        (!destination.empty() && progressWindow.cancelled) ||
                                                        GetClipboardSequenceNumber() != local; };
                    const auto report = [&](const ClipboardUploadProgress& progress) {
                        if (!destination.empty()) progressWindow.update(progress);
                        std::lock_guard lock(queue.mutex);
                        if (queue.downloadCancelled) return false;
                        queue.download = progress;
                        queue.download.taskId = taskId;
                        queue.download.startedAtMs = started;
                        return true;
                    };
                    auto reader = baseRequest;
                    if (!snapshot.task.isEmpty()) {
                        endpoint.setPath("/clipboard/transfer");
                        reader.setUrl(endpoint);
                        reader.setRawHeader("X-Clipboard-Token", snapshot.task);
                    }
                    const int status = receiveSnapshot(network, reader, m_Certificate, snapshot.payload, revision,
                                                       destination.empty() ? stagingRoot : destination, local,
                                                       cancelled, report, &paths, !destination.empty());
                    std::lock_guard lock(queue.mutex);
                    if (queue.downloadCancelled) {
                        queue.download = {};
                    } else if (status == 204) {
                        queue.download.state = destination.empty() ? FileQueue::State::ReadyLocally : FileQueue::State::Saved;
                        queue.readySequence = local;
                        queue.readyUntil = monotonicMs() + 4000;
                    } else if (status == 409) {
                        queue.download = {};
                    } else {
                        queue.download.state = FileQueue::State::Failed;
                        queue.download.error = status == 507 ? FileQueue::message("Not enough local disk space") :
                                               FileQueue::message("File download failed");
                    }
                    return status == 204;
                };
                status = GetClipboardSequenceNumber() != local ? 409 : 400;
                if (oleReady && validSelection && GetClipboardSequenceNumber() == local) {
                    auto next = new ClipboardVirtualFiles(std::move(descriptors), [download](ClipboardVirtualFiles::Paths& paths) {
                        return download(paths, {});
                    }, std::move(roots));
                    if (SUCCEEDED(OleSetClipboard(next))) {
                        clearOffer();
                        offer = next;
                        applied = GetClipboardSequenceNumber();
                        pasteToFolder = [&, download, applied](const explorer_paste::Target& target) {
                            bool detached = false;
                            const auto current = explorer_paste::focusedTarget();
                            ClipboardVirtualFiles::Paths unusedPaths;
                            bool ok = false;
                            if (current.window == target.window && current.pane == target.pane &&
                                current.directory == target.directory && GetClipboardSequenceNumber() == applied) {
                                win_clipboard::detail::file_handle pin(CreateFileW(target.directory.c_str(),
                                    FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
                                if (pin.get() != INVALID_HANDLE_VALUE) ok = download(unusedPaths, target.directory, target.window, &detached);
                                else {
                                    qWarning() << "Clipboard destination could not be opened" << GetLastError();
                                    pin.release();
                                }
                            }
                            else {
                                qWarning() << "Clipboard paste target changed" << (current.window == target.window)
                                           << (current.pane == target.pane) << (current.directory == target.directory)
                                           << "sequence" << applied << GetClipboardSequenceNumber();
                            }
                            if (!detached) {
                                if (!ok && !progressWindow.visible()) progressWindow.show(target.window);
                                progressWindow.finish(ok);
                            }
                        };
                        status = 204;
                    } else { next->Release(); status = 409; }
                }
            } else {
                status = win_clipboard::write(m_Text, m_Files, payload, applied, stagingRoot, local, m_Images);
            }
            if (status == 204) {
                // If a paste was waiting on the replaced revision, paste the host's newer copy.
                uint64_t awaiting = local;
                local = applied;
                m_IncomingSequence = applied;
                m_ConfirmedSequence = applied;
                m_PasteSequence.compare_exchange_strong(awaiting, Ready);
            }
            // A newer local copy supersedes the host revision we just saw. Its
            // upload must compare against that revision, even after a long download.
            if (status != 409 || GetClipboardSequenceNumber() != local) remote = response.sequence;
            if (status != 204 && status != 409) qWarning() << "Cannot apply incoming clipboard data, status" << status;
            failures = 0;
        } else if (response.status == 400 || response.status == 413) {
            if (!upload.isEmpty()) local = copied;
            if (explicitPaste) m_PasteSequence.compare_exchange_strong(paste, NoPaste);
            if (upload.isEmpty() && response.sequence) remote = response.sequence;
            failures = 0;
        } else if (!upload.isEmpty() && response.status == 409 &&
                   response.sequence && *response.sequence != 0 && response.sequence != remote) {
            // Fetch the newer host contents on the next poll, unless the user copies again.
            local = copied;
            if (explicitPaste) m_PasteSequence.compare_exchange_strong(paste, Ready);
            failures = 0;
        } else {
            failures = (std::min)(failures + 1, 4);
        }
        const int delay = 250 << failures;
        m_SendingSequence = NoPaste;
        for (int ms = 0; ms < delay && !isInterruptionRequested(); ms += 50) {
            if (m_PasteSequence <= UINT32_MAX && !explicitPaste && !failures) break;
            {
                std::lock_guard lock(m_FileQueue->mutex);
                if (std::any_of(m_FileQueue->jobs.begin(), m_FileQueue->jobs.end(), [](const auto& job) {
                    return job->progress.state == FileQueue::State::Queued;
                })) break;
            }
            waitForEvents(50);
        }
    }
    if (offer && OleIsCurrentClipboard(offer) == S_OK) OleSetClipboard(nullptr);
    clearOffer();
    if (oleReady) OleUninitialize();
}

int ClipboardTransferProcess::run(const QString& channel)
{
    QLocalSocket socket;
    const auto task = take(channel, socket);
    QUrl host(task.value("host").toString());
    QSslCertificate certificate(task.value("certificate").toString().toLatin1());
    const QByteArray token = task.value("token").toString().toLatin1();
    const auto manifest = task.value("manifest").toString().toUtf8();
    bool valid = false;
    const DWORD revision = task.value("revision").toString().toUInt(&valid);
    const auto folder = std::filesystem::path(task.value("folder").toString().toStdWString());
    if (!valid || host.scheme() != "https" || host.host().isEmpty() || host.path() != "/clipboard/transfer" ||
        certificate.isNull() || token.size() != 64 || !manifest.startsWith('M') ||
        !win_clipboard::detail::local_file_path(folder)) return 1;

    // Acquire the directory while the sending process still holds its own pin.
    HANDLE handle = CreateFileW(folder.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return 1;
    win_clipboard::detail::file_handle pin(handle);
    StreamingPreferences::get()->retranslate();
    QNetworkAccessManager network;
    network.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    auto ssl = IdentityManager::get()->getSslConfig();
    ssl.setCaCertificates({certificate});
    QNetworkRequest request(host);
    request.setSslConfiguration(ssl);
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("X-Clipboard-Token", token);

    DesktopTransferProgress panel;
    panel.show(reinterpret_cast<HWND>(task.value("window").toString().toULongLong()));
    if (socket.write("R", 1) != 1 || (socket.bytesToWrite() && !socket.waitForBytesWritten(3000))) return 1;
    socket.disconnectFromServer();
    const auto cancelled = [&] { return panel.cancelled; };
    const auto report = [&](const ClipboardUploadProgress& progress) {
        panel.update(progress);
        return !panel.cancelled;
    };
    const int result = receiveSnapshot(network, request, certificate, manifest, revision, folder, 0,
                                       cancelled, report, nullptr, true);
    pin.reset();
    panel.finish(result == 204);
    while (panel.visible()) {
        QEventLoop loop;
        QTimer::singleShot(100, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return result == 204 ? 0 : 1;
}
