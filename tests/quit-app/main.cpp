#include "backend/nvhttp.h"
#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdio>

extern "C" const char* LiGetLaunchUrlQueryParameters(void) { return ""; }

static QByteArray read(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2; // Argument: absolute path to server.py
    QTemporaryDir temp;
    QProcess server;
    server.start("python", {QString::fromLocal8Bit(argv[1]), temp.path()});
    if (!server.waitForReadyRead(10000)) return 2;
    const auto port = server.readLine().trimmed().toUShort();
    if (!port) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    app.setOrganizationName("MoonlightQuitTest");
    app.setApplicationName("Test");
    QSettings settings;
    settings.setValue("certificate", read(temp.filePath("client.crt")));
    settings.setValue("key", read(temp.filePath("client.pem")));
    settings.sync();
    NvHTTP http(NvAddress("127.0.0.1", port), port,
                QSslCertificate(read(temp.filePath("host.crt"))), true);
    int failures = 0;
    for (const auto& mode : {"success", "refused", "missing", "denied", "busy", "slow"}) {
        QFile file(temp.filePath("mode"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 2;
        file.write(mode);
        file.close();
        int status = 0;
        try { http.quitApp(); }
        catch (const GfeHttpResponseException& e) { status = e.getStatusCode(); }
        catch (const QtNetworkReplyException&) { status = -1; }
        const QByteArray name(mode);
        const bool passed = (name == "success" || name == "slow") ? status == 0 :
                            name == "busy" ? status == 599 :
                            name == "denied" ? status == 403 : status > 0;
        printf("%s %s (status %d)\n", passed ? "PASS" : "FAIL", mode, status);
        fflush(stdout);
        failures += !passed;
    }
    server.kill();
    server.waitForFinished();
    return failures ? 1 : 0;
}
