// Standalone Windows regression test for metadata-only clipboard queries.
#include "../streaming/clipboardvirtualfiles.h"
#include <cassert>

int main()
{
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
}
