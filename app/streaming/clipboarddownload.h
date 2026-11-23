#pragma once

#include "clipboard_tree.h"
#include "clipboarduploadprogress.h"
#include "fileconflict.h"
#include "filetransfercheckpoint.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <limits>
#include <functional>

namespace clipboard_download {
constexpr quint64 ChunkBytes = 1024 * 1024;

// A numbered name is only used after the user chose to keep both files.
template<typename Cancelled>
int publishCopy(const win_clipboard::fs::path& path, const win_clipboard::fs::path& root, Cancelled cancelled)
{
    const auto name = path.filename();
    for (unsigned suffix = 2; suffix <= 10000; ++suffix) {
        if (cancelled()) return 409;
        const auto counter = L" (" + std::to_wstring(suffix) + L")";
        auto extension = name.extension().wstring().substr(0, 254 - counter.size());
        auto stem = name.stem().wstring().substr(0, 255 - counter.size() - extension.size());
        if (!extension.empty() && IS_HIGH_SURROGATE(extension.back())) extension.pop_back();
        if (!stem.empty() && IS_HIGH_SURROGATE(stem.back())) stem.pop_back();
        const auto target = root / (stem + counter + extension);
        if (MoveFileW(path.c_str(), target.c_str())) return 200;
        const DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS &&
            GetFileAttributesW(target.c_str()) == INVALID_FILE_ATTRIBUTES) return 500;
    }
    return 500;
}

struct FolderTarget {
    std::optional<FileConflictChoice> choice;
    QString version;
};

struct File {
    win_clipboard::fs::path name;
    quint64 size;
    bool directory;
    FolderTarget target;
};

inline int parseFiles(const QJsonArray& entries, std::vector<File>& files, quint64& required)
{
    if (entries.isEmpty() || entries.size() > static_cast<qsizetype>(win_clipboard::tree::max_entries)) return 400;
    win_clipboard::tree::layout layout;
    for (const auto& value : entries) {
        const auto entry = value.toObject();
        const auto name = entry.value("name").toString().toStdWString();
        const auto length = entry.value("size").toString();
        bool valid, directory;
        const auto size = length.toULongLong(&valid);
        win_clipboard::fs::path relative;
        if (!valid || QString::number(size) != length || !layout.add(name, relative, directory) ||
            (directory && size) || size > static_cast<quint64>((std::numeric_limits<LONGLONG>::max)())) return 400;
        if (size > (std::numeric_limits<quint64>::max)() - required) return 507;
        required += size;
        files.push_back({std::move(relative), size, directory, {}});
    }
    return 200;
}

class FolderConflicts {
public:
    int inspect(const win_clipboard::fs::path& target, FolderTarget& result)
    {
        result = {};
        const DWORD attributes = GetFileAttributesW(target.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) return GetLastError() == ERROR_FILE_NOT_FOUND ? 200 : 500;
        const auto path = QString::fromStdWString(target.native());
        result.version = transfer_checkpoint::localFileVersion(path);
        QString reason;
        if (attributes & FILE_ATTRIBUTE_DIRECTORY)
            reason = QCoreApplication::translate("FileTransferEngine", "A folder occupies this name.");
        else if (result.version.isEmpty())
            reason = QCoreApplication::translate("FileTransferEngine", "The file changed or is in use: %1").arg(path);
        m_Files.append({path, reason});
        m_Targets.append(&result);
        return 200;
    }

    int choose(int itemCount, const ClipboardConflictPrompt& prompt) const
    {
        if (m_Files.isEmpty()) return 200;
        if (!prompt) return 409;
        const auto choices = prompt(itemCount, m_Files);
        if (!choices || choices->size() != m_Files.size()) return 500;
        for (int i = 0; i < m_Files.size(); ++i) {
            const auto choice = choices->at(i);
            if (choice == FileConflictChoice::Replace && !m_Files[i].replaceUnavailable.isEmpty()) return 500;
            m_Targets[i]->choice = choice;
        }
        return 200;
    }

private:
    QList<FileConflict> m_Files;
    QList<FolderTarget*> m_Targets;
};

