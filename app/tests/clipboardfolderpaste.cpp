// Standalone Windows test; link Qt Core and the clipboard Win32 libraries.
#include "../streaming/clipboarddownload.h"
#include <QTemporaryDir>
#include <QFile>
#include <cassert>

static QByteArray readFile(const QString& path)
{
    QFile file(path);
    assert(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

int main()
{
    QTemporaryDir folder;
    assert(folder.isValid());
    const auto root = win_clipboard::fs::path(folder.path().toStdWString());
    QFile original(folder.filePath("sample.txt"));
    assert(original.open(QIODevice::WriteOnly) && original.write("existing") == 8);
    original.close();
    // A queued upload may still own its read handle while Explorer renames or deletes the source.
    {
        QFile source(folder.filePath("source.txt"));
        assert(source.open(QIODevice::WriteOnly) && source.write("original") == 8);
        source.close();
        auto selected = win_clipboard::detail::open_file(root / L"source.txt");
        assert(selected);
        assert(MoveFileW((root / L"source.txt").c_str(), (root / L"moved.txt").c_str()));
        assert(DeleteFileW((root / L"moved.txt").c_str()));
        char contents[8];
        DWORD size = 0;
        assert(ReadFile(selected->handle.get(), contents, sizeof(contents), &size, nullptr));
        assert(size == sizeof(contents) && QByteArray(contents, size) == "original");
    }
    QByteArray data(clipboard_download::ChunkBytes + 7, 'x');
    const QByteArray manifest = R"(M{"lease":"1","files":[{"name":"sample.txt","size":"1048583"}]})";
    struct Reply { int status; DWORD sequence; QByteArray payload; };
    bool cancelAfterChunk = true, cancelled = false;
    int chunks = 0, prompts = 0;
    FileConflictChoice choice = FileConflictChoice::Replace;
    const auto request = [&](const char* op, const QString&, int, quint64 offset, quint64) {
        if (qstrcmp(op, "finish") == 0) return Reply{204, 7, {}};
        ++chunks;
        cancelled = cancelAfterChunk;
        return Reply{200, 7, data.mid(offset, clipboard_download::ChunkBytes)};
    };
    DWORD ignored = 0;
    auto run = [&] {
        return clipboard_download::receive(manifest, root, 7, 0, ignored, request,
            [&]{return cancelled;}, {}, nullptr, true, [&](int count, const QList<FileConflict>& conflicts) {
                ++prompts;
                assert(count == 1 && conflicts.size() == 1 && QFileInfo(conflicts[0].path) == QFileInfo(original));
                assert(conflicts[0].replaceUnavailable.isEmpty());
                return QList{choice};
            });
    };
    assert(run() == 409);
    assert(std::distance(win_clipboard::fs::directory_iterator(root),
                         win_clipboard::fs::directory_iterator{}) == 1);
    assert(readFile(original.fileName()) == "existing" && prompts == 1);
    cancelAfterChunk = cancelled = false;
    choice = FileConflictChoice::Skip;
    chunks = 0;
    assert(run() == 204 && chunks == 0 && prompts == 2);
    choice = FileConflictChoice::KeepBoth;
    assert(run() == 204);
    assert(readFile(original.fileName()) == "existing" && readFile(folder.filePath("sample (2).txt")) == data);
    assert(run() == 204 && readFile(folder.filePath("sample (3).txt")) == data);
    choice = FileConflictChoice::Replace;
    assert(run() == 204 && readFile(original.fileName()) == data);
    assert(!QFileInfo::exists(folder.filePath("sample (4).txt")) && prompts == 5);

    // A new or changed target after the initial check must get its own prompt at publication.
    for (bool initiallyPresent : {true, false}) {
        if (!initiallyPresent) assert(original.remove());
        int questions = 0;
        const auto replaceDuringDownload = [&](const char* op, const QString&, int, quint64 offset, quint64) {
            if (qstrcmp(op, "finish") == 0) return Reply{204, 7, {}};
            if (!offset) {
                assert(original.open(QIODevice::WriteOnly) && original.write("changed") == 7);
                original.close();
            }
            return Reply{200, 7, data.mid(offset, clipboard_download::ChunkBytes)};
        };
        assert(clipboard_download::receive(manifest, root, 7, 0, ignored, replaceDuringDownload,
            [] { return false; }, {}, nullptr, true, [&](int, const QList<FileConflict>& conflicts) {
                assert(conflicts.size() == 1 && conflicts[0].replaceUnavailable.isEmpty());
                ++questions;
                return QList{initiallyPresent && questions == 1 ? FileConflictChoice::Replace : FileConflictChoice::Skip};
            }) == 204);
        assert(questions == (initiallyPresent ? 2 : 1) && readFile(original.fileName()) == "changed");
    }
    assert(clipboard_download::receive(manifest, root, 7, 0, ignored, request, [] { return false; },
        {}, nullptr, true, [&](int, const QList<FileConflict>&) {
            assert(original.remove());
            return QList{FileConflictChoice::KeepBoth};
        }) == 204);
    assert(!original.exists() && readFile(folder.filePath("sample (4).txt")) == data);

    // Skipping a colliding directory must leave the next file at its original manifest index.
    assert(QDir(folder.path()).mkdir("blocked"));
    const QByteArray mixed = R"(M{"lease":"2","files":[{"name":"blocked","size":"9223372036854775807"},{"name":"next","size":"4"}]})";
    assert(clipboard_download::receive(mixed, root, 7, 0, ignored,
        [](const char* op, const QString&, int index, quint64, quint64) {
            if (qstrcmp(op, "finish") == 0) return Reply{204, 7, {}};
            assert(index == 1);
            return Reply{200, 7, "next"};
        }, [] { return false; }, {}, nullptr, true, [](int count, const QList<FileConflict>& conflicts) {
            assert(count == 2 && conflicts.size() == 1 && !conflicts[0].replaceUnavailable.isEmpty());
            return QList{FileConflictChoice::Skip};
        }) == 204);
    assert(QFileInfo(folder.filePath("blocked")).isDir() && readFile(folder.filePath("next")) == "next");

    // A single answer can mix all three decisions. Only the selected entries are downloaded.
    {
        QTemporaryDir target;
        assert(target.isValid());
        for (const auto name : {"skip.txt", "keep.txt", "replace.txt"}) {
            QFile old(target.filePath(name));
            assert(old.open(QIODevice::WriteOnly) && old.write("old") == 3);
        }
        const QByteArray selection = R"(M{"lease":"3","files":[{"name":"skip.txt","size":"3"},{"name":"keep.txt","size":"3"},{"name":"new.txt","size":"3"},{"name":"replace.txt","size":"3"}]})";
        int questions = 0;
        QList<int> requested;
        assert(clipboard_download::receive(selection, win_clipboard::fs::path(target.path().toStdWString()), 7, 0, ignored,
            [&](const char* op, const QString&, int index, quint64, quint64) {
                if (qstrcmp(op, "finish") == 0) return Reply{204, 7, {}};
                requested << index;
                return Reply{200, 7, "new"};
            }, [] { return false; }, {}, nullptr, true, [&](int count, const QList<FileConflict>& conflicts) {
                ++questions;
                assert(count == 4 && conflicts.size() == 3);
                assert(QFileInfo(conflicts[0].path).fileName() == "skip.txt");
                assert(QFileInfo(conflicts[1].path).fileName() == "keep.txt");
                assert(QFileInfo(conflicts[2].path).fileName() == "replace.txt");
                return QList{FileConflictChoice::Skip, FileConflictChoice::KeepBoth, FileConflictChoice::Replace};
            }) == 204);
        assert(questions == 1 && (requested == QList<int>{1, 2, 3}));
        assert(readFile(target.filePath("skip.txt")) == "old" && readFile(target.filePath("keep.txt")) == "old");
        assert(readFile(target.filePath("keep (2).txt")) == "new" && readFile(target.filePath("replace.txt")) == "new");
        assert(readFile(target.filePath("new.txt")) == "new");
    }

    // Walk an actual selection and paste its indexed manifest over an existing tree.
    {
        QTemporaryDir source, destination;
        assert(source.isValid() && destination.isValid());
        const auto tree = QStringLiteral("照片");
        assert(QDir(source.path()).mkpath(tree + "/empty") && QDir(source.path()).mkpath(tree + "/nested"));
        for (const auto path : {tree + "/nested/a.txt", tree + "/a.txt", QStringLiteral("loose.txt")}) {
            QFile file(source.filePath(path));
            assert(file.open(QIODevice::WriteOnly) && file.write(path.toUtf8()) == path.toUtf8().size());
        }
        QJsonArray items;
        std::vector<win_clipboard::detail::file_handle> handles;
        using Path = win_clipboard::fs::path;
        assert(win_clipboard::tree::walk({Path(source.filePath(tree).toStdWString()), Path(source.filePath("loose.txt").toStdWString())},
            true, false, [&](const Path&, const std::wstring& name, const auto& info, auto& handle) {
                const bool dir = name.back() == L'/';
                items.append(QJsonObject{{"name", QString::fromStdWString(name)}, {"size", QString::number(dir ? 0 : info.nFileSizeLow)}});
                handles.push_back(dir ? nullptr : std::move(handle));
            }) == 200);
        const QByteArray packet = "M" + QJsonDocument(QJsonObject{{"lease","7"}, {"files",items}}).toJson(QJsonDocument::Compact);
        auto fetch = [&](const char* op, const QString&, int index, quint64, quint64 size) {
            if (qstrcmp(op, "finish") == 0) return Reply{204, 7, {}};
            QByteArray bytes(int(size), '\0');
            DWORD read;
            assert(handles[index] && SetFilePointer(handles[index].get(), 0, nullptr, FILE_BEGIN) == 0);
            assert(ReadFile(handles[index].get(), bytes.data(), DWORD(size), &read, nullptr) && read == size);
            return Reply{200, 7, bytes};
        };
        const Path target(destination.path().toStdWString());
        assert(QDir(destination.path()).mkpath(tree + "/nested"));
        QFile existing(destination.filePath(tree + "/nested/a.txt"));
        assert(existing.open(QIODevice::WriteOnly) && existing.write("old") == 3);
        existing.close();
        int prompts = 0;
        assert(clipboard_download::receive(packet, target, 7, 0, ignored, fetch, [] { return false; }, {}, nullptr, true,
            [&](int count, const QList<FileConflict>& conflicts) {
                assert(count == items.size() && conflicts.size() == 1 && QFileInfo(conflicts[0].path) == QFileInfo(existing));
                ++prompts;
                return QList{FileConflictChoice::KeepBoth};
            }) == 204);
        assert(prompts == 1 && readFile(existing.fileName()) == "old");
        assert(readFile(destination.filePath(tree + "/nested/a (2).txt")) == (tree + "/nested/a.txt").toUtf8());
        assert(readFile(destination.filePath(tree + "/a.txt")) == (tree + "/a.txt").toUtf8());
        assert(QFileInfo(destination.filePath(tree + "/empty")).isDir() && readFile(destination.filePath("loose.txt")) == "loose.txt");
        std::vector<Path> paths;
        assert(clipboard_download::receive(packet, target, 7, 0, ignored, fetch, [] { return false; }, {}, &paths) == 204);
        assert(paths.size() == size_t(items.size()) && win_clipboard::fs::is_directory(paths.front() / L"empty"));
        assert(readFile(QString::fromStdWString((paths.front() / L"nested/a.txt").native())) == (tree + "/nested/a.txt").toUtf8());
        assert(readFile(QString::fromStdWString(paths.back().native())) == "loose.txt");
        win_clipboard::tree::layout layout;
        Path relative;
        bool dir;
        assert(layout.add(L"a/", relative, dir) && dir);
        for (auto invalid : {L"/abs", L"../x", L"a/../x", L"a//f", L"a/NUL", L"a/C:x", L"a\\x", L"missing/child", L"A"})
            assert(!layout.add(invalid, relative, dir));
        handles.clear();
    }

    const QByteArray multiple = R"(M{"lease":"1","files":[{"name":"empty.txt","size":"0"},{"name":"first.txt","size":"4"},{"name":"second.txt","size":"1048583"}]})";
    for (bool cancel : {false, true}) {
        QTemporaryDir target;
        const auto targetPath = win_clipboard::fs::path(target.path().toStdWString());
        bool sawFirst = false;
        cancelled = false;
        const auto read = [&](const char* op, const QString&, int index, quint64, quint64) {
            if (qstrcmp(op, "finish") == 0) return Reply{204, 7, {}};
            if (index == 1) return Reply{200, 7, "done"};
            QFile first(target.filePath("first.txt"));
            assert(first.open(QIODevice::ReadOnly) && first.readAll() == "done");
            assert(QFile::exists(target.filePath("empty.txt")));
            assert(!QFile::exists(target.filePath("second.txt")));
            sawFirst = true;
            cancelled = cancel;
            return cancel ? Reply{200, 7, data.left(clipboard_download::ChunkBytes)} : Reply{409, 7, {}};
        };
        assert(clipboard_download::receive(multiple, targetPath, 7, 0, ignored, read,
                   [&]{ return cancelled; }, {}, nullptr, true) == 409);
        assert(sawFirst && QFile::exists(target.filePath("first.txt")) && QFile::exists(target.filePath("empty.txt")));
        assert(std::distance(win_clipboard::fs::directory_iterator(targetPath),
                             win_clipboard::fs::directory_iterator{}) == 2);
    }
}
