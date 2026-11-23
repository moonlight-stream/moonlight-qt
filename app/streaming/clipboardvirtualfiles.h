#pragma once

#include "clipboard_win32.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <functional>
#include <atomic>
#include <cstring>

#ifdef _MSC_VER
#pragma comment(lib, "shlwapi.lib")
#endif

// Explorer enumerates FILEGROUPDESCRIPTORW to discover a selection. Only a
// request for FILECONTENTS needs the actual bytes.
class ClipboardVirtualFiles final : public IDataObject, public IDataObjectAsyncCapability
{
public:
    using Paths = std::vector<std::filesystem::path>;
    using Download = std::function<bool(Paths&)>;

    ClipboardVirtualFiles(std::vector<FILEDESCRIPTORW> files, Download download, std::vector<size_t> dropRoots = {})
        : descriptors(std::move(files)), roots(std::move(dropRoots)), fetch(std::move(download)) {}

    void disconnect() { fetch = {}; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_IDataObject) *out = static_cast<IDataObject*>(this);
        else if (iid == IID_IDataObjectAsyncCapability) *out = static_cast<IDataObjectAsyncCapability*>(this);
        else return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto n = --refs;
        if (!n) delete this;
        return n;
    }
    // Shell targets that handle their own paste (including the context menu)
    // can extract streams on a worker instead of blocking their UI thread.
    HRESULT STDMETHODCALLTYPE SetAsyncMode(BOOL enabled) override { asyncMode = enabled; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetAsyncMode(BOOL* enabled) override {
        if (!enabled) return E_POINTER;
        *enabled = asyncMode;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE StartOperation(IBindCtx*) override {
        if (!inOperation) { AddRef(); inOperation = true; }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE InOperation(BOOL* active) override {
        if (!active) return E_POINTER;
        *active = inOperation;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EndOperation(HRESULT, IBindCtx*, DWORD) override {
        if (inOperation) { inOperation = false; Release(); }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* f) override {
        if (!f) return E_POINTER;
        if (f->dwAspect != DVASPECT_CONTENT) return DV_E_DVASPECT;
        if (!roots.empty()) return f->cfFormat == CF_HDROP && (f->tymed & TYMED_HGLOBAL) ? S_OK : DV_E_FORMATETC;
        if (f->cfFormat == descriptorFormat && (f->tymed & TYMED_HGLOBAL)) return S_OK;
        if (f->cfFormat != contentsFormat || !(f->tymed & TYMED_ISTREAM)) return DV_E_FORMATETC;
        return f->lindex >= 0 && size_t(f->lindex) < descriptors.size() ? S_OK : DV_E_LINDEX;
    }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* f, STGMEDIUM* medium) override {
        if (!medium) return E_POINTER;
        *medium = {};
        const HRESULT supported = QueryGetData(f);
        if (FAILED(supported)) return supported;
        if (f->cfFormat == descriptorFormat) {
            const size_t bytes = offsetof(FILEGROUPDESCRIPTORW, fgd) + descriptors.size() * sizeof(FILEDESCRIPTORW);
            HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (!memory) return E_OUTOFMEMORY;
            auto group = static_cast<FILEGROUPDESCRIPTORW*>(GlobalLock(memory));
            if (!group) { GlobalFree(memory); return E_OUTOFMEMORY; }
            group->cItems = static_cast<UINT>(descriptors.size());
            std::memcpy(group->fgd, descriptors.data(), bytes - offsetof(FILEGROUPDESCRIPTORW, fgd));
            GlobalUnlock(memory);
            medium->tymed = TYMED_HGLOBAL;
            medium->hGlobal = memory;
            return S_OK;
        }
        if (paths.empty()) {
            if (loading || !fetch) return STG_E_READFAULT;
            loading = true;
            const bool ok = fetch(paths);
            loading = false;
            if (!ok || (roots.empty() ? paths.size() != descriptors.size() : paths.size() <= roots.back())) {
                paths.clear();
                return STG_E_READFAULT;
            }
        }
        if (f->cfFormat == CF_HDROP) {
            std::wstring names;
            for (const auto index : roots) {
                names += paths[index].native();
                names.push_back(L'\0');
            }
            medium->hGlobal = win_clipboard::detail::make_data(names, true);
            if (!medium->hGlobal) return E_OUTOFMEMORY;
            medium->tymed = TYMED_HGLOBAL;
            return S_OK;
        }
        const HRESULT hr = SHCreateStreamOnFileEx(paths[f->lindex].c_str(),
            STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &medium->pstm);
        if (SUCCEEDED(hr)) medium->tymed = TYMED_ISTREAM;
        return hr;
    }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction, IEnumFORMATETC** out) override {
        if (direction != DATADIR_GET) return E_NOTIMPL;
        if (!roots.empty()) {
            FORMATETC drop{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            return SHCreateStdEnumFmtEtc(1, &drop, out);
        }
        FORMATETC formats[] = {{descriptorFormat, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL},
                               {contentsFormat, nullptr, DVASPECT_CONTENT, -1, TYMED_ISTREAM}};
        return SHCreateStdEnumFmtEtc(2, formats, out);
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) override {
        if (out) out->ptd = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }

private:
    const CLIPFORMAT descriptorFormat = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW));
    const CLIPFORMAT contentsFormat = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_FILECONTENTS));
    std::atomic<ULONG> refs{1};
    std::vector<FILEDESCRIPTORW> descriptors;
    std::vector<size_t> roots; // Explorer receives the original top-level items when a selection contains folders.
    Download fetch;
    Paths paths;
    BOOL asyncMode = TRUE;
    bool inOperation = false;
    bool loading = false;
};
