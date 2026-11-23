#pragma once

#include "clipboard_win32.h"
#include "clipboarduploadprogress.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <limits>
#include <functional>

namespace clipboard_download {
constexpr quint64 ChunkBytes = 1024 * 1024;

// Publish one closed file on the same volume. MoveFileW reserves the final
// name without overwriting an existing file, even with concurrent pastes.
inline int publishFile(const win_clipboard::fs::path& path, const win_clipboard::fs::path& root)
{
    const auto name = path.filename();
    auto target = root / name;
    for (unsigned suffix = 2; !MoveFileW(path.c_str(), target.c_str()); ++suffix) {
        const DWORD error = GetLastError();
        if ((error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS) || suffix > 10000) return 500;
        const auto tail = L" (" + std::to_wstring(suffix) + L")" + name.extension().wstring();
        if (tail.size() >= 255) return 400;
        target = root / (name.stem().wstring().substr(0, 255 - tail.size()) + tail);
    }
    return 200;
}

// The reader supplies one bounded response from the authenticated host. Retry a
// lost response at the same offset; writes and Windows clipboard publication
// happen only after a complete, revision-matched response.
template<typename Request, typename Cancelled>
int receive(const QByteArray& manifest, const win_clipboard::fs::path& root, DWORD hostSequence,
            DWORD expectedLocal, DWORD& applied, Request request, Cancelled cancelled,
            const std::function<bool(const ClipboardUploadProgress&)>& report = {},
            std::vector<win_clipboard::fs::path>* stagedPaths = nullptr, bool saveToFolder = false)
{
    using namespace win_clipboard;
    if (manifest.size() > 64 * 1024) return 400;
    const auto object = QJsonDocument::fromJson(manifest.mid(1)).object();
    const auto lease = object.value("lease").toString();
    bool valid = false;
    const auto leaseId = lease.toULongLong(&valid);
    const auto entries = object.value("files").toArray();
    if (!valid || !leaseId || QString::number(leaseId) != lease ||
        entries.isEmpty() || entries.size() > static_cast<int>(max_files)) return 400;

    detail::staged_batch batch;
    ClipboardUploadProgress progress;
    progress.downloading = true;
    const auto stage = [&]() -> int {
        int status = detail::create_batch(root, batch);
        if (status != 200) return status;
        if (saveToFolder) SetFileAttributesW(batch.directory.c_str(), FILE_ATTRIBUTE_HIDDEN);
        struct File { std::wstring name; quint64 size; };
        std::vector<File> files;
        quint64 required = 0;
        for (const auto& value : entries) {
            const auto entry = value.toObject();
            const auto name = entry.value("name").toString().toStdWString();
            const auto length = entry.value("size").toString();
            const auto size = length.toULongLong(&valid);
            if (!detail::valid_name(name) || !valid || QString::number(size) != length ||
                size > static_cast<quint64>((std::numeric_limits<LONGLONG>::max)())) return 400;
            if (size > (std::numeric_limits<quint64>::max)() - required) return 507;
            required += size;
            files.push_back({name, size});
            progress.names.append(QString::fromStdWString(name));
        }
        progress.total = required;
        if (cancelled() || (report && !report(progress))) return 409;
        ULARGE_INTEGER available {};
        if (!GetDiskFreeSpaceExW(batch.directory.c_str(), &available, nullptr, nullptr)) return 500;
        if (required > available.QuadPart) {
            if (saveToFolder) return 507;
            detail::expire_batches(root, batch.directory, 1);
            if (!GetDiskFreeSpaceExW(batch.directory.c_str(), &available, nullptr, nullptr)) return 500;
            if (required > available.QuadPart) return 507;
        }
        for (size_t index = 0; index < files.size(); ++index) {
            const auto& [name, size] = files[index];
            progress.index = static_cast<int>(index);
            progress.fileSize = size;
            progress.fileBytes = 0;
            if (cancelled() || (report && !report(progress))) return 409;
            const auto path = batch.directory / name;
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
                status = publishFile(path, root);
                if (status != 200) return status;
                batch.paths.pop_back();
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
        *stagedPaths = batch.paths;
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
        const int published = publish_files(batch, root, applied, expectedLocal);
        if (published != 409 || attempt == 10 || GetClipboardSequenceNumber() != expectedLocal)
            return published;
        Sleep(25);
    }
}
} // namespace clipboard_download
