#include "filetransferprocess.h"
#include "filetransferwindow.h"
#include "backend/identitymanager.h"
#include "settings/streamingpreferences.h"

#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDialog>
#include <QUuid>
#include <QDeadlineTimer>
#include <QProcess>
#include <QSslCertificate>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <qt_windows.h>

namespace {
std::unique_ptr<QLocalSocket> launchHelper(const char* argument, qint64& pid)
{
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    const auto name = QStringLiteral("moonlight-transfer-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!server.listen(name) || !QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                                         {QString::fromLatin1(argument), name}, {}, &pid) ||
        !server.waitForNewConnection(7000)) return {};
    auto* socket = server.nextPendingConnection();
    ULONG client = 0;
    if (!socket || !GetNamedPipeClientProcessId(reinterpret_cast<HANDLE>(socket->socketDescriptor()), &client) ||
        client != static_cast<ULONG>(pid)) return {};
    socket->setParent(nullptr);
    return std::unique_ptr<QLocalSocket>(socket);
}

bool sendPacket(QLocalSocket& socket, const QByteArray& packet)
{
    if (socket.write(packet) != packet.size()) return false;
    QDeadlineTimer deadline(5000);
    while (socket.bytesToWrite()) {
        if (deadline.hasExpired() || !socket.waitForBytesWritten(deadline.remainingTime())) return false;
    }
    return true;
}

bool acknowledged(QLocalSocket& socket)
{
    return (socket.bytesAvailable() || socket.waitForReadyRead(5000)) && socket.read(1) == "R";
}
}

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

bool Window::show(const QUrl& host, const QSslCertificate& certificate, const QByteArray& streamToken,
                  bool standalone, const QString& hostName, const QString& hostId, const QByteArray& command)
{
    if (command.size() >= maxPacket) return false;
    // The SDL loop does not dispatch Qt socket notifications.
    if (m_Socket && !PeekNamedPipe(reinterpret_cast<HANDLE>(m_Socket->socketDescriptor()),
                                    nullptr, 0, nullptr, nullptr, nullptr)) closeChannel();
    if (!m_Socket) {
        m_Socket = launchHelper(Argument, m_Pid);
        if (!m_Socket) return false;
        QJsonObject settings{{"host", host.toString()}, {"certificate", QString::fromLatin1(certificate.toPem())},
                             {"standalone", standalone}, {"name", hostName}, {"id", hostId}};
        if (!standalone) settings.insert("token", QString::fromLatin1(streamToken));
        if (!sendPacket(*m_Socket, QJsonDocument(settings).toJson(QJsonDocument::Compact) + '\n')) {
            closeChannel();
            return false;
        }
    }
    AllowSetForegroundWindow(static_cast<DWORD>(m_Pid));
    if (sendPacket(*m_Socket, (command.isEmpty() ? QByteArray("R") : command) + '\n') &&
        acknowledged(*m_Socket)) return true;
    closeChannel();
    return false;
}

int run(const QString& channel)
{
    QLocalSocket socket;
    const auto settings = ClipboardTransferProcess::take(channel, socket);
    const QUrl host(settings.value("host").toString());
    if (!host.isValid() || host.scheme() != QStringLiteral("https") || host.host().isEmpty()) return 1;
    const bool standalone = settings.value("standalone").toBool();
    const auto token = standalone ? FileTransferEngine::newSessionToken() : settings.value("token").toString().toLatin1();
    if (standalone && token.isEmpty()) return 1;

    StreamingPreferences::get()->retranslate();
    IdentityManager::get();
    QApplication::setQuitOnLastWindowClosed(false);
    FileTransferWindow window(host, QSslCertificate(settings.value("certificate").toString().toLatin1()), token,
                              settings.value("name").toString(), settings.value("id").toString(), nullptr, standalone);
    auto activate = [&window]() {
        if (window.isMinimized())
            window.showNormal();
        else
            window.show();
        window.raise();
        window.activateWindow();
    };
    auto dispatch = [&] {
        while (socket.canReadLine()) {
            QByteArray line = socket.readLine();
            line.chop(1);
            if (line == "R") {
                activate();
                socket.write("R", 1);
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
            socket.write("R", 1);
        }
        if (socket.bytesAvailable() >= maxPacket) socket.abort();
    };
    bool attached = true;
    auto streamEnded = [&] {
        if (!attached) return;
        attached = false;
        dispatch();
        // A hidden standalone window defers its close until the queue drains.
        if (!standalone || (!window.isVisible() && window.close())) QCoreApplication::quit();
    };
    QObject::connect(&window, &FileTransferWindow::closed, &window, [&] {
        if (!attached) QCoreApplication::quit();
    });
    QObject::connect(&socket, &QLocalSocket::readyRead, &window, dispatch);
    QObject::connect(&socket, &QLocalSocket::disconnected, &window, streamEnded);
    QTimer::singleShot(0, &window, [&] {
        dispatch(); // The first command may have arrived with the settings.
        if (socket.state() == QLocalSocket::UnconnectedState) streamEnded();
    });
    return QApplication::exec();
}
}

