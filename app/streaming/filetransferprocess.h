#pragma once

#include "fileconflict.h"

#include <QByteArray>
#include <QList>
#include <QLocalSocket>
#include <QString>
#include <memory>

class QJsonObject;
class QSslCertificate;
class QUrl;
struct TransferEntry;

// The stream owns only a command channel to the transfer window.
namespace FileTransferProcess {
constexpr auto Argument = "--file-transfer-window";
class Window {
public:
    bool show(const QUrl& host, const QSslCertificate& certificate, const QByteArray& streamToken,
              bool standalone, const QString& hostName, const QString& hostId, const QByteArray& command = {});
    void closeChannel() { m_Socket.reset(); }
private:
    std::unique_ptr<QLocalSocket> m_Socket;
    qint64 m_Pid = 0;
};
QByteArray dropCommand(bool fromRemote, const QList<TransferEntry>& entries, const QString& destination);
int run(const QString& channel);
}


// One process owns one already-authorized Explorer paste, including its progress window.
namespace ClipboardTransferProcess {
constexpr auto Argument = "--clipboard-file-task";
constexpr auto ConflictArgument = "--clipboard-file-conflict";
std::optional<QList<FileConflictChoice>> askConflicts(bool remote, int itemCount, const QList<FileConflict>& conflicts,
                                                     const std::function<bool()>& cancelled);
int runConflict();
bool launch(const QJsonObject& task);
QJsonObject take(const QString& channel, QLocalSocket& socket);
int run(const QString& channel);
}
