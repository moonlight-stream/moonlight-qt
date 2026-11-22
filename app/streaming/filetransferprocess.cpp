#include "filetransferprocess.h"
#include "filetransferwindow.h"
#include "backend/identitymanager.h"
#include "settings/streamingpreferences.h"

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSslCertificate>
#include <QTimer>
#include <QUrl>
#include <qt_windows.h>

namespace FileTransferProcess {
bool show(QProcess& process, const QUrl& host, const QSslCertificate& certificate,
          const QByteArray& token, const QString& hostName, const QString& hostId)
{
    // The SDL thread doesn't dispatch Qt events, so explicitly reap an exited child.
    if (process.state() != QProcess::NotRunning && !process.waitForFinished(0)) {
        AllowSetForegroundWindow(static_cast<DWORD>(process.processId()));
        return process.write("R", 1) == 1 && process.waitForBytesWritten(1000);
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
    if (process.write(packet) == packet.size()) {
        while (process.bytesToWrite() && process.waitForBytesWritten(1000)) {}
        if (!process.bytesToWrite())
            return true;
    }
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
    const auto settings = QJsonDocument::fromJson(packet.left(packet.indexOf('\n'))).object();
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
    activate();
    // Closing the pipe on disconnect or process death also ends this GUI loop,
    // so the window and any partially written transfer are cleaned up together.
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &window, [&]() {
        DWORD pending = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &pending, nullptr)) {
            QCoreApplication::quit();
        } else if (pending && ReadFile(pipe, block, qMin<DWORD>(pending, sizeof(block)), &bytes, nullptr)) {
            activate();
        }
    });
    timer.start(200);
    return QApplication::exec();
}
}
