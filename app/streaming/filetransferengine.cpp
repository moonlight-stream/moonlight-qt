#include "filetransferengine.h"
#include "filetransfercheckpoint.h"
#include "clipboard_tree.h"
#include "transferfilehash.h"
#include "shellfiledrop.h"
#include "backend/identitymanager.h"

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <deque>
#include <filesystem>
#include <openssl/rand.h>
#include <functional>

namespace {
constexpr qint64 ChunkSize = 1024 * 1024;
constexpr int SmallFileConcurrency = 4;
constexpr qint64 ListLimit = 16 * 1024 * 1024;
using Flag = std::shared_ptr<std::atomic_bool>;
using Params = QList<QPair<QString, QString>>;
using Progress = std::function<void(qint64)>;
using Engine = FileTransferEngine;
using FileAction = Engine::FileAction;
using ConflictResolver = std::function<QList<FileConflictChoice>(bool, int, const QList<FileConflict>&)>;

struct ObservedConflict {
    FileConflict details;
    QString source;
    QString replaceVersion;
};

struct ConflictDecision {
    FileConflictChoice choice;
    QString replaceVersion;
};

using ConflictDecisions = QHash<QPair<QString, QString>, ConflictDecision>;

struct CopyTarget {
    QString path;
    QString replaceVersion; // Empty means that publication must not overwrite anything.
};

namespace ck = transfer_checkpoint;
struct Failure { QString message; bool interrupted = false; int code = 0; };

bool needsConflictDecision(const QJsonObject& state)
{
    return !state.value("skipped").toBool() && state.value("copyName").toString().isEmpty() &&
           state.value("replace").toString().isEmpty() && !state.contains("sha256") && !state.contains("identity");
}

void checkCancelled(const Flag& flag)
{
    if (*flag) throw Failure{Engine::tr("Cancelled")};
}

bool safeName(const QString& name)
{
    static const QRegularExpression reserved(QStringLiteral(
        R"(^(CON|PRN|AUX|NUL|CLOCK\$|CONIN\$|CONOUT\$|(?:COM|LPT)[0-9¹²³]) *(?:\.|$))"),
        QRegularExpression::CaseInsensitiveOption);
    if (name.isEmpty() || name.size() > 255 || name.endsWith('.') || name.endsWith(' ') || reserved.match(name).hasMatch()) return false;
    for (const auto c : name) {
        if (c.unicode() < 32 || QStringLiteral("\\/:*?\"<>|").contains(c)) return false;
    }
    return true;
}

bool driveRoot(const QString& path)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z]:/$"));
    return pattern.match(path).hasMatch();
}

QString childPath(const QString& parent, const QString& name)
{
    return parent.endsWith('/') ? parent + name : parent + '/' + name;
}

bool validRemoteDragPath(const QString& path)
{
    if (!driveRoot(path.left(3))) return false;
    if (path.size() == 3) return true;
    const auto parts = path.mid(3).split('/');
    return std::all_of(parts.cbegin(), parts.cend(), safeName);
}

QString availableCopyName(const QString& name, const QSet<QString>& existing)
{
    if (!existing.contains(name.toCaseFolded())) return name;
    const int dot = name.lastIndexOf('.');
    const auto stem = dot > 0 ? name.left(dot) : name;
    const auto extension = dot > 0 ? name.mid(dot) : QString();
    for (int n = 1; ; ++n) {
        const QString suffix = n == 1 ? Engine::tr(" - Copy") : Engine::tr(" - Copy (%1)").arg(n);
        const bool keepExtension = suffix.size() + extension.size() < 255;
        const auto ending = suffix + (keepExtension ? extension : QString());
        auto prefix = (keepExtension ? stem : name).left(255 - ending.size());
        if (!prefix.isEmpty() && prefix.back().isHighSurrogate()) prefix.chop(1);
        const auto candidate = prefix + ending;
        if (!existing.contains(candidate.toCaseFolded())) return candidate;
    }
}

using ck::localFileVersion;

void checkReplacement(const CopyTarget& target)
{
    if (target.replaceVersion != localFileVersion(target.path))
        throw Failure{Engine::tr("The file changed or is in use: %1").arg(target.path)};
}

