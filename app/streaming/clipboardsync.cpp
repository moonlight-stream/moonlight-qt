#include "clipboardsync.h"
#include "clipboard_win32.h"
#include "backend/identitymanager.h"

#include <QDebug>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>

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
    if (reply->error() == QNetworkReply::NoError || status >= 400) {
        result.status = status;
        bool valid = false;
        DWORD sequence = reply->rawHeader("X-Clipboard-Sequence").toUInt(&valid);
        if (valid) result.sequence = sequence;
        const auto offset = reply->rawHeader("X-Clipboard-Offset").toULongLong(&valid);
        if (valid) result.nextOffset = offset;
    }
    delete reply;
    return result;
}

Result uploadFileBatch(QNetworkAccessManager& network, QNetworkRequest request, const QUrl& host,
                       const QSslCertificate& pairedHost, const std::vector<win_clipboard::detail::source_file>& files,
                       DWORD expectedHostSequence, const std::function<bool()>& cancelled,
                       const std::function<void(int)>& progress)
{
    constexpr quint64 MaxBatchBytes = 16ULL * 1024 * 1024 * 1024;
    constexpr DWORD ChunkBytes = 4 * 1024 * 1024;
    QJsonArray manifest;
    quint64 total = 0;
    for (const auto& file : files) {
        if (file.size > MaxBatchBytes - total) return {413, {}, {}, {}};
        total += file.size;
        manifest.append(QJsonObject{{"name", QString::fromUtf8(file.name.data(), static_cast<int>(file.name.size()))},
                                    {"size", static_cast<double>(file.size)}});
    }

    QUrl url = host;
    url.setPath("/clipboard/batch");
    auto post = [&](const QUrlQuery& query, const QByteArray& body, bool cleanup = false) {
        url.setQuery(query);
        request.setUrl(url);
        if (!cleanup && cancelled()) return Result{};
        return exchange(network, request, pairedHost, &body, cleanup ? std::function<bool()>{} : cancelled,
                        cleanup ? 3000 : 120000);
    };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    auto begin = post(QUrlQuery("op=begin"), QJsonDocument(manifest).toJson(QJsonDocument::Compact));
    if (begin.status != 200) {
        begin.sequence.reset();
        return begin;
    }
    const auto& id = begin.payload;
    if (id.size() != 32 || !std::all_of(id.begin(), id.end(), [](char ch) {
            return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
        })) return {};

    auto batchQuery = [&](const QString& op) {
        QUrlQuery query;
        query.addQueryItem("op", op);
        query.addQueryItem("id", QString::fromLatin1(id));
        return query;
    };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    auto send = [&]() -> Result {
        quint64 acknowledged = 0;
        progress(0);
        QByteArray chunk;
        chunk.reserve(ChunkBytes);
        for (size_t index = 0; index < files.size(); ++index) {
            const auto& file = files[index];
            quint64 offset = 0;
            while (offset < file.size) {
                if (cancelled()) return {};
                const DWORD length = static_cast<DWORD>((std::min<quint64>)(ChunkBytes, file.size - offset));
                chunk.resize(length);
                DWORD count = 0;
                if (!ReadFile(file.handle.get(), chunk.data(), length, &count, nullptr) || count != length) return {};
                auto query = batchQuery("chunk");
                query.addQueryItem("file", QString::number(index));
                query.addQueryItem("offset", QString::number(offset));
                auto result = post(query, chunk);
                if (result.status != 204) return result;
                offset += length;
                if (result.nextOffset != offset) return {};
                acknowledged += length;
                progress(static_cast<int>(acknowledged * 100 / total));
            }
        }
        auto query = batchQuery("commit");
        query.addQueryItem("since", QString::number(expectedHostSequence));
        return post(query, {});
    };
    auto result = send();
    if (result.status != 204 || !result.sequence) post(batchQuery("abort"), {}, true);
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
    m_PasteSequence = NoPaste;
    m_PasteCount = 0;
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
                const auto cancelled = [&] {
                    return isInterruptionRequested() || m_PasteSequence != copied || GetClipboardSequenceNumber() != copied;
                };
                const auto result = uploadFileBatch(network, request, m_Host, m_Certificate, largeFiles, *remote, cancelled,
                                                    [this](int percent) { m_FileUploadPercent = percent; });
                m_FileUploadPercent = -1;
                local = copied;
                if (result.status == 204 && result.sequence) {
                    remote = result.sequence;
                    m_PasteActivity = monotonicMs();
                    m_ConfirmedSequence = copied;
                    m_PasteSequence.compare_exchange_strong(paste, Ready);
                } else {
                    qWarning() << "Clipboard file batch did not complete, HTTP" << result.status;
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
