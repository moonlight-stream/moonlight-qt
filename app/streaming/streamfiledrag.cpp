#include "streamfiledrag.h"
#include "shellfiledrop.h"
#include "streamutils.h"
#include "../../moonlight-common-c/inputmode.h"
#include <Limelight.h>
#include <SDL_syswm.h>
#include <QScopeGuard>
#include <shellapi.h>

namespace {
template <class T> bool ready(const std::future<T>& future)
{
    return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}
}

class StreamFileDrag::DropTarget final : public IDropTarget
{
public:
    explicit DropTarget(StreamFileDrag* drag) : owner(drag) {}
    StreamFileDrag* owner;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (iid != IID_IUnknown && iid != IID_IDropTarget) return E_NOINTERFACE;
        *object = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto count = --refs;
        if (!count) delete this;
        return count;
    }
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD, POINTL point, DWORD* effect) override
    {
        files = data && data->QueryGetData(&format) == S_OK;
        return DragOver(0, point, effect);
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL point, DWORD* effect) override
    {
        *effect = owner && files && owner->framePoint({point.x, point.y}) ? (*effect & DROPEFFECT_COPY) : DROPEFFECT_NONE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override { files = false; return S_OK; }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD, POINTL point, DWORD* effect) override
    {
        const bool copy = (*effect & DROPEFFECT_COPY) != 0;
        *effect = DROPEFFECT_NONE;
        files = false;
        if (!owner || !copy || !data) return S_OK;
        const auto position = owner->framePoint({point.x, point.y});
        STGMEDIUM medium {};
        if (!position || FAILED(data->GetData(&format, &medium))) return S_OK;
        const auto release = qScopeGuard([&] { ReleaseStgMedium(&medium); });
        if (medium.tymed != TYMED_HGLOBAL) return S_OK;
        const auto drop = reinterpret_cast<HDROP>(medium.hGlobal);
        const auto count = DragQueryFileW(drop, 0xffffffff, nullptr, 0);
        if (!count || count > 4096) return S_OK;
        QList<QUrl> urls;
        for (UINT i = 0; i < count; ++i) {
            const auto length = DragQueryFileW(drop, i, nullptr, 0);
            if (!length || length > 32767) return S_OK;
            std::wstring name(length + 1, L'\0');
            if (DragQueryFileW(drop, i, name.data(), length + 1) != length) return S_OK;
            name.resize(length);
            urls << QUrl::fromLocalFile(QString::fromStdWString(name));
        }
        owner->m_Uploads.push_back(owner->m_Engine.remoteDragLocation(false, *position, std::move(urls)));
        *effect = DROPEFFECT_COPY;
        return S_OK;
    }

private:
    std::atomic<ULONG> refs{1};
    FORMATETC format{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    bool files = false;
};

StreamFileDrag::StreamFileDrag(SDL_Window* window, QSize frame, QUrl host, QSslCertificate certificate,
                               QByteArray token, Transfer transfer, Error error)
    : m_Window(window), m_Frame(frame), m_Engine(std::move(host), std::move(certificate), std::move(token)),
      m_Transfer(std::move(transfer)), m_Error(std::move(error)), m_Ole(OleInitialize(nullptr))
{
    updateWindow();
}

StreamFileDrag::~StreamFileDrag()
{
    cancelGesture();
    detach();
    if (SUCCEEDED(m_Ole)) OleUninitialize();
}

void StreamFileDrag::detach()
{
    if (m_DropTarget) {
        m_DropTarget->owner = nullptr;
        RevokeDragDrop(m_Owner);
        m_DropTarget->Release();
        m_DropTarget = nullptr;
    }
    m_Owner = nullptr;
}

void StreamFileDrag::updateWindow()
{
    SDL_SysWMinfo info {};
    SDL_VERSION(&info.version);
    const HWND owner = SDL_GetWindowWMInfo(m_Window, &info) ? info.info.win.window : nullptr;
    if (owner == m_Owner) return;
    cancelGesture();
    detach();
    m_Owner = owner;
    if (!owner || FAILED(m_Ole)) return;
    DragAcceptFiles(owner, FALSE);
    auto* target = new DropTarget(this);
    auto hr = RegisterDragDrop(owner, target);
    // sdl2-compat registers SDL3's generic drop target on this HWND. We need
    // the original OLE drop point and must advertise only the copy effect.
    if (hr == DRAGDROP_E_ALREADYREGISTERED && SUCCEEDED(RevokeDragDrop(owner))) hr = RegisterDragDrop(owner, target);
    if (SUCCEEDED(hr)) m_DropTarget = target;
    else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot register stream file drop target: 0x%08lx", hr);
        target->Release();
    }
}

