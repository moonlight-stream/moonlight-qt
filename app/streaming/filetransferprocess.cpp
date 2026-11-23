#include "filetransferprocess.h"
#include "filetransferwindow.h"
#include "backend/identitymanager.h"
#include "settings/streamingpreferences.h"

#include <QApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QUuid>
#include <QDeadlineTimer>
#include <QProcess>
#include <QSslCertificate>
#include <QTimer>
#include <QUrl>
#include <qt_windows.h>

namespace FileTransferProcess {
constexpr qsizetype maxPacket = 16 * 1024 * 1024;

QByteArray dropCommand(bool fromRemote, const QList<TransferEntry>& entries, const QString& destination)
{
    QJsonArray files;
    for (const auto& entry : entries) {
        files.append(QJsonObject{{"name", entry.name}, {"path", entry.path}, {"directory", entry.directory},
                                 {"size", QString::number(entry.size)}, {"modified", QString::number(entry.modified.toMSecsSinceEpoch())}});
    }
    return QJsonDocument(QJsonObject{{"files", files}, {"fromRemote", fromRemote}, {"destination", destination}})
        .toJson(QJsonDocument::Compact);
}

static bool send(QProcess& process, const QByteArray& packet)
{
    if (process.write(packet) != packet.size()) return false;
    while (process.bytesToWrite() && process.waitForBytesWritten(1000)) {}
    return !process.bytesToWrite();
}

bool show(QProcess& process, const QUrl& host, const QSslCertificate& certificate,
          const QByteArray& token, const QString& hostName, const QString& hostId, const QByteArray& command)
{
    if (command.size() >= maxPacket) return false;
    // The SDL thread doesn't dispatch Qt events, so explicitly reap an exited child.
    if (process.state() != QProcess::NotRunning && !process.waitForFinished(0)) {
        AllowSetForegroundWindow(static_cast<DWORD>(process.processId()));
        return send(process, (command.isEmpty() ? QByteArray("R") : command) + '\n');
    }
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(QCoreApplication::applicationFilePath(), {QString::fromLatin1(Argument)});
    if (!process.waitForStarted(2000))
        return false;
    AllowSetForegroundWindow(static_cast<DWORD>(process.processId()));
    QJsonObject settings {
        {"host", host.toString()}, {"certificate", QString::fromLatin1(certificate.toPem())},
        {"token", QString::fromLatin1(token)}, {"name", hostName}, {"id", hostId}
    };
    QByteArray packet = QJsonDocument(settings).toJson(QJsonDocument::Compact) + '\n';
    packet += (command.isEmpty() ? QByteArray("R") : command) + '\n';
    if (send(process, packet)) return true;
    stop(process);
    return false;
}

void stop(QProcess& process)
{
    if (process.state() != QProcess::NotRunning) {
        process.closeWriteChannel();
        if (!process.waitForFinished(2000)) {
            process.kill();
            process.waitForFinished();
        }
    }
}

int run()
{
    HANDLE pipe = GetStdHandle(STD_INPUT_HANDLE);
    if (GetFileType(pipe) != FILE_TYPE_PIPE)
        return 1;

    QByteArray packet;
    char block[4096];
    DWORD bytes = 0;
    while (!packet.contains('\n')) {
        if (!ReadFile(pipe, block, sizeof(block), &bytes, nullptr) || !bytes || packet.size() >= 65536)
            return 1;
        packet.append(block, int(bytes));
    }
    const auto newline = packet.indexOf('\n');
    const auto settings = QJsonDocument::fromJson(packet.left(newline)).object();
    packet.remove(0, newline + 1);
    const QUrl host(settings.value("host").toString());
    if (!host.isValid() || host.scheme() != QStringLiteral("https") || host.host().isEmpty())
        return 1;

    StreamingPreferences::get()->retranslate();
    IdentityManager::get();
    // Keep the session's queue and any active copies alive when the user closes
    // the browser; another click in the stream opens the same window again.
    QApplication::setQuitOnLastWindowClosed(false);
    FileTransferWindow window(host, QSslCertificate(settings.value("certificate").toString().toLatin1()),
                              settings.value("token").toString().toLatin1(),
                              settings.value("name").toString(), settings.value("id").toString());
    auto activate = [&window]() {
        if (window.isMinimized())
            window.showNormal();
        else
            window.show();
        window.raise();
        window.activateWindow();
    };
    auto dispatch = [&] {
        qsizetype end;
        while ((end = packet.indexOf('\n')) >= 0) {
            const auto line = packet.left(end);
            packet.remove(0, end + 1);
            if (line == "R") {
                activate();
                continue;
            }
            const auto drop = QJsonDocument::fromJson(line).object();
            const auto folder = drop.value("destination").toString();
            if (!QDir::isAbsolutePath(folder) || !drop.value("fromRemote").isBool()) continue;
            QList<TransferEntry> entries;
            for (const auto value : drop.value("files").toArray()) {
                const auto file = value.toObject();
                entries.append({file.value("name").toString(), file.value("path").toString(), file.value("directory").toBool(),
                                file.value("size").toString().toLongLong(),
                                QDateTime::fromMSecsSinceEpoch(file.value("modified").toString().toLongLong())});
            }
            window.transferTo(drop.value("fromRemote").toBool(), entries, folder);
            if (!window.isVisible()) {
                window.setAttribute(Qt::WA_ShowWithoutActivating, true);
                window.showMinimized();
                window.setAttribute(Qt::WA_ShowWithoutActivating, false);
            }
        }
    };
    dispatch();
    // Closing the pipe on disconnect or process death also ends this GUI loop,
    // so the window and any partially written transfer are cleaned up together.
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &window, [&]() {
        DWORD pending;
        for (;;) {
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &pending, nullptr)) {
                QCoreApplication::quit();
                return;
            }
            if (!pending) break;
            if (!ReadFile(pipe, block, qMin<DWORD>(pending, sizeof(block)), &bytes, nullptr) || !bytes) {
                QCoreApplication::quit();
                return;
            }
            packet.append(block, int(bytes));
            dispatch();
            if (packet.size() >= maxPacket) {
                QCoreApplication::quit();
                return;
            }
        }
    });
    timer.start(200);
    return QApplication::exec();
}
}

