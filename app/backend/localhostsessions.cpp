#include "localhostsessions.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRunnable>
#include <QThreadPool>
#include <QTimer>

#ifdef Q_OS_WIN
#include <QScopeGuard>
#include <string>
#include <windows.h>

// Query only the status pipe in our own Windows session. Never allow the server
// to impersonate the GUI user, including when Moonlight was launched elevated.
static QJsonObject queryHost(const QByteArray& command)
{
    DWORD session;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session)) return {};
    const auto name = std::wstring(LR"(\\.\pipe\Sunshine.HostSessions.v1.)") + std::to_wstring(session);
    const auto open = [&] {
        return CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                           FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
    };
    HANDLE pipe = open();
    if (pipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY && WaitNamedPipeW(name.c_str(), 500)) {
        pipe = open();
    }
    if (pipe == INVALID_HANDLE_VALUE) {
        return {{"absent", GetLastError() == ERROR_FILE_NOT_FOUND}};
    }
    const auto closePipe = qScopeGuard([&] { CloseHandle(pipe); });
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) return {};

    OVERLAPPED io {};
    io.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!io.hEvent) return {};
    const auto closeEvent = qScopeGuard([&] { CloseHandle(io.hEvent); });
    QByteArray response(1024 * 1024, Qt::Uninitialized);
    DWORD size = 0;
    if (!TransactNamedPipe(pipe, const_cast<char*>(command.constData()), DWORD(command.size()),
                           response.data(), DWORD(response.size()), &size, &io)) {
        if (GetLastError() != ERROR_IO_PENDING) return {};
        if (WaitForSingleObject(io.hEvent, 2000) != WAIT_OBJECT_0) {
            CancelIoEx(pipe, &io);
            GetOverlappedResult(pipe, &io, &size, TRUE);
            return {};
        }
        if (!GetOverlappedResult(pipe, &io, &size, FALSE)) return {};
    }
    response.resize(int(size));
    return QJsonDocument::fromJson(response).object();
}
#endif

bool LocalHostSessions::isLocalAudioHost(const QString& id)
{
#ifdef Q_OS_WIN
    // A tunnel to another machine can also use localhost. Only the Sunshine
    // instance in our Windows session owns the event advertised over paired HTTPS.
    const auto token = id.toLatin1();
    if (token.size() != 32 || QByteArray::fromHex(token).toHex() != token.toLower())
        return false;
    const auto name = QByteArrayLiteral("Local\\Sunshine.AudioHost.") + token;
    const HANDLE event = OpenEventA(SYNCHRONIZE, FALSE, name.constData());
    if (!event)
        return GetLastError() == ERROR_ACCESS_DENIED; // A SYSTEM-owned event can have a restrictive ACL.
    CloseHandle(event);
    return true;
#else
    Q_UNUSED(id);
    return false;
#endif
}

class LocalSessionRequest : public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit LocalSessionRequest(const QByteArray& command) : m_Command(command) {}
    void run() override
    {
#ifdef Q_OS_WIN
        emit finished(queryHost(m_Command));
#else
        emit finished({});
#endif
    }

signals:
    void finished(QJsonObject result);

private:
    QByteArray m_Command;
};

quint64 LocalHostSessions::now() const
{
#ifdef Q_OS_WIN
    return GetTickCount64();
#else
    return 0;
#endif
}

void LocalHostSessions::refresh()
{
    emit clockChanged();
    if (!m_InFlight) request("list");
}

void LocalHostSessions::disconnectDevice(const QString& visitId)
{
    if (!m_Available) return;
    for (const auto& device : m_Devices) {
        if (!visitId.isEmpty() && device.toMap().value("id").toString() == visitId) {
            queueDisconnect("disconnect " + visitId.toLatin1());
            return;
        }
    }
}

void LocalHostSessions::disconnectAll()
{
    if (m_Available && !m_Devices.isEmpty()) queueDisconnect("disconnect-all");
}

void LocalHostSessions::queueDisconnect(const QByteArray& command)
{
    if (disconnecting()) return;
    m_Disconnect = command;
    emit disconnectingChanged();
    if (!m_InFlight) request(m_Disconnect);
}

void LocalHostSessions::request(const QByteArray& command)
{
    m_InFlight = true;
    const bool action = command != "list";
    auto* task = new LocalSessionRequest(command);
    connect(task, &LocalSessionRequest::finished, this, [this, action](const QJsonObject& result) {
        const bool available = result.value("devices").isArray() && result.value("since").isDouble() && result.value("ok").isBool();
        // If a running host is temporarily busy, keep the card and show its
        // unavailable state until Sunshine confirms which streams are still active.
        const bool retain = !available && !result.value("absent").toBool();
        const auto devices = retain ? m_Devices : result.value("devices").toArray().toVariantList();
        const auto since = retain ? m_Since : result.value("since").toVariant().toULongLong();
        if (available != m_Available || devices != m_Devices || since != m_Since) {
            m_Available = available;
            m_Devices = devices;
            m_Since = since;
            emit statusChanged();
        }
        if (action) {
            m_Disconnect.clear();
            emit disconnectingChanged();
            if (!available || !result.value("ok").toBool()) emit disconnectFailed();
        }
        m_InFlight = false;
        if (disconnecting()) {
            request(m_Disconnect);
        } else if (action) {
            QTimer::singleShot(250, this, &LocalHostSessions::refresh);
        }
    }, Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(task);
}

#include "localhostsessions.moc"