// The old file remains in place until the staged download is complete. A new
// collision or a changed replacement target needs a fresh decision.
template<typename Cancelled>
int publishFile(const win_clipboard::fs::path& staged, const win_clipboard::fs::path& root,
                FolderTarget& target, int itemCount, const ClipboardConflictPrompt& prompt, Cancelled cancelled)
{
    const auto path = root / staged.filename();
    while (!cancelled()) {
        if (target.choice == FileConflictChoice::Skip) return DeleteFileW(staged.c_str()) ? 200 : 500;
        if (target.choice == FileConflictChoice::KeepBoth) return publishCopy(staged, root, cancelled);
        if (target.choice == FileConflictChoice::Replace) {
            if (transfer_checkpoint::localFileVersion(QString::fromStdWString(path.native())) == target.version)
                return MoveFileExW(staged.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) ? 200 : 500;
        } else {
            if (MoveFileW(staged.c_str(), path.c_str())) return 200;
            const DWORD error = GetLastError();
            if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS &&
                GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return 500;
        }
        FolderConflicts fresh;
        int status = fresh.inspect(path, target);
        if (status == 200 && !cancelled()) status = fresh.choose(itemCount, prompt);
        if (status != 200) return cancelled() ? 409 : status;
    }
    return 409;
}

// The reader supplies one bounded response from the authenticated host. Retry a
// lost response at the same offset; writes and Windows clipboard publication
// happen only after a complete, revision-matched response.
template<typename Request, typename Cancelled>
int receive(const QByteArray& manifest, const win_clipboard::fs::path& root, DWORD hostSequence,
            DWORD expectedLocal, DWORD& applied, Request request, Cancelled cancelled,
            const std::function<bool(const ClipboardUploadProgress&)>& report = {},
            std::vector<win_clipboard::fs::path>* stagedPaths = nullptr, bool saveToFolder = false,
            const ClipboardConflictPrompt& prompt = {})
{
    using namespace win_clipboard;
    if (manifest.size() > static_cast<qsizetype>(tree::max_manifest)) return 400;
    const auto object = QJsonDocument::fromJson(manifest.mid(1)).object();
    const auto lease = object.value("lease").toString();
    bool valid = false;
    const auto leaseId = lease.toULongLong(&valid);
    const auto entries = object.value("files").toArray();
    if (!valid || !leaseId || QString::number(leaseId) != lease) return 400;

    detail::staged_batch batch;
    std::vector<fs::path> indexedPaths, roots;
    ClipboardUploadProgress progress;
    progress.downloading = true;
    const auto stage = [&]() -> int {
        std::vector<File> files;
        quint64 required = 0;
        const int parsed = parseFiles(entries, files, required);
        if (parsed != 200) return parsed;
        std::vector<transfer_checkpoint::Handle> rootPins;
        std::vector<detail::file_handle> folderPins;
        if (saveToFolder) {
            if (!transfer_checkpoint::pinDirectory(QString::fromStdWString(root.native()), rootPins)) return 500;
            FolderConflicts conflicts;
            for (auto& file : files) {
                if (cancelled()) return 409;
                if (file.directory) {
                    const auto path = root / file.name;
                    if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return 500;
                    if (!tree::pin_folder(path, folderPins.emplace_back())) return 409;
                    continue;
                }
                const int status = conflicts.inspect(root / file.name, file.target);
                if (status != 200) return status;
            }
            if (cancelled()) return 409;
            const int status = conflicts.choose(int(files.size()), prompt);
            if (status != 200) return cancelled() ? 409 : status;
        }
        for (const auto& file : files) {
            if (file.target.choice == FileConflictChoice::Skip) {
                ++progress.skipped;
                required -= file.size;
                continue;
            }
            progress.names.append(QString::fromStdWString(file.name.native()));
        }
        progress.total = required;
        if (cancelled() || (report && !report(progress))) return 409;
        if (progress.names.isEmpty()) return 200;
        int status = detail::create_batch(root, batch);
        if (status != 200) return status;
        if (saveToFolder) SetFileAttributesW(batch.directory.c_str(), FILE_ATTRIBUTE_HIDDEN);
        ULARGE_INTEGER available {};
        if (!GetDiskFreeSpaceExW(batch.directory.c_str(), &available, nullptr, nullptr)) return 500;
        if (required > available.QuadPart) {
            if (saveToFolder) return 507;
            detail::expire_batches(root, batch.directory, 1);
            if (!GetDiskFreeSpaceExW(batch.directory.c_str(), &available, nullptr, nullptr)) return 500;
            if (required > available.QuadPart) return 507;
        }
        int progressIndex = 0;
        for (size_t index = 0; index < files.size(); ++index) {
            auto& [name, size, directory, target] = files[index];
            if (target.choice == FileConflictChoice::Skip) continue;
            progress.index = progressIndex++;
            progress.fileSize = size;
            progress.fileBytes = 0;
            if (cancelled() || (report && !report(progress))) return 409;
            const auto path = batch.directory / (saveToFolder ? name.filename() : name);
            if (!saveToFolder) {
                indexedPaths.push_back(path);
                if (!name.has_parent_path()) roots.push_back(path);
            }
            if (directory) {
                if (!saveToFolder) {
                    if (!CreateDirectoryW(path.c_str(), nullptr)) return 500;
                    batch.folders.emplace_back(path, nullptr);
                    if (!tree::pin_folder(path, batch.folders.back().second)) return 500;
                }
                continue;
            }
            HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (handle == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_EXISTS ? 400 : 500;
            detail::file_handle file(handle);
            batch.paths.push_back(path);
            for (quint64 offset = 0; offset < size; ) {
                if (cancelled()) return 409;
                auto chunk = request("chunk", lease, static_cast<int>(index), offset, size);
                if (chunk.status != 200) return chunk.status ? chunk.status : 409;
                const auto expected = (std::min<quint64>)(size - offset, ChunkBytes);
                if (chunk.sequence != hostSequence || static_cast<quint64>(chunk.payload.size()) != expected)
                    return 400;
                DWORD written = 0;
                if (!WriteFile(handle, chunk.payload.constData(), static_cast<DWORD>(expected), &written, nullptr) ||
                    written != expected) return 507;
                offset += written;
                progress.fileBytes = offset;
                progress.acknowledged += written;
                if (report && !report(progress)) return 409;
            }
            if (!FlushFileBuffers(handle)) return 507;
            if (saveToFolder) {
                file.reset();
                if (cancelled()) return 409;
                status = publishFile(path, root / name.parent_path(), target, int(files.size()), prompt, cancelled);
                if (status != 200) return status;
                batch.paths.pop_back();
                if (target.choice == FileConflictChoice::Skip) {
                    ++progress.skipped;
                    if (report && !report(progress)) return 409;
                }
            }
        }
        return 200;
    };

    int status = stage();
    // Finish also releases the host's read handles on a local error. A new
    // manifest or session teardown drops the snapshot if this request is lost.
    const auto finish = request("finish", lease, 0, 0, 0);
    if (status != 200) return status;
    if (!finish.status || finish.status == 409 || cancelled()) return 409;
    if (finish.status != 204 || finish.sequence != hostSequence) return 400;
    if (saveToFolder) return 204;
    if (stagedPaths) {
        *stagedPaths = std::move(indexedPaths);
        batch.published = true;
        detail::expire_batches(root, batch.directory);
        return 204;
    }
    // A cancellation accepted before this point must leave the previous clipboard intact.
    progress.committing = true;
    if (report && !report(progress)) return 409;
    // Explorer and clipboard listeners can briefly hold OpenClipboard. Keep the
    // completed batch while waiting, and check the revision on every attempt.
    for (int attempt = 0; ; ++attempt) {
        if (cancelled()) return 409;
        const int published = publish_files(batch, root, applied, expectedLocal, &roots);
        if (published != 409 || attempt == 10 || GetClipboardSequenceNumber() != expectedLocal)
            return published;
        Sleep(25);
    }
}
} // namespace clipboard_download
