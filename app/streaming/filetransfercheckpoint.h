#pragma once

#include "clipboard_win32.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <cstring>
#include <functional>

namespace transfer_checkpoint {
using Handle = win_clipboard::detail::file_handle;

inline QString digest(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

inline QString nonce()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}

inline bool validHex(const QString& s, int length)
{
    return s.size() == length && std::all_of(s.begin(), s.end(), [](QChar c) {
        return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f');
    });
}

// Each worker locks the top-level job across processes. File records are individually
// replaced, so a folder with many children never rewrites its whole history per chunk.
class Journal
{
public:
    Journal(const QByteArray& host, const QByteArray& client, bool fromRemote,
            const QString& source, const QString& target)
    {
        QJsonArray key{QString::fromLatin1(host.toHex()), QString::fromLatin1(client.toHex()),
                       fromRemote, QDir::cleanPath(source), QDir::cleanPath(target)};
        const auto path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                          QStringLiteral("/FileTransferCheckpoints/") + digest(QJsonDocument(key).toJson(QJsonDocument::Compact));
        dir = QDir(path);
        if (dir.mkpath(".")) {
            lock = std::make_unique<QLockFile>(path + "/job.lock");
            lock->setStaleLockTime(0);
            locked = lock->tryLock();
        }
    }
    ~Journal()
    {
        if (locked) {
            lock->unlock();
            QDir().rmdir(dir.absolutePath());
        }
    }
    bool ready() const { return locked; }
    QString fileName(const QString& path) const { return digest(QDir::cleanPath(path).toUtf8()) + ".json"; }
    bool load(const QString& target, QJsonObject& state) const
    {
        const auto path = dir.filePath(fileName(target));
        if (!QFileInfo::exists(path)) { state = {}; return true; }
        return read(path, state) && state.value("target").toString() == target;
    }
    bool store(const QString& target, const QJsonObject& state) const
    {
        QSaveFile f(dir.filePath(fileName(target)));
        const auto data = QJsonDocument(state).toJson(QJsonDocument::Compact);
        return f.open(QIODevice::WriteOnly) && f.write(data) == data.size() && f.commit();
    }
    bool each(const std::function<void(const QJsonObject&)>& callback) const
    {
        for (const auto& n : dir.entryList({"*.json"}, QDir::Files)) {
            QJsonObject state;
            if (!read(dir.filePath(n), state) || fileName(state.value("target").toString()) != n) return false;
            callback(state);
        }
        return true;
    }
    void clear() const
    {
        for (const auto& n : dir.entryList({"*.json"}, QDir::Files)) QFile::remove(dir.filePath(n));
    }

private:
    static bool read(const QString& path, QJsonObject& state)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly) || f.size() > 1024 * 1024) return false;
        const auto doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) return false;
        state = doc.object();
        return true;
    }
    QDir dir;
    std::unique_ptr<QLockFile> lock;
    bool locked = false;
};

inline Handle open(const QString& path, DWORD access, DWORD share, DWORD creation, DWORD flags)
{
    auto h = CreateFileW(reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(path).utf16()),
                         access, share, nullptr, creation, flags | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    return Handle(h == INVALID_HANDLE_VALUE ? nullptr : h);
}

inline QString identity(const BY_HANDLE_FILE_INFORMATION& info)
{
    return QStringLiteral("%1-%2-%3").arg(info.dwVolumeSerialNumber, 0, 16)
        .arg(info.nFileIndexHigh, 0, 16).arg(info.nFileIndexLow, 0, 16);
}

inline bool regular(HANDLE handle, BY_HANDLE_FILE_INFORMATION& info)
{
    return GetFileType(handle) == FILE_TYPE_DISK && GetFileInformationByHandle(handle, &info) &&
           !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}

inline bool pinDirectory(const QString& path, std::vector<Handle>& chain)
{
    QDir dir(path);
    QStringList parts;
    while (true) {
        parts.prepend(dir.absolutePath());
        if (dir.isRoot()) break;
        if (!dir.cdUp()) return false;
    }
    for (const auto& part : parts) {
        auto h = open(part, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS);
        BY_HANDLE_FILE_INFORMATION info{};
        if (!h || !GetFileInformationByHandle(h.get(), &info) || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
        chain.emplace_back(std::move(h));
    }
    return !parts.isEmpty();
}

inline QString partPath(const QString& destination, const QString& id)
{
    return QFileInfo(destination).dir().filePath(".moonlight-" + id + ".part");
}

inline bool disposition(HANDLE h, bool remove)
{
    FILE_DISPOSITION_INFO info{static_cast<BOOLEAN>(remove)};
    return SetFileInformationByHandle(h, FileDispositionInfo, &info, sizeof(info)) != FALSE;
}

inline Handle openPart(const QString& target, const QJsonObject& state, bool existing)
{
    const auto id = state.value("id").toString();
    if (!validHex(id, 32)) return {};
    auto h = open(partPath(target, id), GENERIC_READ | GENERIC_WRITE | DELETE, 0,
                  existing ? OPEN_EXISTING : CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN);
    BY_HANDLE_FILE_INFORMATION info{};
    if (!h || !regular(h.get(), info) || info.nNumberOfLinks != 1 ||
        (existing && identity(info) != state.value("identity").toString())) return {};
    return h;
}

inline bool publish(HANDLE h, const QString& target)
{
    const auto path = QDir::toNativeSeparators(QFileInfo(target).absoluteFilePath()).toStdWString();
    std::vector<unsigned char> data(sizeof(FILE_RENAME_INFO) + path.size() * sizeof(wchar_t));
    auto* info = reinterpret_cast<FILE_RENAME_INFO*>(data.data());
    info->FileNameLength = static_cast<DWORD>(path.size() * sizeof(wchar_t));
    std::memcpy(info->FileName, path.data(), info->FileNameLength);
    return FlushFileBuffers(h) && SetFileInformationByHandle(h, FileRenameInfo, info, static_cast<DWORD>(data.size()));
}
}