// Keep the old file in place while a local copy or a legacy download is being written.
void writeLocalCopy(const CopyTarget& target, const Flag& cancelled, const std::function<void(const QString&)>& write)
{
    if (target.replaceVersion.isEmpty()) {
        write(target.path);
        return;
    }
    const auto parent = QFileInfo(target.path).absolutePath();
    std::vector<ck::Handle> pins;
    if (!ck::pinDirectory(parent, pins)) throw Failure{Engine::tr("Cannot open folder: %1").arg(parent)};
    QTemporaryDir staging(QDir(parent).filePath(".moonlight-XXXXXX"));
    if (!staging.isValid()) throw Failure{staging.errorString()};
    const auto staged = staging.filePath("file");
    write(staged);
    checkCancelled(cancelled);
    checkReplacement(target);
    if (!MoveFileExW(qUtf16Printable(QDir::toNativeSeparators(staged)), qUtf16Printable(QDir::toNativeSeparators(target.path)),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw Failure{Engine::tr("The file changed or is in use: %1").arg(target.path)};
}

QList<TransferEntry> localList(const QString& path, const Flag& cancelled)
{
    QList<TransferEntry> entries;
    if (path.isEmpty()) {
        for (const auto& info : QDir::drives()) {
            const auto root = info.absoluteFilePath();
            entries.append({root, root, true, 0, {}});
        }
        return entries;
    }
    // QDir::entryInfoList also returns an empty list when access is denied.
    // Propagate enumeration failures so a failed folder copy cannot appear complete.
    std::error_code error;
    std::filesystem::directory_iterator it(path.toStdWString(), error), end;
    for (; !error && it != end; it.increment(error)) {
        checkCancelled(cancelled);
        const QFileInfo info(QString::fromStdWString(it->path().wstring()));
        if (info.isSymLink() || info.isJunction() || (!info.isDir() && !info.isFile())) continue;
        entries.append({info.fileName(), info.absoluteFilePath(), info.isDir(),
                        info.isDir() ? 0 : info.size(), info.lastModified()});
    }
    if (error) throw Failure{Engine::tr("Cannot open folder: %1").arg(path)};
    return entries;
}

// A connection stays on its pool thread and is scoped to this engine's peer and client identity.
using Networks = QThreadStorage<QNetworkAccessManager*>;

QNetworkAccessManager& transferNetwork(Networks& networks)
{
    if (!networks.hasLocalData()) {
        auto* manager = new QNetworkAccessManager;
        manager->setProxy(QNetworkProxy::NoProxy);
        networks.setLocalData(manager);
    }
    return *networks.localData();
}

class Worker
{
public:
    Worker(const Worker&) = delete;
    Worker(Worker&&) = default;

    Worker(Networks& networks, QUrl host, const QSslConfiguration& ssl, QByteArray token, Flag cancelled, bool standalone,
           ConflictResolver resolve = {}, ConflictDecisions* decisions = nullptr, std::optional<QJsonObject>* batchCaps = nullptr) :
        networks(networks), host(std::move(host)), certificate(ssl.caCertificates().value(0)), cancelled(std::move(cancelled)),
        needsSession(standalone && !(batchCaps && batchCaps->has_value())), resolveConflicts(std::move(resolve)),
        conflictDecisions(decisions), batchCaps(batchCaps)
    {
        request.setSslConfiguration(ssl);
        request.setRawHeader("X-Clipboard-Token", std::move(token));
        if (standalone) request.setRawHeader("X-Moonlight-File-Session", "1");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    }

    QList<TransferEntry> list(bool remote, const QString& path, const QString& name = {})
    {
        checkCancelled(cancelled);
        if (!remote) {
            if (name.isEmpty()) return localList(path, cancelled);
            const QFileInfo info(childPath(path, name));
            if (!info.exists() && !info.isSymLink() && !info.isJunction()) return {};
            return {{name, info.absoluteFilePath(), info.isDir(), info.size(), info.lastModified(), localFileVersion(info.absoluteFilePath())}};
        }
        Params params{{"path", path}};
        if (!name.isEmpty()) params.append({"name", name});
        const auto payload = exchange("list", params, nullptr, ListLimit);
        const auto array = QJsonDocument::fromJson(payload).object().value("entries");
        const Failure invalid{Engine::tr("Invalid folder listing from host")};
        if (!array.isArray()) throw invalid;
        QList<TransferEntry> entries;
        for (const auto item : array.toArray()) {
            checkCancelled(cancelled);
            const auto obj = item.toObject();
            TransferEntry entry;
            entry.name = obj.value("name").toString();
            entry.path = obj.value("path").toString();
            if (!obj.value("directory").isBool()) throw invalid;
            entry.directory = obj.value("directory").toBool();
            entry.replaceVersion = obj.value("replaceVersion").toString();
            if (path.isEmpty()) {
                if (!driveRoot(entry.path) || !entry.directory) throw invalid;
                entry.name = entry.path;
            } else {
                if (!safeName(entry.name)) throw invalid;
                // Only accept direct children of the directory that was requested.
                // Reconstruct their paths locally even if the peer returns another path.
                entry.path = childPath(path, entry.name);
            }
            bool ok = false;
            entry.size = obj.value("size").toString().toLongLong(&ok);
            if (!ok || entry.size < 0) throw invalid;
            const qint64 stamp = obj.value("modified").toString().toLongLong(&ok);
            if (ok) entry.modified = QDateTime::fromMSecsSinceEpoch(stamp);
            entries.append(std::move(entry));
        }
        return entries;
    }

    // Only the browser waits for the interactive desktop. File operations keep
    // their original failure and are never replayed by this check.
    QList<TransferEntry> browse(bool remote, const QString& path, bool& waitingForDesktop)
    {
        try {
            return list(remote, path);
        } catch (const Failure& denied) {
            if (!remote || denied.code != 403 || !desktopUnavailable(path)) throw;
            waitingForDesktop = true;
            return {};
        }
    }

    QList<TransferPlace> places(bool remote)
    {
        QList<TransferPlace> result;
        if (remote) {
            const auto obj = QJsonDocument::fromJson(exchange("places", {}, nullptr, 128 * 1024)).object();
            const auto entries = obj.value("entries");
            if (!entries.isArray()) throw Failure{Engine::tr("Invalid folder listing from host")};
            for (const auto value : entries.toArray()) {
                const auto item = value.toObject();
                const auto path = QDir::fromNativeSeparators(item.value("path").toString());
                if (!QDir::isAbsolutePath(path)) continue;
                result.append({item.value("kind").toString(), item.value("name").toString(), path});
            }
            return result;
        }
        for (const auto& drive : QDir::drives()) {
            checkCancelled(cancelled);
            const auto path = drive.absoluteFilePath();
            const auto label = QStorageInfo(path).name();
            const auto letter = path.left(2);
            result.append({"drive", label.isEmpty() ? letter : QStringLiteral("%1 (%2)").arg(label, letter), path});
        }
        const struct { const char* kind; QStandardPaths::StandardLocation location; } folders[] = {
            {"documents", QStandardPaths::DocumentsLocation}, {"desktop", QStandardPaths::DesktopLocation},
            {"pictures", QStandardPaths::PicturesLocation}, {"videos", QStandardPaths::MoviesLocation},
            {"music", QStandardPaths::MusicLocation}, {"downloads", QStandardPaths::DownloadLocation}
        };
        for (const auto& folder : folders) {
            const auto path = QStandardPaths::writableLocation(folder.location);
            if (!path.isEmpty() && QFileInfo(path).isDir()) result.append({QLatin1String(folder.kind), {}, path});
        }
        return result;
    }

    FileDragLocation dragLocation(bool source, QPoint position)
    {
        const auto payload = exchange(source ? "drag-source" : "drop-target",
                                      {{"x", QString::number(position.x())}, {"y", QString::number(position.y())}}, nullptr, ListLimit);
        const auto obj = QJsonDocument::fromJson(payload).object();
        const Failure invalid{Engine::tr("Invalid folder listing from host")};
        FileDragLocation result;
        if (!source) {
            if (!obj.value("directory").isString()) throw invalid;
            result.folder = obj.value("directory").toString();
            if (!result.folder.isEmpty() && !validRemoteDragPath(result.folder)) throw invalid;
            return result;
        }
        if (!obj.value("entries").isArray()) throw invalid;
        const auto entries = obj.value("entries").toArray();
        QSet<QString> names;
        if (entries.size() > 4096) throw invalid;
        for (const auto item : entries) {
            const auto record = item.toObject();
            TransferEntry entry;
            entry.path = record.value("path").toString();
            entry.name = record.value("name").toString();
            bool sizeOk, timeOk;
            entry.size = record.value("size").toString().toLongLong(&sizeOk);
            entry.modified = QDateTime::fromMSecsSinceEpoch(record.value("modified").toString().toLongLong(&timeOk));
            if (!validRemoteDragPath(entry.path) || !safeName(entry.name) ||
                entry.path.section('/', -1) != entry.name || !record.value("directory").isBool() ||
                !sizeOk || entry.size < 0 || !timeOk || names.contains(entry.name.toCaseFolded())) throw invalid;
            entry.directory = record.value("directory").toBool();
            names.insert(entry.name.toCaseFolded());
            result.entries.append(std::move(entry));
        }
        return result;
    }

    bool makeDirectory(bool remote, const QString& parent, const QString& name, bool merge = false)
    {
        if (parent.isEmpty() || !safeName(name))
            throw Failure{Engine::tr("Invalid file or folder: %1").arg(name)};
        const auto path = childPath(parent, name);
        if (remote) {
            try { exchange("mkdir", {{"path", path}}, &empty); }
            catch (const Failure& error) {
                if (!merge || error.code != 409) throw;
                for (const auto& item : list(true, parent, name))
                    if (item.directory && item.name.compare(name, Qt::CaseInsensitive) == 0) return false;
                throw;
            }
        } else if (!QDir(parent).mkdir(name)) {
            std::vector<ck::Handle> pins;
            if (merge && ck::pinDirectory(path, pins)) return false;
            throw Failure{Engine::tr("Cannot create folder: %1").arg(path)};
        }
        return true;
    }

    void manageFiles(FileAction action, bool remote, const QList<TransferEntry>& entries, const QString& folder)
    {
        if (folder.isEmpty() || !QDir::isAbsolutePath(folder) || entries.isEmpty() ||
            (action == FileAction::DownloadToOpen && (!remote || entries.size() != 1)))
            throw Failure{Engine::tr("Invalid file or folder: %1").arg(folder)};
        for (const auto& entry : entries) {
            if ((entry.directory && action == FileAction::DownloadToOpen) || entry.size < 0 || !safeName(entry.name) || !QDir::isAbsolutePath(entry.path) ||
                QFileInfo(entry.path).fileName() != entry.name ||
                (action == FileAction::Delete && QDir::cleanPath(entry.path).compare(QDir::cleanPath(childPath(folder, entry.name)), Qt::CaseInsensitive) != 0))
                throw Failure{Engine::tr("Invalid file or folder: %1").arg(entry.path)};
        }
        if (action == FileAction::DownloadToOpen) {
            const auto& entry = entries.first();
            const auto target = childPath(folder, entry.name);
            backgroundTransfers = capabilities().value("backgroundTransfers").toBool();
            if (backgroundTransfers) {
                withDownloadSource(entry, ck::nonce(), false, [&](const QString& sha, const QString& readId) {
                    download(entry, target, [](qint64) {}, readId, sha);
                });
            } else {
                download(entry, target, [](qint64) {});
            }
            // Preserve Windows attachment checks when opening the downloaded file with its default application.
            QFile zone(target + QStringLiteral(":Zone.Identifier"));
            const QByteArray mark("[ZoneTransfer]\r\nZoneId=3\r\n");
            if (!zone.open(QIODevice::WriteOnly) || zone.write(mark) != mark.size() || !zone.flush())
                throw Failure{zone.errorString()};
            return;
        }
        if (remote && action == FileAction::Delete) {
            const auto caps = capabilities();
            const auto allowed = caps.value("deleteFiles");
            if (!allowed.isBool()) throw Failure{Engine::tr("Update Sunshine to delete remote files.")};
            if (!allowed.toBool()) throw Failure{Engine::tr("Enable “Allow file deletion” in Sunshine under Configuration → Input.")};
            if (!caps.value("deleteDirectories").toBool() && std::any_of(entries.cbegin(), entries.cend(), [](const auto& e) { return e.directory; }))
                throw Failure{Engine::tr("Update Sunshine to delete remote folders.")};
        }
        if (action == FileAction::Copy) prepareConflicts(remote, remote, entries, folder);
        for (const auto& entry : entries) {
            checkCancelled(cancelled);
            if (!remote) {
                const QFileInfo info(entry.path);
                if ((entry.directory ? !info.isDir() : !info.isFile() || info.size() != entry.size) || info.isSymLink() || info.isJunction() ||
                    (entry.modified.isValid() && info.lastModified() != entry.modified))
                    throw Failure{Engine::tr("Source changed: %1").arg(entry.path)};
            }
            if (action == FileAction::Delete) deleteFile(remote, entry);
            else copyEntry(remote, remote, entry, folder, [](qint64) {});
        }
    }

    void prepareConflicts(bool fromRemote, bool toRemote, const QList<TransferEntry>& entries, const QString& folder)
    {
        if (!conflictDecisions || (entries.size() == 1 && !entries.first().directory)) return;
        const bool resumable = fromRemote != toRemote && capabilities().value("version").toInt() >= 2;
        QList<ObservedConflict> observed;
        const auto names = listConflictNames(toRemote, folder, entries.size());
        for (const auto& entry : entries) {
            checkCancelled(cancelled);
            if (names && !names->contains(entry.name.toCaseFolded())) continue;
            const auto previous = observed.size();
            try {
                if (resumable) openJournal(fromRemote, entry, childPath(folder, entry.name));
                const auto closeJournal = qScopeGuard([this] { journal.reset(); });
                collectConflicts(fromRemote, toRemote, entry, folder, observed);
            } catch (const Failure&) {
                checkCancelled(cancelled);
                // The actual job will report its own listing, source or checkpoint failure.
                observed.resize(previous);
            }
        }
        if (observed.isEmpty()) return;
        QList<FileConflict> conflicts;
        for (const auto& conflict : observed) conflicts.append(conflict.details);
        const auto choices = resolveConflicts(toRemote, int(entries.size()), conflicts);
        checkCancelled(cancelled);
        for (qsizetype i = 0; i < observed.size(); ++i) {
            const auto& conflict = observed[i];
            conflictDecisions->insert({conflict.source, conflict.details.path}, {choices[i], conflict.replaceVersion});
        }
    }

    std::shared_ptr<ck::Journal> copy(bool fromRemote, const TransferEntry& entry, const QString& folder,
                                     const Progress& report, const Flag& closing, QThreadPool& smallFiles)
    {
        filePool = &smallFiles;
        const auto target = childPath(folder, entry.name);
        try {
            const auto caps = capabilities();
            backgroundTransfers = caps.value("backgroundTransfers").toBool();
            if (caps.value("version").toInt() >= 2) openJournal(fromRemote, entry, target);
            copyEntry(fromRemote, !fromRemote, entry, folder, report);
        } catch (Failure& e) {
            // All sibling writes have joined. A lost grant after partial progress is resumable.
            if (e.code == 403 && journal && bytes > 0) e.interrupted = true;
            if (!*closing && (*cancelled || !e.interrupted)) {
                // An explicit cancel can arrive before the session/capabilities request finishes.
                if (*cancelled && !journal) openJournal(fromRemote, entry, target);
                if (journal) clean(fromRemote, entry, target);
            }
            throw;
        }
        // Retain the final receipt until the UI consumes the terminal event and retires its durable queue row.
        return std::move(journal);
    }

    void discard(bool fromRemote, const TransferEntry& entry, const QString& folder)
    {
        const auto target = childPath(folder, entry.name);
        openJournal(fromRemote, entry, target);
        clean(fromRemote, entry, target);
    }

    qint64 bytes = 0;
    qint64 wireBytes = 0;
    int skipped = 0;

    void copyEntry(bool fromRemote, bool toRemote, const TransferEntry& entry, const QString& parent, const Progress& report,
                   int depth = 0, bool newParent = false, bool parallel = false)
    {
        checkCancelled(cancelled);
        if (parent.isEmpty() || !safeName(entry.name) || depth > 128)
            throw Failure{Engine::tr("Invalid file or folder: %1").arg(entry.path)};
        if (!fromRemote) {
            const QFileInfo info(entry.path);
            if (!info.exists() || info.isSymLink() || info.isJunction() || entry.directory != info.isDir())
                throw Failure{Engine::tr("Source changed: %1").arg(entry.path)};
        }
        auto destination = childPath(parent, entry.name);
        if (entry.directory) {
            auto name = entry.name;
            if (!depth && fromRemote == toRemote && copyingToSourceParent(fromRemote, entry.path, parent)) {
                const auto choice = chooseTarget(toRemote, entry, parent, true, report);
                if (!choice) return;
                destination = choice->path;
                name = QFileInfo(destination).fileName();
            }
            const auto children = list(fromRemote, entry.path);
            const bool created = journal ? createJobDirectory(fromRemote, parent, name, destination) :
                                           makeDirectory(toRemote, parent, name, name == entry.name);
            std::vector<ck::Handle> pins;
            if (!toRemote && !ck::pinDirectory(destination, pins))
                throw Failure{Engine::tr("Cannot open folder: %1").arg(destination)};
            copyChildren(fromRemote, toRemote, children, destination, report, depth + 1, created);
            return;
        }

        // A second spelling of a pending name must see the first file's publication and receipt.
        if (std::any_of(pendingFiles.cbegin(), pendingFiles.cend(), [&](const auto& file) {
                return file.path.compare(destination, Qt::CaseInsensitive) == 0;
            })) finishFiles(report);
        auto state = journal ? record(destination) : QJsonObject{};
        CopyTarget target{destination, state.value("replace").toString()};
        const auto savedName = state.value("copyName").toString();
        if (!savedName.isEmpty()) {
            if (!safeName(savedName) || !target.replaceVersion.isEmpty()) throw Failure{Engine::tr("Cannot read transfer checkpoint")};
            target.path = childPath(parent, savedName);
        } else if (state.value("skipped").toBool()) {
            ++skipped;
            return;
        } else if (!newParent && needsConflictDecision(state)) {
            // Older checkpoints already own their partial file or publication receipt.
            // A fresh copy asks before writing and remembers the decision across a restart.
            const auto choice = chooseTarget(toRemote, entry, parent, fromRemote == toRemote, report);
            if (!choice) {
                ++skipped;
                state.insert("skipped", true);
                if (journal) save(state);
                return;
            }
            target = *choice;
            if (target.path != destination) state.insert("copyName", QFileInfo(target.path).fileName());
            if (!target.replaceVersion.isEmpty()) state.insert("replace", target.replaceVersion);
            if (journal && (target.path != destination || !target.replaceVersion.isEmpty())) save(state);
        }
        if (parallel && target.path == destination && target.replaceVersion.isEmpty()) {
            queueSmallFile(fromRemote, entry, target, state);
        } else {
            finishFiles(report);
            transferFile(fromRemote, toRemote, entry, target, state, report);
        }
    }

    void transferFile(bool fromRemote, bool toRemote, const TransferEntry& entry, const CopyTarget& target,
                      const QJsonObject& state, const Progress& report)
    {
        if (fromRemote == toRemote) {
            if (toRemote) {
                writeChunks(target, entry.size, [&](qint64 offset, qint64 count) { return readBlock(entry, offset, count); }, report);
            } else {
                writeLocalCopy(target, cancelled, [&](const QString& path) {
                    QFile file(entry.path);
                    if (!file.copy(path)) throw Failure{QStringLiteral("%1\n%2").arg(path, file.errorString())};
                });
            }
        } else if (fromRemote) {
            if (journal) resumeDownload(entry, target, state, report);
            else writeLocalCopy(target, cancelled, [&](const QString& path) { download(entry, path, report); });
        } else {
            if (journal) resumeUpload(entry, target, state, report);
            else upload(entry, target, report);
        }
    }


    void endSession()
    {
        // Always end the original grant, even if the last request was interrupted.
        // A lost end request is also covered by the host's idle expiry.
        try { sendRequest("session/end", {}, &empty, 4096, true); }
        catch (const Failure&) {}
    }

private:
    struct FileResult {
        qint64 bytes;
        qint64 wireBytes;
        std::optional<Failure> failure;
    };
    struct PendingFile {
        QString path;
        std::future<FileResult> result;
    };
    std::deque<PendingFile> pendingFiles;

    void copyChildren(bool fromRemote, bool toRemote, const QList<TransferEntry>& children, const QString& destination,
                      const Progress& report, int depth, bool created)
    {
        // Join even on a listing/target error before releasing the directory pins or cleaning its journal.
        auto join = qScopeGuard([&] {
            while (!pendingFiles.empty()) finishOneFile(report);
        });
        for (const auto& child : children) {
            const bool parallel = filePool && children.size() > 1 && !child.directory && child.size <= ChunkSize;
            finishFiles(report, parallel ? SmallFileConcurrency - 1 : 0);
            copyEntry(fromRemote, toRemote, child, destination, report, depth, created, parallel);
        }
        finishFiles(report);
    }

    void queueSmallFile(bool fromRemote, const TransferEntry& entry, const CopyTarget& target, const QJsonObject& state)
    {
        Worker file(networks, host, request.sslConfiguration(), request.rawHeader("X-Clipboard-Token"), cancelled,
                    request.hasRawHeader("X-Moonlight-File-Session"));
        file.needsSession = false; // The coordinator opened the grant. Each request still checks it.
        file.backgroundTransfers = backgroundTransfers;
        file.journal = journal;
        std::packaged_task<FileResult()> task([file = std::move(file), fromRemote, entry, target, state]() mutable {
            std::optional<Failure> failure;
            try {
                file.transferFile(fromRemote, !fromRemote, entry, target, state, [](qint64) {});
            } catch (const Failure& e) { failure = e; }
            return FileResult{file.bytes, file.wireBytes, failure};
        });
        pendingFiles.push_back({target.path, task.get_future()});
        filePool->start(std::move(task));
    }

    std::optional<Failure> finishOneFile(const Progress& report)
    {
        auto result = pendingFiles.front().result.get();
        pendingFiles.pop_front();
        bytes += result.bytes;
        wireBytes += result.wireBytes;
        report(bytes);
        return result.failure;
    }

    void finishFiles(const Progress& report, size_t maxPending = 0)
    {
        while (pendingFiles.size() > maxPending) {
            if (auto failure = finishOneFile(report)) throw *failure;
        }
    }

    bool desktopUnavailable(const QString& path)
    {
        if (!path.isEmpty()) {
            try {
                sendRequest("list", {{"path", {}}}, nullptr, ListLimit, false);
                return false; // The drive list is readable; only this folder was denied.
            } catch (const Failure& rootError) {
                if (rootError.code != 403) throw;
            }
        }
        // Capabilities requires the same grant as list, but no interactive shell.
        try {
            const auto response = sendRequest("capabilities", {}, nullptr, 4096, false);
            return QJsonDocument::fromJson(response).object().value("version").toInt() >= 1;
        } catch (const Failure& probe) {
            if (probe.code == 401 || probe.code == 403)
                throw Failure{Engine::tr("File session denied. Check file browsing permission in Sunshine and reconnect.")};
            if (probe.code == 400 || probe.code == 404 || probe.code == 405) return false;
            throw;
        }
    }

    QString folderPath(bool remote, const QString& path)
    {
        QString resolved;
        if (remote) {
            try { resolved = QString::fromUtf8(exchange("folder-path", {{"path", path}}, nullptr, 128 * 1024)); }
            catch (const Failure& e) {
                if (e.code == 405 || (e.code == 404 && !capabilities().value("folderPaths").toBool()))
                    throw Failure{Engine::tr("Update Sunshine to copy folders within the remote PC.")};
                throw;
            }
        } else {
            std::vector<ck::Handle> pins;
            if (ck::pinDirectory(path, pins)) resolved = QString::fromStdWString(win_clipboard::tree::folder_path(pins.back().get()));
        }
        if (resolved.isEmpty()) throw Failure{Engine::tr("Cannot open folder: %1").arg(path)};
        return QDir::fromNativeSeparators(resolved);
    }

    bool copyingToSourceParent(bool remote, const QString& path, const QString& parent)
    {
        const auto source = folderPath(remote, path);
        const auto targetParent = folderPath(remote, parent);
        if (targetParent.compare(source, Qt::CaseInsensitive) == 0 || targetParent.startsWith(source + '/', Qt::CaseInsensitive))
            throw Failure{Engine::tr("Cannot copy a folder into itself: %1").arg(path)};
        return source.left(source.lastIndexOf('/') + 1).compare(childPath(targetParent, {}), Qt::CaseInsensitive) == 0;
    }

    std::optional<QSet<QString>> listConflictNames(bool remote, const QString& folder, qsizetype count)
    {
        if (!remote || count < 2) return std::nullopt;
        try {
            QSet<QString> names;
            for (const auto& item : list(true, folder)) names.insert(item.name.toCaseFolded());
            return names;
        } catch (const Failure& error) {
            if (error.code != 413) throw;
            // Large directories can still be queried by name.
            return std::nullopt;
        }
    }

    std::optional<TransferEntry> destinationEntry(bool remote, const QString& folder, const QString& name)
    {
        const auto entries = list(remote, folder, name);
        // Older hosts return the whole directory, even for a single-name lookup.
        const auto found = std::find_if(entries.cbegin(), entries.cend(), [&](const TransferEntry& file) {
            return file.name.compare(name, Qt::CaseInsensitive) == 0;
        });
        if (found == entries.cend()) return std::nullopt;
        return *found;
    }

    ObservedConflict describeConflict(bool remote, const TransferEntry& entry, const QString& path,
                                      const TransferEntry& existing, bool sameSide)
    {
        const auto& version = existing.replaceVersion;
        QString reason;
        if (sameSide && QDir::cleanPath(entry.path).compare(QDir::cleanPath(path), Qt::CaseInsensitive) == 0)
            reason = Engine::tr("The source and destination are the same file.");
        else if (existing.directory)
            reason = Engine::tr("A folder occupies this name.");
        else if (version.isNull() && remote)
            reason = Engine::tr("Replacing remote files requires an updated Sunshine host with file deletion enabled.");
        else if (version.isEmpty())
            reason = Engine::tr("The file changed or is in use: %1").arg(path);
        return {{path, reason}, entry.path, reason.isEmpty() ? version : QString{}};
    }

    void collectConflicts(bool fromRemote, bool toRemote, const TransferEntry& entry, const QString& folder,
                          QList<ObservedConflict>& observed, int depth = 0)
    {
        if (!safeName(entry.name) || depth > 128) return;
        if (!fromRemote) {
            const QFileInfo source(entry.path);
            if (!source.exists() || source.isSymLink() || source.isJunction() || source.isDir() != entry.directory) return;
        }
        const auto existing = destinationEntry(toRemote, folder, entry.name);
        if (!existing) return;
        const auto target = childPath(folder, entry.name);
        if (!entry.directory) {
            QJsonObject state;
            if (journal && !journal->load(target, state)) throw Failure{Engine::tr("Cannot read transfer checkpoint")};
            if (needsConflictDecision(state))
                observed.append(describeConflict(toRemote, entry, target, *existing, fromRemote == toRemote));
            return;
        }
        if (!depth && fromRemote == toRemote && copyingToSourceParent(fromRemote, entry.path, folder)) {
            observed.append(describeConflict(toRemote, entry, target, *existing, true));
            return;
        }
        if (!existing->directory) return;
        const auto children = list(fromRemote, entry.path);
        const auto names = listConflictNames(toRemote, target, children.size());
        for (const auto& child : children) {
            checkCancelled(cancelled);
            if (!names || names->contains(child.name.toCaseFolded()))
                collectConflicts(fromRemote, toRemote, child, target, observed, depth + 1);
        }
    }

    std::optional<CopyTarget> chooseTarget(bool remote, const TransferEntry& entry, const QString& folder, bool sameSide,
                                          const Progress& report)
    {
        const auto path = childPath(folder, entry.name);
        const auto key = qMakePair(entry.path, path);
        ConflictDecision decision;
        if (conflictDecisions && conflictDecisions->contains(key)) {
            decision = conflictDecisions->take(key);
        } else {
            const auto existing = destinationEntry(remote, folder, entry.name);
            if (!existing) return CopyTarget{path, {}};
            if (!resolveConflicts) throw Failure{Engine::tr("Destination already exists: %1").arg(path)};
            const auto conflict = describeConflict(remote, entry, path, *existing, sameSide);
            const auto choices = resolveConflicts(remote, 1, {conflict.details});
            checkCancelled(cancelled);
            decision = {choices.first(), conflict.replaceVersion};
        }
        if (decision.choice == Engine::ConflictChoice::KeepBoth) {
            finishFiles(report); // Number the copy against all previously published siblings.
            QSet<QString> names;
            names.insert(entry.name.toCaseFolded());
            if (remote) {
                for (const auto& file : list(true, folder)) names.insert(file.name.toCaseFolded());
            } else {
                for (const auto& name : QDir(folder).entryList(QDir::AllEntries | QDir::System | QDir::Hidden | QDir::NoDotAndDotDot))
                    names.insert(name.toCaseFolded());
            }
            return CopyTarget{childPath(folder, availableCopyName(entry.name, names)), {}};
        }
        if (decision.choice == Engine::ConflictChoice::Replace && !decision.replaceVersion.isEmpty())
            return CopyTarget{path, decision.replaceVersion};
        return std::nullopt;
    }

    void deleteFile(bool remote, const TransferEntry& entry)
    {
        if (remote) {
            if (!entry.modified.isValid()) throw Failure{Engine::tr("Source changed: %1").arg(entry.path)};
            exchange(entry.directory ? "rmdir" : "delete", {{"path", entry.path}, {"size", QString::number(entry.size)},
                                {"modified", QString::number(entry.modified.toMSecsSinceEpoch())}}, &empty);
        } else {
            QFile file(entry.path);
            if (!file.moveToTrash()) throw Failure{QStringLiteral("%1\n%2").arg(entry.path, file.errorString())};
        }
    }

    QJsonObject capabilities()
    {
        if (batchCaps && batchCaps->has_value()) return batchCaps->value();
        QJsonObject caps;
        try { caps = QJsonDocument::fromJson(exchange("capabilities", {}, nullptr)).object(); }
        catch (const Failure& e) {
            if (e.code != 400 && e.code != 404 && e.code != 405) throw;
        }
        if (batchCaps) *batchCaps = caps;
        return caps;
    }

    QByteArray exchange(const QString& operation, const Params& params, const QByteArray* body,
                        qint64 limit = 4096, bool cleanup = false)
    {
        // Open a grant for a new task or batch. Later files in a negotiated batch share it.
        if (needsSession && !cleanup) {
            sendRequest("session", {}, &empty, 4096, false);
            needsSession = false;
        }
        try {
            return sendRequest(operation, params, body, limit, cleanup);
        } catch (const Failure& e) {
            // Source hashing can outlast the standalone grant. Renew it before retrying a
            // read or a v2 operation with an idempotent task/lease and explicit offset.
            if (cleanup || e.code != 403 || !request.hasRawHeader("X-Moonlight-File-Session") ||
                operation == "mkdir" || operation == "write" || operation == "delete" || operation == "rmdir") throw;
            sendRequest("session", {}, &empty, 4096, false);
            return sendRequest(operation, params, body, limit, false);
        }
    }

    QByteArray sendRequest(const QString& operation, const Params& params, const QByteArray* body,
                           qint64 limit, bool cleanup)
    {
        if (!cleanup) checkCancelled(cancelled);
        if (certificate.isNull() || host.scheme() != QStringLiteral("https") ||
            request.rawHeader("X-Clipboard-Token").isEmpty()) {
            throw Failure{Engine::tr("File transfer is unavailable for this session")};
        }
        QUrl url = host;
        url.setPath("/files/" + operation);
        QByteArray query;
        for (const auto& param : params) {
            if (!query.isEmpty()) query += '&';
            query += param.first.toLatin1() + '=' + QUrl::toPercentEncoding(param.second);
        }
        url.setQuery(QString::fromLatin1(query), QUrl::StrictMode);
        request.setUrl(url);
        QEventLoop loop;
        auto& network = transferNetwork(networks);
        std::unique_ptr<QNetworkReply> reply(body ? network.post(request, *body) : network.get(request));
        auto* r = reply.get();
        bool invalidResponse = false;
        QObject::connect(r, &QNetworkReply::encrypted, &loop, [this, r, &invalidResponse] {
            if (r->sslConfiguration().peerCertificate() != certificate) {
                invalidResponse = true;
                r->abort();
            }
        });
        QObject::connect(r, &QNetworkReply::sslErrors, r, [this, r](const QList<QSslError>& errors) {
            if (r->sslConfiguration().peerCertificate() == certificate) r->ignoreSslErrors(errors);
        });
        QByteArray data;
        auto drain = [&] {
            if (r->bytesAvailable() > limit - data.size()) {
                invalidResponse = true;
                r->abort();
            } else {
                data.append(r->readAll());
            }
        };
        r->setReadBufferSize(64 * 1024);
        QObject::connect(r, &QIODevice::readyRead, &loop, drain);
        QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer cancelTimer;
        if (!cleanup) {
            QObject::connect(&cancelTimer, &QTimer::timeout, r, [this, r] {
                if (*cancelled) r->abort();
            });
            cancelTimer.start(50);
        }
        const bool hitTest = operation == "drag-source" || operation == "drop-target";
        QTimer::singleShot(cleanup ? 1500 : (hitTest ? 3000 : 60000), r, &QNetworkReply::abort);
        if (!r->isFinished()) loop.exec();
        if (r->bytesAvailable()) drain();
        if (!cleanup) checkCancelled(cancelled);
        int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        lastCode = code;
        nextOffset = r->rawHeader("X-File-Offset");
        if (invalidResponse) throw Failure{Engine::tr("Unexpected host response (HTTP %1)").arg(code), false, code};
        const bool transport = r->error() != QNetworkReply::SslHandshakeFailedError &&
                               (code < 400 || code == 408 || code == 429 || (code >= 500 && code != 507));
        if (operation == "session") {
            if (code == 403 || code == 401)
                throw Failure{Engine::tr("Enable “Browse and transfer files” in Sunshine and check that this client is paired. Then refresh.")};
            if (code == 404 || code == 405 || (code >= 200 && code < 300 && code != 204))
                throw Failure{Engine::tr("Update Sunshine to a version that supports standalone file transfer, then refresh.")};
            if (!code && r->error() != QNetworkReply::NoError)
                throw Failure{Engine::tr("Could not start file transfer. Check the connection and that Sunshine supports standalone file transfer. %1").arg(r->errorString()), transport, code};
        }
        const QString path = params.isEmpty() ? QString() : params.first().second;
        if (code == 409 && (operation == "delete" || operation == "rmdir"))
            throw Failure{Engine::tr("The file changed or is in use: %1").arg(path), false, code};
        if (code == 409) throw Failure{Engine::tr("File changed or destination exists: %1").arg(path), false, code};
        if (code == 403) throw Failure{Engine::tr("Access denied by host"), journal && bytes > 0, code};
        // A refreshed session or an expired upload stage invalidates its previous lease.
        // On retry the worker negotiates a new lease and uses the host's current offset.
        if (code == 404) throw Failure{Engine::tr("File or folder not found: %1").arg(path),
                                       operation == "chunk" || operation == "commit" || operation == "read-pinned", code};
        if (r->error() != QNetworkReply::NoError)
            throw Failure{r->errorString(), transport, code};
        const int expected = operation == "resume" ? 200 : (body ? 204 : 200);
        if (code != expected && !((operation == "identity" || operation == "open-read") && code == 202))
            throw Failure{Engine::tr("Unexpected host response (HTTP %1)").arg(code), false, code};
        return data;
    }

    void openJournal(bool fromRemote, const TransferEntry& entry, const QString& target)
    {
        const auto peer = certificate.digest(QCryptographicHash::Sha256);
        const auto local = request.sslConfiguration().localCertificate().digest(QCryptographicHash::Sha256);
        auto job = std::make_shared<ck::Journal>(peer, local, fromRemote, entry.path, target);
        if (!job->ready())
            throw Failure{Engine::tr("This transfer is already running or its checkpoint is unavailable")};
        journal = std::move(job);
    }

    QJsonObject record(const QString& target)
    {
        QJsonObject state;
        if (!journal->load(target, state)) throw Failure{Engine::tr("Cannot read transfer checkpoint")};
        if (state.isEmpty()) {
            state = QJsonObject{{"target", target}, {"id", ck::nonce()}};
            save(state);
        }
        if (!ck::validHex(state.value("id").toString(), 32))
            throw Failure{Engine::tr("Cannot read transfer checkpoint")};
        return state;
    }

    void save(const QJsonObject& state)
    {
        if (!journal->store(state.value("target").toString(), state)) throw Failure{Engine::tr("Cannot save transfer checkpoint")};
    }

    bool createJobDirectory(bool fromRemote, const QString& parent, const QString& name, const QString& target)
    {
        auto state = record(target);
        std::vector<ck::Handle> pinned;
        if (fromRemote && !ck::pinDirectory(parent, pinned))
            throw Failure{Engine::tr("Cannot open folder: %1").arg(parent)};
        if (state.value("created").toBool()) {
            if (fromRemote) {
                if (!ck::pinDirectory(target, pinned)) throw Failure{Engine::tr("Cannot open folder: %1").arg(target)};
                BY_HANDLE_FILE_INFORMATION info{};
                if (!GetFileInformationByHandle(pinned.back().get(), &info) ||
                    ck::identity(info) != state.value("identity").toString())
                    throw Failure{Engine::tr("Destination already exists: %1").arg(target)};
            }
            return false;
        }
        const bool created = makeDirectory(!fromRemote, parent, name, true);
        if (fromRemote) {
            BY_HANDLE_FILE_INFORMATION info{};
            if (!ck::pinDirectory(target, pinned) || !GetFileInformationByHandle(pinned.back().get(), &info))
                throw Failure{Engine::tr("Cannot open folder: %1").arg(target)};
            state.insert("identity", ck::identity(info));
        }
        state.insert("created", true);
        save(state);
        return created;
    }

    void clean(bool fromRemote, const TransferEntry& entry, const QString& root)
    {
        bool grantAttempted = false;
        auto openGrant = [&] {
            if (needsSession && !grantAttempted) {
                grantAttempted = true;
                try {
                    sendRequest("session", {}, &empty, 4096, true);
                    needsSession = false;
                } catch (const Failure&) {
                }
            }
            return !needsSession;
        };
        journal->each([&](const QJsonObject& state) {
            auto target = state.value("target").toString();
            const auto id = state.value("id").toString();
            if (!ck::validHex(id, 32) || state.value("done").toBool() || state.value("created").toBool() ||
                target != QDir::cleanPath(target) ||
                (target != root && !(entry.directory && target.startsWith(root + '/'))))
                return;
            const auto copyName = state.value("copyName").toString();
            if (!copyName.isEmpty()) {
                if (!safeName(copyName)) return;
                target = childPath(QFileInfo(target).absolutePath(), copyName);
            }
            if (fromRemote) {
                std::vector<ck::Handle> pinned;
                if (!state.value("identity").toString().isEmpty() &&
                    ck::pinDirectory(QFileInfo(target).absolutePath(), pinned)) {
                    if (auto h = ck::openPart(target, state, true)) ck::disposition(h.get(), true);
                }
                if (openGrant()) {
                    try {
                        exchange("abort", {{"id", state.value("readId").toString(id)}}, &empty, 4096, true);
                    } catch (const Failure&) {
                    }
                }
            } else if (openGrant()) {
                try {
                    exchange("discard", {{"path", target}, {"id", id}, {"sha256", state.value("sha256").toString()}},
                             &empty, 4096, true);
                } catch (const Failure&) { /* A new transfer gets a fresh ID. Host expiry also removes this stage. */
                }
            }
        });
        journal->clear();
    }

    QString remoteFingerprint(const TransferEntry& entry, const QString& id, bool pin = false)
    {
        Params params{{"path", entry.path}, {"size", QString::number(entry.size)}, {"id", id}};
        if (entry.modified.isValid()) params.append({"modified", QString::number(entry.modified.toMSecsSinceEpoch())});
        qint64 hashed = -1;
        for (;;) {
            const auto obj = QJsonDocument::fromJson(exchange(pin ? "open-read" : "identity", params, nullptr)).object();
            if (lastCode == 200) {
                const auto sha = obj.value("sha256").toString();
                if (obj.value("size").toString() == QString::number(entry.size) && ck::validHex(sha, 64)) return sha;
                break;
            }
            bool ok = false;
            const auto next = obj.value("offset").toString().toLongLong(&ok);
            if (!ok || next <= hashed || next > entry.size) break;
            hashed = next;
        }
        throw Failure{Engine::tr("Invalid transfer checkpoint from host")};
    }

    bool hashLocal(HANDLE file, qint64 size, QCryptographicHash& hash, const std::function<void()>& renew)
    {
        QElapsedTimer timer;
        timer.start();
        return transfer_file::hashBytes(file, size, hash, [&] {
            if (*cancelled) return true;
            // Hashing a large local file or a resumed prefix can outlast the host's idle timeout.
            if (backgroundTransfers && timer.hasExpired(5 * 60 * 1000)) {
                renew();
                timer.restart();
            }
            return false;
        });
    }

    void resumeUpload(const TransferEntry& entry, const CopyTarget& destination, QJsonObject state, const Progress& report)
    {
        const auto& target = destination.path;
        const Failure changed{Engine::tr("Source changed: %1").arg(entry.path)};
        std::vector<ck::Handle> parents;
        if (!ck::pinDirectory(QFileInfo(entry.path).absolutePath(), parents)) throw changed;
        auto h = ck::open(entry.path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN);
        BY_HANDLE_FILE_INFORMATION info{};
        LARGE_INTEGER length{};
        if (!h || !ck::regular(h.get(), info) || !GetFileSizeEx(h.get(), &length) || length.QuadPart != entry.size)
            throw changed;
        auto id = state.value("id").toString();
        const auto previous = state.value("sha256").toString();
        Params params{{"path", target}, {"size", QString::number(entry.size)}};
        if (!destination.replaceVersion.isEmpty()) params.append({"replace", destination.replaceVersion});
        // For a fresh single-chunk file, resume itself reserves the path immediately after hashing.
        const bool prepare = backgroundTransfers && !state.value("done").toBool() &&
                             (entry.size > ChunkSize || !previous.isEmpty());
        // Reserve the exact destination before hashing a potentially large local file.
        const auto reserve = [&] {
            if (!prepare) return;
            auto reservation = params;
            reservation.append({"id", id});
            if (!previous.isEmpty()) reservation.append({"sha256", previous});
            exchange("prepare-upload", reservation, &empty);
        };
        reserve();
        QCryptographicHash hasher(QCryptographicHash::Sha256);
        if (!hashLocal(h.get(), entry.size, hasher, reserve)) {
            checkCancelled(cancelled);
            throw changed;
        }
        const QString sha = QString::fromLatin1(hasher.result().toHex());
        if (!previous.isEmpty() && previous != sha) {
            if (state.value("done").toBool()) throw changed;
            exchange("discard", {{"path", target}, {"id", state.value("id").toString()}, {"sha256", previous}}, &empty);
            id = ck::nonce();
            state.insert("id", id);
        }
        state.insert("sha256", sha);
        save(state);
        if (state.value("done").toBool()) {
            TransferEntry remote{entry.name, target, false, entry.size, {}};
            if (remoteFingerprint(remote, id) != sha)
                throw Failure{Engine::tr("Destination already exists: %1").arg(target)};
            bytes += entry.size;
            report(bytes);
            return;
        }
        params.append({"id", id});
        params.append({"sha256", sha});
        const auto reply = QJsonDocument::fromJson(exchange("resume", params, &empty)).object();
        const auto lease = reply.value("id").toString();
        bool ok = false;
        qint64 offset = reply.value("offset").toString().toLongLong(&ok);
        if (!ok || !ck::validHex(lease, 32) || !reply.value("complete").isBool() || offset < 0 || offset > entry.size ||
            (reply.value("complete").toBool() && offset != entry.size) || !transfer_file::seek(h.get(), offset))
            throw Failure{Engine::tr("Invalid transfer checkpoint from host")};
        bytes += offset;
        report(bytes);
        while (offset < entry.size) {
            checkCancelled(cancelled);
            const DWORD count = static_cast<DWORD>(std::min(ChunkSize, entry.size - offset));
            QByteArray block(count, Qt::Uninitialized);
            DWORD read = 0;
            if (!ReadFile(h.get(), block.data(), count, &read, nullptr) || read != count) throw changed;
            try {
                exchange("chunk",
                         {{"path", target},
                          {"id", lease},
                          {"size", QString::number(entry.size)},
                          {"offset", QString::number(offset)}},
                         &block);
            } catch (const Failure& error) {
                // Qt may replay a POST whose success response was lost. Each lease is exclusive;
                // only an exact acknowledgement for the same pinned bytes is safe to accept.
                if (error.code != 409 || nextOffset != QByteArray::number(offset + count)) throw;
            }
            if (nextOffset != QByteArray::number(offset + count))
                throw Failure{Engine::tr("Invalid transfer checkpoint from host")};
            offset += count;
            bytes += count;
            wireBytes += count;
            report(bytes);
        }
        if (!reply.value("complete").toBool()) {
            Params commit{{"path", target}, {"id", lease}};
            if (!destination.replaceVersion.isEmpty()) commit.append({"replace", destination.replaceVersion});
            exchange("commit", commit, &empty);
        }
        state.insert("done", true);
        save(state);
    }

    void withDownloadSource(const TransferEntry& entry, const QString& id, bool resumable,
                            const std::function<void(const QString&, const QString&)>& receive)
    {
        if (!ck::validHex(id, 32)) throw Failure{Engine::tr("Cannot read transfer checkpoint")};
        const auto readId = backgroundTransfers ? id : QString{};
        // The source stays pinned through hashing and every offset read, even if the host locks.
        auto release = qScopeGuard([&] {
            if (readId.isEmpty()) return;
            try { exchange("abort", {{"id", readId}}, &empty, 4096, true); }
            catch (const Failure&) { /* The host also expires abandoned source handles. */ }
        });
        try {
            const auto sha = remoteFingerprint(entry, id, backgroundTransfers);
            receive(sha, readId);
        } catch (const Failure& error) {
            if (resumable && error.interrupted && !*cancelled) release.dismiss();
            throw;
        }
    }

    void resumeDownload(const TransferEntry& entry, const CopyTarget& destination, QJsonObject state, const Progress& report)
    {
        const auto id = state.value("id").toString();
        withDownloadSource(entry, backgroundTransfers ? state.value("readId").toString(id) : id, true,
                           [&](const QString& sha, const QString& readId) {
                               receiveDownload(entry, destination, std::move(state), report, sha, readId);
                           });
    }

    void receiveDownload(const TransferEntry& entry, const CopyTarget& destination, QJsonObject state, const Progress& report,
                         const QString& sha, const QString& readId)
    {
        const auto& target = destination.path;
        const auto renew = [&] {
            if (remoteFingerprint(entry, readId, true) != sha) throw Failure{Engine::tr("Source changed: %1").arg(entry.path)};
        };
        std::vector<ck::Handle> parents;
        if (!ck::pinDirectory(QFileInfo(target).absolutePath(), parents))
            throw Failure{Engine::tr("Cannot open folder: %1").arg(target)};
        const Failure conflict{Engine::tr("Destination already exists: %1").arg(target)};
        // Recognize our own completed file even if the process stopped between rename and saving its receipt.
        if (QFileInfo::exists(target) &&
            (destination.replaceVersion.isEmpty() || localFileVersion(target) != destination.replaceVersion)) {
            auto h = ck::open(target, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN);
            BY_HANDLE_FILE_INFORMATION info{};
            LARGE_INTEGER size{};
            if (!h || !ck::regular(h.get(), info) || !GetFileSizeEx(h.get(), &size) || size.QuadPart != entry.size ||
                ck::identity(info) != state.value("identity").toString() || state.value("sha256") != sha)
                throw conflict;
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (!hashLocal(h.get(), entry.size, hash, renew)) {
                checkCancelled(cancelled);
                throw conflict;
            }
            if (QString::fromLatin1(hash.result().toHex()) != sha) throw conflict;
            state.insert("done", true);
            save(state);
            bytes += entry.size;
            report(bytes);
            return;
        }
        const bool knownPart = !state.value("identity").toString().isEmpty() && !state.value("done").toBool();
        auto part = knownPart ? ck::openPart(target, state, true) : ck::Handle{};
        if (knownPart && !part && QFileInfo::exists(ck::partPath(target, state.value("id").toString())))
            throw Failure{Engine::tr("The partial download changed: %1").arg(target)};
        if (!part) {
            if (!state.value("identity").toString().isEmpty()) {
                // A new local partial file gets its own name. Retries still refer to the original host handle.
                if (!readId.isEmpty()) state.insert("readId", readId);
                state.insert("id", ck::nonce());
            }
            state.remove("done");
            part = ck::openPart(target, state, false);
            BY_HANDLE_FILE_INFORMATION info{};
            if (!part || !GetFileInformationByHandle(part.get(), &info) || !ck::disposition(part.get(), true))
                throw Failure{Engine::tr("Cannot create partial download: %1").arg(target)};
            state.insert("identity", ck::identity(info));
            state.insert("sha256", sha);
            save(state);
            if (!ck::disposition(part.get(), false)) throw Failure{Engine::tr("Cannot save transfer checkpoint")};
        }
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(part.get(), &size)) throw Failure{Engine::tr("Cannot read transfer checkpoint")};
        qint64 offset = size.QuadPart;
        if (state.value("sha256") != sha || offset > entry.size) {
            if (!transfer_file::seek(part.get(), 0) || !SetEndOfFile(part.get()))
                throw Failure{Engine::tr("Cannot save transfer checkpoint")};
            state.insert("sha256", sha);
            save(state);
            offset = 0;
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!transfer_file::seek(part.get(), 0) ||
            !hashLocal(part.get(), offset, hash, renew)) {
            checkCancelled(cancelled);
            throw Failure{Engine::tr("Cannot read transfer checkpoint")};
        }
        bytes += offset;
        report(bytes);
        while (offset < entry.size) {
            const auto block = readBlock(entry, offset, std::min(ChunkSize, entry.size - offset), readId);
            DWORD wrote = 0;
            if (!WriteFile(part.get(), block.constData(), static_cast<DWORD>(block.size()), &wrote, nullptr) ||
                wrote != block.size())
                throw Failure{Engine::tr("Cannot write partial download: %1").arg(target)};
            hash.addData(block);
            offset += wrote;
            bytes += wrote;
            wireBytes += wrote;
            report(bytes);
        }
        checkCancelled(cancelled);
        if (QString::fromLatin1(hash.result().toHex()) != sha)
            throw Failure{Engine::tr("File verification failed: %1").arg(entry.path)};
        if (entry.modified.isValid()) {
            const auto ticks = (entry.modified.toMSecsSinceEpoch() + 11644473600000LL) * 10000ULL;
            const FILETIME t{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
            SetFileTime(part.get(), nullptr, nullptr, &t);
        }
        if (!FlushFileBuffers(part.get())) throw Failure{Engine::tr("Cannot write partial download: %1").arg(target)};
        checkCancelled(cancelled);
        if (!destination.replaceVersion.isEmpty()) checkReplacement(destination);
        if (!ck::renamePart(part.get(), target, !destination.replaceVersion.isEmpty()))
            throw Failure{Engine::tr("Could not publish download; check the destination: %1").arg(target)};
        state.insert("done", true);
        save(state);
    }

    QByteArray readBlock(const TransferEntry& entry, qint64 offset, qint64 count, const QString& id = {})
    {
        Params params{{"path", entry.path}, {"offset", QString::number(offset)}, {"size", QString::number(entry.size)}};
        if (!id.isEmpty()) params.append({"id", id});
        if (entry.modified.isValid()) params.append({"modified", QString::number(entry.modified.toMSecsSinceEpoch())});
        auto block = exchange(id.isEmpty() ? "read" : "read-pinned", params, nullptr, ChunkSize);
        if (block.size() != count) throw Failure{Engine::tr("Incomplete data from host: %1").arg(entry.path), bool(journal)};
        return block;
    }

    void download(const TransferEntry& entry, const QString& destination, const Progress& report,
                  const QString& readId = {}, const QString& sha = {})
    {
        QTemporaryFile output(QFileInfo(destination).dir().filePath(".moonlight-XXXXXX.part"));
        if (!output.open()) throw Failure{output.errorString()};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        qint64 offset = 0;
        do {
            const auto block = readBlock(entry, offset, std::min(ChunkSize, entry.size - offset), readId);
            if (output.write(block) != block.size()) throw Failure{output.errorString()};
            if (!readId.isEmpty()) hash.addData(block);
            bytes += block.size();
            wireBytes += block.size();
            offset += block.size();
            report(bytes);
        } while (offset < entry.size);
        checkCancelled(cancelled);
        if (!readId.isEmpty() && QString::fromLatin1(hash.result().toHex()) != sha)
            throw Failure{Engine::tr("File verification failed: %1").arg(entry.path)};
        if (!output.flush()) throw Failure{output.errorString()};
        if (entry.modified.isValid()) output.setFileTime(entry.modified, QFileDevice::FileModificationTime);
        if (!output.rename(destination)) throw Failure{output.errorString()};
        output.setAutoRemove(false);
    }

    void upload(const TransferEntry& entry, const CopyTarget& destination, const Progress& report)
    {
        QFile input(entry.path);
        if (!input.open(QIODevice::ReadOnly)) throw Failure{input.errorString()};
        const qint64 size = input.size();
        const auto modified = input.fileTime(QFileDevice::FileModificationTime);
        if (size != entry.size || (entry.modified.isValid() && modified != entry.modified))
            throw Failure{Engine::tr("Source changed: %1").arg(entry.path)};
        writeChunks(destination, size, [&](qint64, qint64 count) {
            auto block = input.read(count);
            if (block.size() != count || input.size() != size || input.fileTime(QFileDevice::FileModificationTime) != modified)
                throw Failure{Engine::tr("Source changed: %1").arg(entry.path)};
            return block;
        }, report);
    }

    template<class Read>
    void writeChunks(const CopyTarget& destination, qint64 size, Read read, const Progress& report)
    {
        const auto id = QUuid::createUuid().toString(QUuid::Id128);
        qint64 offset = 0;
        bool attempted = false;
        auto cleanup = qScopeGuard([&] {
            if (attempted) {
                try { exchange("abort", {{"id", id}}, &empty, 4096, true); }
                catch (const Failure&) { /* Stream teardown also discards abandoned uploads. */ }
            }
        });
        do {
            checkCancelled(cancelled);
            const qint64 count = std::min(ChunkSize, size - offset);
            const auto block = read(offset, count);
            attempted = true;
            Params params{{"path", destination.path}, {"id", id}, {"offset", QString::number(offset)},
                          {"size", QString::number(size)}};
            if (!destination.replaceVersion.isEmpty()) params.append({"replace", destination.replaceVersion});
            exchange("write", params, &block);
            offset += count;
            bytes += count;
            wireBytes += count;
            report(bytes);
        } while (offset < size);
        cleanup.dismiss();
    }

    std::shared_ptr<ck::Journal> journal;
    int lastCode = 0;
    QByteArray nextOffset;
    Networks& networks;
    QUrl host;
    QSslCertificate certificate;
    Flag cancelled;
    bool needsSession;
    bool backgroundTransfers = false;
    ConflictResolver resolveConflicts;
    ConflictDecisions* conflictDecisions;
    std::optional<QJsonObject>* batchCaps;
    QThreadPool* filePool = nullptr;
    QNetworkRequest request;
    const QByteArray empty;
};
}

struct FileTransferEngine::Batch {
    std::atomic_bool prepared{false};
    ConflictDecisions decisions;
    std::optional<QJsonObject> capabilities;
};

QByteArray FileTransferEngine::newSessionToken()
{
    unsigned char secret[32];
    if (RAND_bytes(secret, sizeof(secret)) != 1) return {};
    return QByteArray(reinterpret_cast<const char*>(secret), sizeof(secret)).toHex();
}

FileTransferEngine::FileTransferEngine(QUrl host, QSslCertificate certificate, QByteArray token, QObject* parent,
                                       bool standalone, ConflictPrompt prompt)
    : QObject(parent), m_Host(std::move(host)), m_Certificate(std::move(certificate)), m_Token(std::move(token)),
      m_Standalone(standalone), m_ConflictPrompt(std::move(prompt)), m_Ssl(IdentityManager::get()->getSslConfig())
{
    m_Ssl.setCaCertificates({m_Certificate});
    m_Lists.setMaxThreadCount(2);
    m_Transfers.setMaxThreadCount(1);
    m_SmallFiles.setMaxThreadCount(SmallFileConcurrency);
}

QList<FileTransferEngine::ConflictChoice> FileTransferEngine::askFileConflicts(const Flag& cancelled, bool remote, int itemCount,
                                                                            const QList<FileConflict>& conflicts)
{
    if (!m_ConflictPrompt) throw Failure{tr("Destination already exists: %1").arg(conflicts.first().path)};
    auto answer = std::make_shared<std::promise<QList<ConflictChoice>>>();
    auto result = answer->get_future();
    QMetaObject::invokeMethod(this, [this, remote, itemCount, conflicts, answer, cancelled] {
        if (!*cancelled) {
            m_ConflictPrompt(remote, itemCount, conflicts, [answer](QList<ConflictChoice> choices) {
                answer->set_value(std::move(choices));
            });
        }
    }, Qt::QueuedConnection);
    // The GUI thread joins this worker during shutdown, even if the question is still open.
    while (result.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) checkCancelled(cancelled);
    auto choices = result.get();
    if (choices.size() != conflicts.size()) throw Failure{tr("Cancelled")};
    return choices;
}

FileTransferEngine::~FileTransferEngine()
{
    *m_WindowClosing = true;
    for (const auto& cancel : m_ListCancel) if (cancel) *cancel = true;
    if (m_ActiveCancel) *m_ActiveCancel = true;
    m_Lists.clear();
    m_Lists.waitForDone();
    if (m_Standalone) {
        m_Transfers.start([this] {
            Worker worker(m_Networks, m_Host, m_Ssl, m_Token, std::make_shared<std::atomic_bool>(false), true);
            worker.endSession();
        });
    }
    m_Transfers.waitForDone();
}

void FileTransferEngine::list(bool remote, QString path)
{
    auto& current = m_ListCancel[int(remote)];
    if (current) *current = true;
    current = std::make_shared<std::atomic_bool>(false);
    m_Lists.start([this, remote, path = std::move(path), flag = current] {
        QList<TransferEntry> entries;
        QString error;
        bool waitingForDesktop = false;
        try {
            checkCancelled(flag);
            Worker worker(m_Networks, m_Host, m_Ssl, m_Token, flag, m_Standalone);
            entries = worker.browse(remote, path, waitingForDesktop);
        } catch (const Failure& failure) {
            error = failure.message;
        }
        QMetaObject::invokeMethod(this, [this, remote, path, entries = std::move(entries), error, waitingForDesktop, flag] {
            if (!*flag) emit listed(remote, path, entries, error, waitingForDesktop);
        }, Qt::QueuedConnection);
    });
}

QList<TransferEntry> FileTransferEngine::localDropEntries(const QList<QUrl>& urls)
{
    QList<TransferEntry> entries;
    QSet<QString> seen;
    for (const auto& url : urls) {
        if (!url.isLocalFile()) return {};
        const QFileInfo info(url.toLocalFile());
        if (!info.isAbsolute() || info.isRoot() || info.isSymLink() || info.isJunction() ||
            (!info.isFile() && !info.isDir()) || !safeName(info.fileName())) return {};
        const auto key = info.absoluteFilePath().toCaseFolded();
        if (seen.contains(key)) continue;
        seen.insert(key);
        entries.append({info.fileName(), info.absoluteFilePath(), info.isDir(),
                        info.isDir() ? 0 : info.size(), info.lastModified()});
    }
    return entries;
}

std::future<FileDragLocation> FileTransferEngine::remoteDragLocation(bool source, QPoint position, QList<QUrl> localFiles)
{
    std::packaged_task<FileDragLocation()> task([this, source, position, localFiles = std::move(localFiles)] {
        try {
            Worker worker(m_Networks, m_Host, m_Ssl, m_Token, m_WindowClosing, false);
            auto location = worker.dragLocation(source, position);
            if (!source && !location.folder.isEmpty()) {
                location.entries = localDropEntries(localFiles);
                if (location.entries.isEmpty()) location.error = tr("The dragged selection contains an unavailable file or folder.");
            }
            return location;
        } catch (const Failure& failure) {
            return FileDragLocation{{}, {}, failure.message};
        }
    });
    auto result = task.get_future();
    m_Lists.start(std::move(task));
    return result;
}

std::future<QString> FileTransferEngine::localDropFolder(QPoint screenPoint, quintptr window, std::function<void(QString)> finished)
{
    std::packaged_task<QString()> task([this, screenPoint, window, finished = std::move(finished)] {
        QString folder;
        POINT point {screenPoint.x(), screenPoint.y()};
        if (WindowFromPhysicalPoint(point) == reinterpret_cast<HWND>(window)) {
            const auto paths = shell_file_drop::paths_at(point, false);
            if (!paths.empty()) folder = QDir::fromNativeSeparators(QString::fromStdWString(paths.front()));
        }
        if (finished) QMetaObject::invokeMethod(this, [finished, folder] { finished(folder); }, Qt::QueuedConnection);
        return folder;
    });
    auto result = task.get_future();
    m_Lists.start(std::move(task));
    return result;
}

void FileTransferEngine::loadPlaces(bool remote)
{
    m_Lists.start([this, remote] {
        QList<TransferPlace> locations;
        QString error;
        try {
            Worker worker(m_Networks, m_Host, m_Ssl, m_Token, m_WindowClosing, m_Standalone);
            locations = worker.places(remote);
        } catch (const Failure& failure) { error = failure.message; }
        QMetaObject::invokeMethod(this, [this, remote, locations, error] {
            emit placesLoaded(remote, locations, error);
        }, Qt::QueuedConnection);
    });
}

void FileTransferEngine::makeDirectory(bool remote, QString parent, QString name)
{
    m_Lists.start([this, remote, parent, name] {
        QString error;
        try {
            Worker worker(m_Networks, m_Host, m_Ssl, m_Token, m_WindowClosing, m_Standalone);
            worker.makeDirectory(remote, parent, name);
        } catch (const Failure& failure) { error = failure.message; }
        QMetaObject::invokeMethod(this, [this, remote, parent, error] {
            emit directoryCreated(remote, parent, error);
        }, Qt::QueuedConnection);
    });
}

void FileTransferEngine::manageFiles(FileAction action, bool remote, QList<TransferEntry> entries, QString folder,
                                     std::function<void(QString)> finished)
{
    m_Transfers.start([this, action, remote, entries = std::move(entries), folder = std::move(folder), finished = std::move(finished)] {
        QString error;
        try {
            ConflictDecisions decisions;
            Worker worker(m_Networks, m_Host, m_Ssl, m_Token, m_WindowClosing, m_Standalone,
                          [this](bool side, int count, const QList<FileConflict>& conflicts) {
                              return askFileConflicts(m_WindowClosing, side, count, conflicts);
                          }, m_ConflictPrompt ? &decisions : nullptr);
            worker.manageFiles(action, remote, entries, folder);
        } catch (const Failure& failure) { error = failure.message; }
        QMetaObject::invokeMethod(this, [finished, error] { finished(error); }, Qt::QueuedConnection);
    });
}

QList<int> FileTransferEngine::enqueue(bool fromRemote, const QList<TransferEntry>& entries, const QString& destinationFolder)
{
    const auto batch = std::make_shared<Batch>();
    QList<int> ids;
    for (const auto& entry : entries) {
        const int id = m_NextId++;
        m_Pending.enqueue({id, fromRemote, entry, destinationFolder, batch});
        ids.append(id);
    }
    startNext();
    return ids;
}

void FileTransferEngine::startNext()
{
    if (m_Active || m_Pending.isEmpty()) return;
    const auto job = m_Pending.dequeue();
    QList<TransferEntry> batchEntries;
    if (!job.batch->prepared) {
        batchEntries.append(job.entry);
        for (const auto& pending : m_Pending) {
            if (pending.batch != job.batch) break;
            batchEntries.append(pending.entry);
        }
    }
    m_Active = job.id;
    m_ActiveCancel = std::make_shared<std::atomic_bool>(false);
    m_Transfers.start([this, job, batchEntries = std::move(batchEntries), flag = m_ActiveCancel] {
        const auto total = job.entry.directory ? -1 : job.entry.size;
        QElapsedTimer timer;
        Worker worker(m_Networks, m_Host, m_Ssl, m_Token, flag, m_Standalone,
                      [this, flag](bool side, int count, const QList<FileConflict>& conflicts) {
                          return askFileConflicts(flag, side, count, conflicts);
                      }, m_ConflictPrompt ? &job.batch->decisions : nullptr, &job.batch->capabilities);
        auto update = [&](qint64 bytes) {
            if (timer.isValid() && timer.elapsed() < 100) return;
            timer.start();
            QMetaObject::invokeMethod(this, [this, id = job.id, bytes, total, wire = worker.wireBytes] {
                emit networkProgress(id, wire);
                emit progress(id, bytes, total, tr("Transferring"), false);
            }, Qt::QueuedConnection);
        };
        update(0);
        bool paused = false;
        std::shared_ptr<ck::Journal> receipt;
        QString result = tr("Completed");
        bool success = true;
        try {
            if (!batchEntries.isEmpty()) {
                worker.prepareConflicts(job.fromRemote, !job.fromRemote, batchEntries, job.destination);
                job.batch->prepared = true;
            }
            receipt = worker.copy(job.fromRemote, job.entry, job.destination, update, m_WindowClosing, m_SmallFiles);
            if (worker.skipped) result = job.entry.directory ? tr("Completed (%1 skipped)").arg(worker.skipped) : tr("Skipped");
        } catch (const Failure& failure) {
            result = failure.message;
            paused = failure.interrupted && !*flag;
            success = false;
        }
        QMetaObject::invokeMethod(this, [this, id = job.id, bytes = worker.bytes, total, result, success, paused,
                                         wire = worker.wireBytes, receipt = std::move(receipt)]() mutable {
            emit networkProgress(id, wire);
            if (paused) emit interrupted(id);
            emit progress(id, bytes, success && total < 0 ? bytes : total, result, true);
            if (receipt) m_Transfers.start([receipt = std::move(receipt)] { receipt->clear(); });
            m_Active = 0;
            m_ActiveCancel.reset();
            startNext();
        }, Qt::QueuedConnection);
    });
}

void FileTransferEngine::cancel(int id)
{
    if (id == m_Active && m_ActiveCancel) {
        *m_ActiveCancel = true;
        return;
    }
    for (int i = 0; i < m_Pending.size(); ++i) {
        if (m_Pending.at(i).id == id) {
            const auto job = m_Pending.takeAt(i);
            discard(job.fromRemote, job.entry, job.destination, [this, id, total = job.entry.directory ? -1 : job.entry.size] {
                emit progress(id, 0, total, tr("Cancelled"), true);
            });
            break;
        }
    }
}

void FileTransferEngine::discard(bool fromRemote, TransferEntry entry, QString destinationFolder,
                                 std::function<void()> finished)
{
    m_Transfers.start([this, fromRemote, entry = std::move(entry), destinationFolder = std::move(destinationFolder),
                       finished = std::move(finished)] {
        Worker worker(m_Networks, m_Host, m_Ssl, m_Token, m_WindowClosing, m_Standalone);
        try { worker.discard(fromRemote, entry, destinationFolder); } catch (const Failure&) {}
        if (finished) QMetaObject::invokeMethod(this, finished, Qt::QueuedConnection);
    });
}
