#include "streaming/clipboardsync.h"

#include <QCoreApplication>
#include <QMimeData>
#include <QTextStream>
#include <QUrl>

namespace {
bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << '\n';
    }
    return condition;
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    bool ok = true;
    ok &= require(!ClipboardSync::shouldTransferOutOfBand(ClipboardSync::INLINE_THRESHOLD - 1),
                  QStringLiteral("payload below the inline threshold used blob transfer"), err);
    ok &= require(ClipboardSync::shouldTransferOutOfBand(ClipboardSync::INLINE_THRESHOLD),
                  QStringLiteral("payload at the inline threshold did not use blob transfer"), err);
    ok &= require(ClipboardSync::shouldTransferOutOfBand(70 * 1024),
                  QStringLiteral("70 KiB payload did not use blob transfer"), err);
    ok &= require(ClipboardSync::MAX_BLOB_BYTES == 64LL * 1024 * 1024,
                  QStringLiteral("blob size cap changed unexpectedly"), err);

    QMimeData localFile;
    localFile.setUrls({QUrl::fromLocalFile(QStringLiteral("/tmp/report.tex"))});
    localFile.setData(QStringLiteral("image/tiff"), QByteArrayLiteral("finder-icon"));
    ok &= require(ClipboardSync::hasFileReferences(&localFile),
                  QStringLiteral("local file URL with image fallback was not protected"), err);

    QMimeData macFile;
    macFile.setData(QStringLiteral("application/x-qt-mac-pasteboard-mime;value=\"public.file-url\""),
                    QByteArrayLiteral("file:///tmp/report.tex"));
    ok &= require(ClipboardSync::hasFileReferences(&macFile),
                  QStringLiteral("macOS public.file-url format was not protected"), err);

    QMimeData promisedFile;
    promisedFile.setData(QStringLiteral("com.apple.pasteboard.promised-file-url"),
                         QByteArrayLiteral("file:///tmp/future-file"));
    ok &= require(ClipboardSync::hasFileReferences(&promisedFile),
                  QStringLiteral("macOS promised file format was not protected"), err);

    QMimeData windowsFile;
    windowsFile.setData(QStringLiteral("application/x-qt-windows-mime;value=\"FileGroupDescriptorW\""),
                        QByteArrayLiteral("descriptor"));
    ok &= require(ClipboardSync::hasFileReferences(&windowsFile),
                  QStringLiteral("Windows file descriptor format was not protected"), err);

    QMimeData webImage;
    webImage.setUrls({QUrl(QStringLiteral("https://example.com/image.png"))});
    webImage.setData(QStringLiteral("image/png"), QByteArrayLiteral("png"));
    ok &= require(!ClipboardSync::hasFileReferences(&webImage),
                  QStringLiteral("remote image URL was misclassified as a file clipboard"), err);

    QMimeData plainImage;
    plainImage.setData(QStringLiteral("image/png"), QByteArrayLiteral("png"));
    ok &= require(!ClipboardSync::hasFileReferences(&plainImage),
                  QStringLiteral("plain image clipboard was misclassified as a file clipboard"), err);

    if (!ok) {
        return 1;
    }

    out << "clipboard_payload_routing=passed\n";
    return 0;
}
