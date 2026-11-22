#include "streamwindow.h"

#include <QProcess>
#include <QVector>
#include <memory>

namespace {
constexpr auto ReservationVariable = "MOONLIGHT_STREAM_WINDOW_HANDLE";
HANDLE reservation = nullptr;
HANDLE viewingMarker = nullptr;
std::wstring property;
HWND progressWindow = nullptr;
HWND connectedWindow = nullptr;

std::wstring propertyName(const QString& uuid)
{
    return (QStringLiteral("Moonlight.StreamWindow.") + uuid).toStdWString();
}

std::wstring eventName(const QString& uuid)
{
    return L"Local\\" + propertyName(uuid);
}

struct WindowSearch {
    std::wstring property;
    HWND target = nullptr;
};

BOOL CALLBACK findWindow(HWND window, LPARAM param)
{
    auto& search = *reinterpret_cast<WindowSearch*>(param);
    if (IsWindowVisible(window)) {
        const auto kind = reinterpret_cast<ULONG_PTR>(GetPropW(window, search.property.c_str()));
        if (kind == StreamWindow::Connected || kind == StreamWindow::Opening) {
            search.target = window;
            if (kind == StreamWindow::Connected)
                return FALSE;
        }
    }
    return TRUE;
}

HWND findStreamWindow(const QString& uuid)
{
    WindowSearch search{propertyName(uuid)};
    EnumWindows(findWindow, reinterpret_cast<LPARAM>(&search));
    return search.target;
}

void raiseWindow(const QString& uuid)
{
    if (const HWND window = findStreamWindow(uuid)) {
        if (IsIconic(window))
            ShowWindowAsync(window, SW_RESTORE);
        SetForegroundWindow(window);
    }
}

bool startDetached(QProcess& child, HANDLE reservationHandle)
{
    // QProcess normally inherits every inheritable handle in the launcher.
    // Forward only the reservation and the standard handles used by this launch.
    QVector<HANDLE> handles{reservationHandle};
    for (DWORD stream : {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE}) {
        const HANDLE handle = GetStdHandle(stream);
        DWORD flags;
        if (handle && handle != INVALID_HANDLE_VALUE && GetHandleInformation(handle, &flags) &&
                (flags & HANDLE_FLAG_INHERIT) && !handles.contains(handle))
            handles.append(handle);
    }

    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    if (!size)
        return false;
    auto storage = std::make_unique<BYTE[]>(size);
    STARTUPINFOEXW startup{};
    startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.get());
    if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &size))
        return false;

    bool started = false;
    if (UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                  handles.data(), handles.size() * sizeof(HANDLE), nullptr, nullptr)) {
        child.setCreateProcessArgumentsModifier([&](QProcess::CreateProcessArguments* args) {
            startup.StartupInfo = *args->startupInfo;
            startup.StartupInfo.cb = sizeof(startup);
            // A parent can have a non-inheritable standard handle. Do not give the
            // child its numeric value, which may refer to a different object there.
            for (HANDLE* handle : {&startup.StartupInfo.hStdInput, &startup.StartupInfo.hStdOutput,
                                   &startup.StartupInfo.hStdError}) {
                if (!handles.contains(*handle))
                    *handle = nullptr;
            }
            args->startupInfo = &startup.StartupInfo;
            args->inheritHandles = TRUE;
            args->flags |= EXTENDED_STARTUPINFO_PRESENT;
        });
        started = child.startDetached();
        child.setCreateProcessArgumentsModifier({});
    }
    DeleteProcThreadAttributeList(startup.lpAttributeList);
    return started;
}
}

StreamWindow::State StreamWindow::state(const QString& uuid, QString* progressText)
{
    if (progressText)
        progressText->clear();
    const HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, eventName(uuid).c_str());
    if (!event)
        return None;

    const bool connected = WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
    if (!connected && progressText) {
        if (const HWND window = findStreamWindow(uuid)) {
            wchar_t title[512];
            const int length = GetWindowTextW(window, title, sizeof(title) / sizeof(title[0]));
            if (length > 0)
                *progressText = QString::fromWCharArray(title, length);
        }
    }
    CloseHandle(event);
    return connected ? Connected : Opening;
}

