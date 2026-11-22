#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <mstcpip.h>

#include "clipboardsync.h"
#include "clipboarduploadrate.h"
#include "clipboard_win32.h"
#include "transferfilehash.h"
#include "backend/identitymanager.h"

#include <Limelight.h>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstring>
#include <functional>
#include <QNetworkAccessManager>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSettings>
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
        result.task = reply->rawHeader("X-Clipboard-Task");
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
        : m_Rate(directEthernetSpeed(host), QHostAddress(host.host()).isLoopback())
    {
        sampleRtt();
    }

    DWORD nextSize(quint64 remaining)
    {
        const auto now = monotonicMs();
        if (now - m_LastSample >= m_Rate.sampleIntervalMs()) {
            m_LastSample = now;
            sampleRtt();
        }
        return static_cast<DWORD>((std::min<quint64>)(remaining, m_Rate.chunkBytes()));
    }

    bool wait(DWORD bytes, const std::function<bool()>& cancelled)
    {
        const int rate = m_Rate.bytesPerSecond();
        const auto due = m_LastSend + (static_cast<qint64>(bytes) * 1000 + rate - 1) / rate;
        if (!waitForUpload(due, cancelled)) return false;
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
                        QByteArray& id, std::vector<quint64>& offsets)
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
    return true;
}

Result uploadFileBatch(QNetworkAccessManager& network, QNetworkRequest request, const QUrl& host,
                       const QSslCertificate& pairedHost, const std::vector<win_clipboard::detail::source_file>& files,
                       DWORD expectedHostSequence, const std::function<bool()>& cancelled,
                       const std::function<void(const ClipboardUploadProgress&)>& update,
                       bool resumable = false, const std::function<bool()>& discardRequested = {},
                       const QString& task = {}, const std::function<bool()>& allowCommit = {},
                       const QString& sourceKey = {}, QJsonArray* cachedManifest = nullptr)
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
        if (result.status == 204) result.status = 502;
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
    QUrlQuery beginQuery(resumable ? "op=resume" : "op=begin");
    if (!task.isEmpty()) beginQuery.addQueryItem("task", task);
    url.setQuery(beginQuery);
    request.setUrl(url);
    auto begin = exchange(network, request, pairedHost, &manifestBody, {}, 15000);
    if (begin.status != 200) return failure("begin", begin);
    QByteArray id = begin.payload;
    std::vector<quint64> offsets(files.size());
    if (resumable && !readResumeResponse(begin.payload, files, id, offsets)) {
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
        for (auto received : offsets) acknowledged += received;
        progress.resumedBytes = acknowledged;
        progress.acknowledged = acknowledged;
        progress.preparing = false;
        ClipboardUploadPacer pacer(host);
        QByteArray chunk;
        for (index = 0; index < files.size(); ++index) {
            const auto& file = files[index];
            offset = offsets[index];
            if (resumable && !transfer_file::seek(file.handle.get(), offset)) return failure("resume", {}, "local file seek");
            progress.index = static_cast<int>(index);
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
        for (int attempt = 0; ; ++attempt) {
            auto result = post(query, {});
            if (result.status == 204 && (!task.isEmpty() || result.sequence)) return result;
            if (result.status != 409 || result.conflict != "busy" || attempt >= 5) return failure("commit", result);
            if (!waitForUpload(monotonicMs() + (100 << attempt), cancelled)) return failure("commit", {}, "cancelled");
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
        bool verifyClipboard = false;
        bool uploadStarted = false;
        DWORD publishedSequence = 0;
    };

    mutable std::mutex mutex;
    std::deque<std::shared_ptr<Job>> jobs;
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
            if (!GetFileInformationByHandle(file.handle.get(), &info)) return {};
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
                    if (!next->verifyClipboard) continue;
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
            endpoint.setQuery(QStringLiteral("since=init&files=1&text=0"));
            auto req = request;
            req.setUrl(endpoint);
            const auto result = exchange(net, req, sync.m_Certificate, nullptr, interrupted, 3000);
            std::lock_guard lock(mutex);
            job->verifyClipboard = false;
            if (result.status == 204 && result.sequence == job->publishedSequence) {
                job->progress.state = job->automaticPaste ? State::WaitingForPaste : State::NeedsPaste;
            } else {
                job->published = false;
                job->progress.error = message("Remote clipboard changed");
                job->progress.state = State::Failed;
                if (result.sequence) remote = result.sequence;
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
                             bool resumable, bool multitask)
    : m_FileQueue(std::make_unique<FileQueue>()), m_Host(std::move(host)), m_Certificate(std::move(certificate)),
      m_Token(std::move(token)), m_Text(text), m_Files(files), m_Resumable(files && resumable),
      m_Multitask(m_Resumable && multitask), m_LocalSequence(GetClipboardSequenceNumber())
{
    m_Host.setPath("/clipboard");
    m_FileQueue->controls.setMaxThreadCount(8);
    if (!m_Token.isEmpty() && !m_Certificate.isNull() && (m_Text || m_Files)) {
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
    if (!(hasFiles ? m_Files : m_Text && IsClipboardFormatAvailable(CF_UNICODETEXT))) return false;
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
        const auto status = win_clipboard::read(false, true, {}, unused, job->sequence, &job->files);
        if (status != 200 || job->files.empty() || job->sequence != sequence) {
            job->files.clear();
            job->progress.state = FileQueue::State::Failed;
            job->progress.error = FileQueue::message("Cannot open the selected files");
            queue.jobs.push_back(job);
            return true;
        }
        for (const auto& file : job->files) {
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
    job->verifyClipboard = job->published;
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
    if (state != NoPaste && state != Ready && monotonicMs() - m_PasteActivity.load() < 15000 && isRunning()) return 0;
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

    DWORD local = m_LocalSequence;
    std::optional<DWORD> remote;
    int failures = 0;
    while (!isInterruptionRequested()) {
        if (m_FileQueue->process(*this, network, local, remote)) {
            msleep(20);
            continue;
        }
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
            const bool syncFiles = m_Files && !m_Multitask && current != m_DeferredFileSequence;
            int status = win_clipboard::read(m_Text, syncFiles, explicitPaste ? std::nullopt : std::optional<DWORD>(local),
                                             encoded, copied);
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
            std::lock_guard lock(m_FileQueue->mutex);
            for (const auto& job : m_FileQueue->jobs) {
                job->progress.state = FileQueue::State::Failed;
                job->progress.error = QStringLiteral("HTTP %1").arg(response.status);
                job->files.clear();
            }
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
                m_IncomingSequence = applied;
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
            {
                std::lock_guard lock(m_FileQueue->mutex);
                if (std::any_of(m_FileQueue->jobs.begin(), m_FileQueue->jobs.end(), [](const auto& job) {
                    return job->progress.state == FileQueue::State::Queued;
                })) break;
            }
            msleep(50);
        }
    }
}
