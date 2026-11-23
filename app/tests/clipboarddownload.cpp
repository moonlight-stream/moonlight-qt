// Standalone Windows test. Link with user32, shell32, gdi32, ole32 and windowscodecs.
#include "../streaming/clipboard_win32.h"

#include <fstream>
#include <iostream>
#include <iterator>

namespace fs = std::filesystem;

static bool checkDownload(const fs::path& cache)
{
    const std::string name = u8"下载.txt";
    const std::string contents = "from the host";
    std::string packet = "F";
    win_clipboard::detail::append_size(packet, static_cast<uint32_t>(name.size()));
    packet += name;
    win_clipboard::detail::append_size(packet, static_cast<uint32_t>(contents.size()));
    packet += contents;

    // QStandardPaths supplies this spelling to the receiver.
    DWORD revision = 0;
    if (win_clipboard::write(false, true, packet, revision, cache.generic_wstring()) != 204) return false;

    win_clipboard::detail::clipboard_lock lock(nullptr);
    if (!lock.opened) return false;
    const auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
    if (!drop || DragQueryFileW(drop, 0xffffffff, nullptr, 0) != 1) return false;
    const auto paths = win_clipboard::detail::file_paths(drop, 1);
    if (paths.size() != 1 || paths.front().filename() != fs::u8path(name)) return false;

    PIDLIST_ABSOLUTE item = nullptr;
    if (FAILED(SHParseDisplayName(paths.front().c_str(), nullptr, &item, 0, nullptr))) return false;
    CoTaskMemFree(item);
    std::ifstream source(paths.front(), std::ios::binary);
    const std::string actual(std::istreambuf_iterator<char>(source), {});
    return actual == contents;
}

int main()
{
    // Clipboard changes stay on this private window station.
    auto stationName = L"MoonlightClipboardTest-" + std::to_wstring(GetCurrentProcessId());
    auto station = CreateWindowStationW(stationName.c_str(), 0, WINSTA_ALL_ACCESS, nullptr);
    if (!station || !SetProcessWindowStation(station)) return 1;
    auto desktop = CreateDesktopW(L"Test", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (!desktop || !SetThreadDesktop(desktop) || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;

    auto cache = fs::temp_directory_path() / stationName;
    if (!fs::create_directory(cache)) return 1;
    const bool passed = checkDownload(cache);
    fs::remove_all(cache);
    CoUninitialize();
    if (!passed) std::cerr << "Windows Shell could not resolve the downloaded clipboard file\n";
    return passed ? 0 : 1;
}
