#pragma once

#include <QByteArray>
#include <QString>

class QProcess;
class QSslCertificate;
class QUrl;

// The SDL streaming loop owns the GUI thread. Run the transfer UI in a child
// with its own Qt event loop and carry session credentials over its stdin pipe.
namespace FileTransferProcess {
constexpr auto Argument = "--file-transfer-window";
bool show(QProcess& process, const QUrl& host, const QSslCertificate& certificate,
          const QByteArray& token, const QString& hostName, const QString& hostId);
void stop(QProcess& process);
int run();
}
