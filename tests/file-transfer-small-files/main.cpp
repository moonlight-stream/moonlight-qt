#include "backend/identitymanager.h"
#include "streaming/filetransferengine.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSslServer>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>

namespace {
// Hold the first wave of pinned reads so completion, failure and cancellation order are deterministic.
class DownloadHost : public QSslServer
{
public:
    DownloadHost()
    {
        auto ssl = IdentityManager::get()->getSslConfig();
        ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
        setSslConfiguration(ssl);
        for (int i = 0; i < 12; ++i) {
            files.insert(QStringLiteral("小文件-%1.txt").arg(i), QByteArray(512 + i, char('a' + i)));
        }
        files.insert(QStringLiteral("零字节"), {});
        connect(this, &QTcpServer::pendingConnectionAvailable, this, [this] {
            auto* socket = nextPendingConnection();
            connect(socket, &QIODevice::readyRead, this, [this, socket] { request(socket); });
            connect(socket, &QAbstractSocket::disconnected, socket, &QObject::deleteLater);
            request(socket);
        });
    }

    void release(bool interrupt)
    {
        hold = false;
        while (!pending.isEmpty()) {
            const auto read = pending.takeLast();
            if (read.socket) {
                if (interrupt && pending.isEmpty()) respond(read.socket, 503);
                else respond(read.socket, 200, read.data);
            }
        }
    }

    struct Read { QPointer<QTcpSocket> socket; QByteArray data; };
    QMap<QString, QByteArray> files;
    QList<Read> pending;
    int capabilities = 0;
    bool hold = true;

private:
    static void respond(QTcpSocket* socket, int code, const QByteArray& body = {})
    {
        socket->write("HTTP/1.1 " + QByteArray::number(code) + " Response\r\nContent-Length: " +
                      QByteArray::number(body.size()) + "\r\n\r\n" + body);
    }

    void request(QTcpSocket* socket)
    {
        const auto header = socket->peek(socket->bytesAvailable());
        const auto end = header.indexOf("\r\n\r\n");
        if (end < 0) return;
        socket->read(end + 4); // Downloads only send GETs and empty POSTs.
        const auto url = QUrl::fromEncoded(header.split(' ').value(1));
        const auto op = url.path().section('/', -1);
        const auto path = QUrlQuery(url).queryItemValue("path", QUrl::FullyDecoded);
        const auto name = path.section('/', -1);
        if (op == "capabilities") {
            ++capabilities;
            respond(socket, 200, R"({"version":2,"backgroundTransfers":true})");
        } else if (op == "list" && path == "D:/source") {
            QJsonArray entries;
            for (auto it = files.cbegin(); it != files.cend(); ++it) {
                entries.append(QJsonObject{{"name", it.key()}, {"size", QString::number(it.value().size())},
                                           {"directory", false}});
            }
            respond(socket, 200, QJsonDocument(QJsonObject{{"entries", entries}}).toJson());
        } else if (op == "open-read" && files.contains(name)) {
            const auto data = files.value(name);
            const auto hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
            respond(socket, 200, QJsonDocument(QJsonObject{{"size", QString::number(data.size())},
                                                         {"sha256", QString::fromLatin1(hash)}}).toJson());
        } else if (op == "read-pinned" && files.contains(name)) {
            if (hold) pending.append({socket, files.value(name)});
            else respond(socket, 200, files.value(name));
        } else if (op == "abort") {
            respond(socket, 204);
        } else {
            respond(socket, 404);
        }
    }
};
}

class SmallFilesTest : public QObject
{
    Q_OBJECT
    QTemporaryDir settings;

private slots:
    void initTestCase()
    {
        QVERIFY(settings.isValid());
        QCoreApplication::setOrganizationName("Moonlight tests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        QStandardPaths::setTestModeEnabled(true);
    }

    void folder_data()
    {
        QTest::addColumn<bool>("cancel");
        QTest::newRow("resume after one read fails") << false;
        QTest::newRow("cancel with reads in flight") << true;
    }

    void selectedFilesShareCapabilities()
    {
        DownloadHost host;
        host.hold = false;
        QVERIFY(host.listen(QHostAddress::LocalHost));
        QTemporaryDir destination;
        QVERIFY(destination.isValid());
        FileTransferEngine engine(QUrl(QStringLiteral("https://127.0.0.1:%1").arg(host.serverPort())),
                                  host.sslConfiguration().localCertificate(), "fixture");
        QList<TransferEntry> entries;
        for (auto it = host.files.cbegin(); it != host.files.cend(); ++it)
            entries.append({it.key(), "D:/source/" + it.key(), false, it.value().size()});
        int finished = 0;
        connect(&engine, &FileTransferEngine::progress, this, [&](int, qint64, qint64, QString status, bool done) {
            if (done) {
                QCOMPARE(status, FileTransferEngine::tr("Completed"));
                ++finished;
            }
        });
        engine.enqueue(true, entries, destination.path());
        QTRY_COMPARE(finished, entries.size());
        QCOMPARE(host.capabilities, 1);
    }

    void folder()
    {
        QFETCH(bool, cancel);
        DownloadHost host;
        QVERIFY(host.listen(QHostAddress::LocalHost));
        QTemporaryDir destination;
        QVERIFY(destination.isValid());
        const QUrl url(QStringLiteral("https://127.0.0.1:%1").arg(host.serverPort()));
        FileTransferEngine engine(url, host.sslConfiguration().localCertificate(), "fixture");
        const QList<TransferEntry> folder{{"source", "D:/source", true}};
        QSignalSpy interrupted(&engine, &FileTransferEngine::interrupted);
        QStringList finished;
        qint64 bytes = 0;
        connect(&engine, &FileTransferEngine::progress, this, [&](int, qint64 transferred, qint64, QString status, bool done) {
            if (done) {
                finished.append(status);
                bytes = transferred;
            }
        });
        const auto id = engine.enqueue(true, folder, destination.path()).first();
        QTRY_COMPARE(host.pending.size(), 4);
        if (cancel) engine.cancel(id);
        else host.release(true);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(interrupted.size(), cancel ? 0 : 1);
        QDir received(destination.filePath("source"));
        if (cancel) {
            QCOMPARE(finished.first(), FileTransferEngine::tr("Cancelled"));
            QVERIFY(received.entryList({".moonlight-*.part"}, QDir::Files | QDir::Hidden).isEmpty());
            host.release(false);
        }

        engine.enqueue(true, folder, destination.path());
        QTRY_COMPARE(finished.size(), 2);
        QCOMPARE(finished.last(), FileTransferEngine::tr("Completed"));
        qint64 expectedBytes = 0;
        for (auto it = host.files.cbegin(); it != host.files.cend(); ++it) {
            QFile file(received.filePath(it.key()));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), it.value());
            expectedBytes += it.value().size();
        }
        QCOMPARE(bytes, expectedBytes);
        QCOMPARE(received.entryList(QDir::Files | QDir::Hidden).size(), host.files.size());
    }
};

QTEST_GUILESS_MAIN(SmallFilesTest)
#include "main.moc"