bool StreamWindow::activate(const QString& uuid)
{
    if (state(uuid) == None)
        return false;

    raiseWindow(uuid);
    return true;
}

bool StreamWindow::isViewOnly(const QString& uuid)
{
    const HANDLE marker = OpenEventW(SYNCHRONIZE, FALSE, (eventName(uuid) + L".Viewing").c_str());
    if (!marker) return false;
    CloseHandle(marker);
    return true;
}

bool StreamWindow::launchOrActivate(QProcess& child, const QString& uuid)
{
    // Reserve before CreateProcess so even concurrent launchers cannot open two windows.
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE event = CreateEventW(&attributes, TRUE, FALSE, eventName(uuid).c_str());
    if (!event)
        return false;

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(event);
        const bool requestedViewOnly = child.processEnvironment().value(QStringLiteral("MOONLIGHT_WINDOW_VIEW_ONLY")) == QStringLiteral("1");
        if (requestedViewOnly != isViewOnly(uuid))
            return false;
        raiseWindow(uuid);
        return true;
    }

    auto environment = child.processEnvironment();
    environment.insert(QLatin1String(ReservationVariable), QString::number(reinterpret_cast<quintptr>(event)));
    child.setProcessEnvironment(environment);
    const bool started = startDetached(child, event);
    CloseHandle(event);
    return started;
}

void StreamWindow::adoptReservation(const QString& uuid)
{
    const auto value = qgetenv(ReservationVariable).toULongLong();
    qunsetenv(ReservationVariable);
    if (!value)
        return;

    // Only the detached GUI child inherits this handle. Its lifetime covers startup
    // and an established stream, including when the original launcher exits.
    HANDLE event = reinterpret_cast<HANDLE>(static_cast<quintptr>(value));
    const HANDLE namedEvent = OpenEventW(SYNCHRONIZE, FALSE, eventName(uuid).c_str());
    if (!namedEvent)
        return;
    using CompareHandles = BOOL (WINAPI*)(HANDLE, HANDLE);
    const auto sameObject = reinterpret_cast<CompareHandles>(GetProcAddress(
        GetModuleHandleW(L"kernelbase.dll"), "CompareObjectHandles"));
    DWORD flags;
    if (GetHandleInformation(event, &flags) && (flags & HANDLE_FLAG_INHERIT) &&
            (!sameObject || sameObject(event, namedEvent)) && SetHandleInformation(event, HANDLE_FLAG_INHERIT, 0)) {
        reservation = event;
        property = propertyName(uuid);
        if (qgetenv("MOONLIGHT_WINDOW_VIEW_ONLY") == "1")
            viewingMarker = CreateEventW(nullptr, TRUE, FALSE, (eventName(uuid) + L".Viewing").c_str());
    }
    CloseHandle(namedEvent);
}

void StreamWindow::setProgressWindow(HWND window)
{
    if (reservation) {
        progressWindow = window;
        SetPropW(window, property.c_str(), reinterpret_cast<HANDLE>(ULONG_PTR(Opening)));
    }
}

void StreamWindow::setConnectedWindow(HWND window)
{
    if (reservation) {
        connectedWindow = window;
        SetPropW(window, property.c_str(), reinterpret_cast<HANDLE>(ULONG_PTR(Connected)));
        SetEvent(reservation);
    }
}

void StreamWindow::release()
{
    if (reservation) {
        if (progressWindow)
            RemovePropW(progressWindow, property.c_str());
        if (connectedWindow)
            RemovePropW(connectedWindow, property.c_str());
        if (viewingMarker) {
            CloseHandle(viewingMarker);
            viewingMarker = nullptr;
        }
        CloseHandle(reservation);
        reservation = nullptr;
        progressWindow = nullptr;
        connectedWindow = nullptr;
    }
}