namespace ClipboardTransferProcess {
bool launch(const QJsonObject& task)
{
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    const auto name = QStringLiteral("moonlight-transfer-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!server.listen(name)) return false;
    qint64 pid = 0;
    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {Argument, name}, {}, &pid)) return false;
    if (!server.waitForNewConnection(7000)) return false;
    auto socket = server.nextPendingConnection();
    ULONG client = 0;
    if (!socket || !GetNamedPipeClientProcessId(reinterpret_cast<HANDLE>(socket->socketDescriptor()), &client) ||
        client != static_cast<ULONG>(pid)) return false;
    const auto packet = QJsonDocument(task).toJson(QJsonDocument::Compact) + '\n';
    if (socket->write(packet) != packet.size()) return false;
    QDeadlineTimer deadline(5000);
    while (socket->bytesToWrite()) {
        if (deadline.hasExpired() || !socket->waitForBytesWritten(deadline.remainingTime())) return false;
    }
    return (socket->bytesAvailable() || socket->waitForReadyRead(5000)) && socket->read(1) == "R";
}

QJsonObject take(const QString& channel, QLocalSocket& socket)
{
    socket.setReadBufferSize(256 * 1024);
    socket.connectToServer(channel);
    if (!socket.waitForConnected(5000)) return {};
    QDeadlineTimer deadline(5000);
    while (!socket.canReadLine()) {
        if (deadline.hasExpired() || socket.bytesAvailable() >= socket.readBufferSize() ||
            !socket.waitForReadyRead(deadline.remainingTime())) return {};
    }
    return QJsonDocument::fromJson(socket.readLine()).object();
}
}