namespace ClipboardTransferProcess {
static std::optional<QList<FileConflictChoice>> askOnGuiThread(bool remote, int itemCount, const QList<FileConflict>& conflicts,
                                                              const std::function<bool()>& cancelled)
{
    QEventLoop loop;
    std::optional<QList<FileConflictChoice>> answer;
    auto* box = showFileConflicts(nullptr, remote, itemCount, conflicts, [&](const QList<FileConflictChoice>& choices) {
        answer = choices;
        loop.quit();
    });
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, box, [&] { if (cancelled()) box->reject(); });
    timer.start(100);
    loop.exec();
    return cancelled() ? std::nullopt : answer;
}

// A clipboard worker cannot create Qt widgets, and the SDL streaming loop owns
// the application's GUI thread. Give the same dialog its own GUI loop there.
std::optional<QList<FileConflictChoice>> askConflicts(bool remote, int itemCount, const QList<FileConflict>& conflicts,
                                                     const std::function<bool()>& cancelled)
{
    if (cancelled() || conflicts.isEmpty() || itemCount < conflicts.size()) return {};
    if (QThread::currentThread() == qApp->thread()) return askOnGuiThread(remote, itemCount, conflicts, cancelled);

    QJsonArray files;
    for (const auto& file : conflicts)
        files.append(QJsonObject{{"path", file.path}, {"reason", file.replaceUnavailable}});
    const auto packet = QJsonDocument(QJsonObject{{"files", files}, {"count", itemCount}, {"remote", remote}})
                            .toJson(QJsonDocument::Compact) + '\n';
    if (packet.size() > FileTransferProcess::maxPacket) return {};

    QProcess child;
    QEventLoop loop;
    QObject::connect(&child, &QProcess::finished, &loop, &QEventLoop::quit);
    child.start(QCoreApplication::applicationFilePath(), {ConflictArgument});
    if (!child.waitForStarted(3000)) return {};
    AllowSetForegroundWindow(static_cast<DWORD>(child.processId()));
    if (child.write(packet) != packet.size()) return {};
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &child, [&] { if (cancelled()) child.kill(); });
    timer.start(100);
    if (child.state() != QProcess::NotRunning) loop.exec();
    if (cancelled() || child.exitStatus() != QProcess::NormalExit || child.exitCode() != 0) return {};
    const auto values = QJsonDocument::fromJson(child.readAllStandardOutput()).array();
    if (values.size() != conflicts.size()) return {};
    QList<FileConflictChoice> choices;
    for (const auto value : values) {
        const int choice = value.toInt(-1);
        if (choice < int(FileConflictChoice::Skip) || choice > int(FileConflictChoice::KeepBoth)) return {};
        choices.append(static_cast<FileConflictChoice>(choice));
    }
    return choices;
}

int runConflict()
{
    HANDLE parent = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetFileType(parent) != FILE_TYPE_PIPE || GetFileType(output) != FILE_TYPE_PIPE) return 1;
    QByteArray packet;
    char block[4096];
    DWORD bytes;
    do {
        if (!ReadFile(parent, block, sizeof(block), &bytes, nullptr) || !bytes) return 1;
        packet.append(block, int(bytes));
        if (packet.size() > FileTransferProcess::maxPacket) return 1;
    } while (!packet.endsWith('\n'));
    const auto request = QJsonDocument::fromJson(packet).object();
    const int itemCount = request.value("count").toInt();
    QList<FileConflict> conflicts;
    for (const auto value : request.value("files").toArray()) {
        const auto file = value.toObject();
        if (!file.value("path").isString() || !file.value("reason").isString()) return 1;
        conflicts.append({file.value("path").toString(), file.value("reason").toString()});
    }
    if (conflicts.isEmpty() || conflicts.size() > itemCount) return 1;
    StreamingPreferences::get()->retranslate();
    const auto answer = askOnGuiThread(request.value("remote").toBool(), itemCount, conflicts, [parent] {
        return !PeekNamedPipe(parent, nullptr, 0, nullptr, nullptr, nullptr);
    });
    if (!answer) return 1;
    QJsonArray values;
    for (const auto choice : *answer) values.append(int(choice));
    const auto response = QJsonDocument(values).toJson(QJsonDocument::Compact);
    return WriteFile(output, response.constData(), DWORD(response.size()), &bytes, nullptr) && bytes == DWORD(response.size()) ? 0 : 1;
}

bool launch(const QJsonObject& task)
{
    qint64 pid = 0;
    auto socket = launchHelper(Argument, pid);
    return socket && sendPacket(*socket, QJsonDocument(task).toJson(QJsonDocument::Compact) + '\n') &&
           acknowledged(*socket);
}

QJsonObject take(const QString& channel, QLocalSocket& socket)
{
    socket.setReadBufferSize(FileTransferProcess::maxPacket);
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
