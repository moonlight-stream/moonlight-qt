// Standalone Windows test; link Qt Core and the clipboard Win32 libraries.
#include "../streaming/clipboarddownload.h"
#include <QTemporaryDir>
#include <QFile>
#include <cassert>

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
    const auto request = [&](const char* op, const QString&, int, quint64 offset, quint64) {
        if (qstrcmp(op, "finish") == 0) return Reply{204, 7, {}};
        cancelled = cancelAfterChunk;
        return Reply{200, 7, data.mid(offset, clipboard_download::ChunkBytes)};
    };
    DWORD ignored = 0;
    auto run = [&] {
        return clipboard_download::receive(manifest, root, 7, 0, ignored, request,
            [&]{return cancelled;}, {}, nullptr, true);
    };
    assert(run() == 409);
    assert(std::distance(win_clipboard::fs::directory_iterator(root),
                         win_clipboard::fs::directory_iterator{}) == 1);
    cancelAfterChunk = cancelled = false;
    assert(run() == 204);
    assert(original.open(QIODevice::ReadOnly) && original.readAll() == "existing");
    QFile received(folder.filePath("sample (2).txt"));
    assert(received.open(QIODevice::ReadOnly) && received.readAll() == data);

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