std::optional<QPoint> StreamFileDrag::framePoint(POINT screen) const
{
    RECT client;
    if (!m_Owner || !ScreenToClient(m_Owner, &screen) || !GetClientRect(m_Owner, &client) ||
        !PtInRect(&client, screen) || m_Frame.isEmpty()) return {};
    SDL_Rect src {0, 0, m_Frame.width(), m_Frame.height()};
    SDL_Rect rect {0, 0, int(client.right), int(client.bottom)};
    StreamUtils::scaleSourceToDestinationSurface(&src, &rect);
    SDL_Point p {int(screen.x), int(screen.y)};
    if (!SDL_PointInRect(&p, &rect)) return {};
    return QPoint(int((qint64(p.x - rect.x) * 65536 + 32768) / rect.w),
                  int((qint64(p.y - rect.y) * 65536 + 32768) / rect.h));
}

bool StreamFileDrag::waitingForRelease() const
{
    return m_Gesture && m_Gesture->deferredRelease;
}

bool StreamFileDrag::pending() const
{
    return m_Gesture.has_value() || !m_Uploads.empty() || !m_Downloads.empty();
}

void StreamFileDrag::cancelGesture()
{
    finishGesture(true);
}

bool StreamFileDrag::finishGesture(bool cancelRemoteDrag)
{
    int status = 0;
    if (waitingForRelease()) {
        // An outward drop must not accidentally move the originals in Explorer
        // if the target lookup is interrupted or the local gesture is cancelled.
        status = cancelRemoteDrag ? MlCancelFileDrag() : LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
    }
    m_Gesture.reset();
    return status == 0;
}

void StreamFileDrag::probe(POINT screen)
{
    if (!m_Gesture || m_Gesture->source.valid() || framePoint(screen) ||
        (std::abs(screen.x - m_Gesture->start.x) < GetSystemMetrics(SM_CXDRAG) &&
         std::abs(screen.y - m_Gesture->start.y) < GetSystemMetrics(SM_CYDRAG)) ||
        !shell_file_drop::shell_window(WindowFromPhysicalPoint(screen))) return;
    m_Gesture->source = m_Engine.remoteDragLocation(true, m_Gesture->anchor);
}

bool StreamFileDrag::handleButton(const SDL_MouseButtonEvent& event, bool canStart, Uint8 primaryButton)
{
    if (event.which == SDL_TOUCH_MOUSEID || event.windowID != SDL_GetWindowID(m_Window)) return false;
    if (event.button != primaryButton || !m_Owner) return false;
    int width, height;
    SDL_GetWindowSize(m_Window, &width, &height);
    RECT client;
    if (width <= 0 || height <= 0 || !GetClientRect(m_Owner, &client)) return false;
    POINT screen {MulDiv(event.x, client.right, width), MulDiv(event.y, client.bottom, height)};
    if (!ClientToScreen(m_Owner, &screen)) return false;
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        const auto position = framePoint(screen);
        if (canStart && position) m_Gesture.emplace(Gesture{*position, screen, {}, {}});
        return false;
    }
    if (!m_Gesture) return false;
    probe(screen);
    const HWND target = WindowFromPhysicalPoint(screen);
    if (!m_Gesture->source.valid() || framePoint(screen) || !shell_file_drop::shell_window(target)) {
        cancelGesture();
        return false;
    }
    m_Gesture->target = m_Engine.localDropFolder(QPoint(screen.x, screen.y), reinterpret_cast<quintptr>(target));
    m_Gesture->releasedAt = SDL_GetTicks();
    m_Gesture->deferredRelease = true;
    return true;
}

void StreamFileDrag::poll()
{
    if (waitingForRelease()) {
        if (ready(m_Gesture->source)) {
            auto source = m_Gesture->source.get();
            // End the host's Explorer drag before the button comes up. A remote
            // window drag or text selection still gets its ordinary release.
            if (!source.entries.isEmpty()) {
                auto target = std::move(m_Gesture->target);
                if (finishGesture(true)) m_Downloads.push_back({std::move(source.entries), std::move(target)});
                else m_Error(FileTransferEngine::tr("File transfer is unavailable for this session"));
            } else {
                finishGesture(!source.error.isEmpty());
                if (!source.error.isEmpty()) m_Error(source.error);
            }
        } else if (SDL_GetTicks() - m_Gesture->releasedAt > 3500) {
            cancelGesture();
            m_Error(FileTransferEngine::tr("The file drop timed out."));
        }
    } else if (m_Gesture && !m_Gesture->source.valid()) {
        POINT screen;
        if (GetCursorPos(&screen)) probe(screen);
    }
    for (auto it = m_Downloads.begin(); it != m_Downloads.end();) {
        if (!ready(it->target)) { ++it; continue; }
        auto folder = it->target.get();
        auto entries = std::move(it->entries);
        it = m_Downloads.erase(it);
        if (!folder.isEmpty()) m_Transfer(true, entries, folder);
        else m_Error(FileTransferEngine::tr("Drop files on a folder or empty space in File Explorer or on the desktop."));
    }
    for (auto it = m_Uploads.begin(); it != m_Uploads.end();) {
        if (!ready(*it)) { ++it; continue; }
        auto target = it->get();
        it = m_Uploads.erase(it);
        if (!target.error.isEmpty()) m_Error(target.error);
        else if (target.folder.isEmpty()) m_Error(FileTransferEngine::tr("Drop files on a folder or empty space in File Explorer or on the desktop."));
        else m_Transfer(false, target.entries, target.folder);
    }
}
