#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

class QProcess;
class QJsonObject;
class QLocalSocket;
class QSslCertificate;
class QUrl;
struct TransferEntry;

// The SDL streaming loop owns the GUI thread. Run the transfer UI in a child
// with its own Qt event loop and carry session credentials over its stdin pipe.
namespace FileTransferProcess {
constexpr auto Argument = "--file-transfer-window";
bool show(QProcess& process, const QUrl& host, const QSslCertificate& certificate,
          const QByteArray& token, const QString& hostName, const QString& hostId,
          const QByteArray& command = {});
QByteArray dropCommand(bool fromRemote, const QList<TransferEntry>& entries, const QString& destination);
void stop(QProcess& process);
int run();
}


// One process owns one already-authorized Explorer paste, including its progress window.
namespace ClipboardTransferProcess {
constexpr auto Argument = "--clipboard-file-task";
bool launch(const QJsonObject& task);
QJsonObject take(const QString& channel, QLocalSocket& socket);
int run(const QString& channel);
}
