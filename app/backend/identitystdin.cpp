#include "identitystdin.h"

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <openssl/crypto.h>

#ifdef Q_OS_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <errno.h>
#include <string.h>
#include <unistd.h>
#endif

#define JSON_CERTIFICATE "certificate"
#define JSON_KEY "key"

namespace IdentityStdin
{

bool
readAll(qint64 maxBytes, QByteArray* data, QString* error)
{
    // Reserve the whole buffer up front so appending never reallocates and leaves
    // stray copies of key material in freed memory
    QByteArray buffer;
    buffer.reserve(maxBytes);

    char chunk[4096];
    bool ok = true;

#ifdef Q_OS_WIN32
    // In a GUI-subsystem process the CRT's stdin may not be connected even when
    // the parent supplied a handle, so read the handle directly
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (input == nullptr || input == INVALID_HANDLE_VALUE) {
        *error = QObject::tr("No standard input is available to read the client identity from.");
        return false;
    }
    if (GetFileType(input) == FILE_TYPE_CHAR) {
        *error = QObject::tr("Standard input is a console. Pipe the client identity JSON into Moonlight instead.");
        return false;
    }
#else
    if (isatty(STDIN_FILENO)) {
        *error = QObject::tr("Standard input is a terminal. Pipe the client identity JSON into Moonlight instead.");
        return false;
    }
#endif

    for (;;) {
        qint64 bytesRead;

#ifdef Q_OS_WIN32
        DWORD count;
        if (!ReadFile(input, chunk, sizeof(chunk), &count, nullptr)) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                // The writer closed its end of the pipe
                break;
            }
            *error = QObject::tr("Unable to read the client identity from standard input (error %1).")
                         .arg(GetLastError());
            ok = false;
            break;
        }
        bytesRead = count;
#else
        ssize_t count = read(STDIN_FILENO, chunk, sizeof(chunk));
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            *error = QObject::tr("Unable to read the client identity from standard input: %1")
                         .arg(QString::fromLocal8Bit(strerror(errno)));
            ok = false;
            break;
        }
        bytesRead = count;
#endif

        if (bytesRead == 0) {
            break;
        }
        if (buffer.size() + bytesRead > maxBytes) {
            *error = QObject::tr("The client identity on standard input is larger than %1 bytes.").arg(maxBytes);
            ok = false;
            break;
        }
        buffer.append(chunk, static_cast<int>(bytesRead));
    }

    OPENSSL_cleanse(chunk, sizeof(chunk));

    if (!ok) {
        cleanse(buffer);
        return false;
    }
    if (buffer.isEmpty()) {
        *error = QObject::tr("No client identity was provided on standard input.");
        return false;
    }

    *data = std::move(buffer);
    return true;
}

bool
parseJson(const QByteArray& json, QByteArray* pemCert, QByteArray* pemKey, QString* error)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        *error = QObject::tr("The client identity is not valid JSON: %1").arg(parseError.errorString());
        return false;
    }
    if (!document.isObject()) {
        *error = QObject::tr("The client identity must be a JSON object.");
        return false;
    }

    QJsonObject object = document.object();
    for (const QString& member : object.keys()) {
        // Reject anything else so that typos are caught instead of ignored
        if (member != JSON_CERTIFICATE && member != JSON_KEY) {
            *error = QObject::tr("The client identity has an unexpected member \"%1\". "
                                 "Only \"%2\" and \"%3\" are allowed.")
                         .arg(member, JSON_CERTIFICATE, JSON_KEY);
            return false;
        }
    }

    QJsonValue certificate = object.value(JSON_CERTIFICATE);
    QJsonValue key = object.value(JSON_KEY);
    if (!certificate.isString() || certificate.toString().isEmpty() ||
            !key.isString() || key.toString().isEmpty()) {
        *error = QObject::tr("The client identity must have nonempty string members \"%1\" and \"%2\".")
                     .arg(JSON_CERTIFICATE, JSON_KEY);
        return false;
    }

    *pemCert = certificate.toString().toUtf8();
    *pemKey = key.toString().toUtf8();
    return true;
}

void
cleanse(QByteArray& data)
{
    if (!data.isEmpty()) {
        OPENSSL_cleanse(data.data(), data.size());
    }
    data.clear();
}

}
