// Standalone Windows regression test for metadata-only clipboard queries.
#include "../streaming/clipboardvirtualfiles.h"
#include <cassert>

int main()
{
    const auto stationName = L"MoonlightVirtualFiles-" + std::to_wstring(GetCurrentProcessId());
    auto station = CreateWindowStationW(stationName.c_str(), 0, WINSTA_ALL_ACCESS, nullptr);
    assert(station && SetProcessWindowStation(station));
    auto desktop = CreateDesktopW(L"Test", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    assert(desktop && SetThreadDesktop(desktop) && SUCCEEDED(OleInitialize(nullptr)));
    FILEDESCRIPTORW descriptor{};
    wcscpy_s(descriptor.cFileName, L"sample.bin");
    int downloads = 0;
    auto files = new ClipboardVirtualFiles({descriptor}, [&](ClipboardVirtualFiles::Paths& paths) {
        ++downloads;
        wchar_t executable[MAX_PATH];
        const auto count = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (!count || count == MAX_PATH) return false;
        paths.emplace_back(executable);
        return true;
    });
    FORMATETC metadata{CLIPFORMAT(RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW)), nullptr,
                       DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    FORMATETC content{CLIPFORMAT(RegisterClipboardFormatW(CFSTR_FILECONTENTS)), nullptr,
                      DVASPECT_CONTENT, 0, TYMED_ISTREAM};
    STGMEDIUM medium{};
    assert(SUCCEEDED(files->GetData(&metadata, &medium)));
    ReleaseStgMedium(&medium);
    assert(SUCCEEDED(files->QueryGetData(&content)) && downloads == 0);
    for (int paste = 0; paste < 2; ++paste) {
        assert(SUCCEEDED(files->GetData(&content, &medium)));
        char magic[2]{};
        ULONG received = 0;
        assert(medium.pstm->Read(magic, sizeof(magic), &received) == S_OK);
        assert(received == 2 && magic[0] == 'M' && magic[1] == 'Z');
        ReleaseStgMedium(&medium);
    }
    assert(downloads == 1);
    files->Release();

    // Native folder paste advertises CF_HDROP lazily and exposes only the selected roots.
    ClipboardVirtualFiles::Paths staged{L"C:\\cache\\photos", L"C:\\cache\\photos\\empty", L"C:\\cache\\loose.txt"};
    ClipboardVirtualFiles* tree = nullptr;
    tree = new ClipboardVirtualFiles({}, [&](auto& paths) {
        assert(OleIsCurrentClipboard(tree) == S_OK);
        ++downloads;
        paths = staged;
        return true;
    }, {0, 2});
    FORMATETC drop{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    assert(SUCCEEDED(tree->QueryGetData(&drop)) && FAILED(tree->QueryGetData(&metadata)) && downloads == 1);
    assert(SUCCEEDED(OleSetClipboard(tree)));
    assert(IsClipboardFormatAvailable(CF_HDROP) && downloads == 1);
    {
        win_clipboard::detail::clipboard_lock lock(nullptr);
        assert(lock.opened);
        auto handle = static_cast<HDROP>(GetClipboardData(CF_HDROP));
        assert(handle && DragQueryFileW(handle, 0xffffffff, nullptr, 0) == 2 && downloads == 2);
    }
    for (int paste = 0; paste < 2; ++paste) {
        assert(SUCCEEDED(tree->GetData(&drop, &medium)) && medium.tymed == TYMED_HGLOBAL);
        auto handle = static_cast<HDROP>(medium.hGlobal);
        assert(DragQueryFileW(handle, 0xffffffff, nullptr, 0) == 2);
        const auto roots = win_clipboard::detail::file_paths(handle, 2);
        assert(roots.size() == 2 && roots[0] == staged[0] && roots[1] == staged[2]);
        ReleaseStgMedium(&medium);
    }
    assert(downloads == 2);
    assert(SUCCEEDED(OleSetClipboard(nullptr)));
    tree->Release();
    OleUninitialize();
}
