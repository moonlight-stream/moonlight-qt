// Standalone Windows test. Link with user32, gdi32, ole32, shell32 and windowscodecs.
#include "../streaming/clipboard_win32.h"
#include <iostream>

static bool putDib(UINT width, UINT height)
{
    const UINT stride = (width * 3 + 3) & ~3u;
    auto data = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + size_t(stride) * height);
    if (!data) return false;
    {
        clipboard_image::locked_memory lock(data);
        if (!lock.bytes) { GlobalFree(data); return false; }
        BITMAPINFOHEADER h{};
        h.biSize = sizeof(h);
        h.biWidth = LONG(width);
        h.biHeight = LONG(height);
        h.biPlanes = 1;
        h.biBitCount = 24;
        h.biCompression = BI_RGB;
        std::memcpy(lock.bytes, &h, sizeof(h));
        auto* bits = static_cast<BYTE*>(lock.bytes) + sizeof(h);
        for (UINT y = 0; y < height; ++y) {
            for (UINT x = 0; x < width; ++x) {
                auto* p = bits + size_t(height - 1 - y) * stride + x * 3;
                p[0] = BYTE(x);
                p[1] = BYTE(y);
                p[2] = BYTE(x ^ y);
            }
        }
    }
    DWORD sequence = 0;
    return win_clipboard::publish_data(CF_DIB, data, sequence, {}) == 204;
}

static bool checkDib(UINT width, UINT height)
{
    if (!putDib(width, height)) return false;
    DWORD sequence = 0;
    std::string packet;
    if (win_clipboard::read(false, false, {}, packet, sequence) != 204) return false;
    if (win_clipboard::read(false, false, {}, packet, sequence, nullptr, true) != 200 ||
        packet.empty() || packet.front() != 'I') return false;
    if (win_clipboard::write(false, false, packet, sequence, {}) != 403 ||
        win_clipboard::write(false, false, "Ibroken", sequence, {}, {}, true) != 400 ||
        win_clipboard::write(false, false, packet, sequence, {}, sequence, true) != 204) return false;

    win_clipboard::detail::clipboard_lock lock(nullptr);
    if (!lock.opened || !GetClipboardData(clipboard_image::png_format())) return false;
    clipboard_image::locked_memory memory(GetClipboardData(CF_DIBV5));
    if (!memory.bytes) return false;
    auto h = static_cast<const BITMAPV5HEADER*>(memory.bytes);
    if (h->bV5Width != LONG(width) || h->bV5Height != -LONG(height) || h->bV5AlphaMask != 0xff000000) return false;
    auto pixels = reinterpret_cast<const BYTE*>(h + 1);
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const auto* p = pixels + (size_t(y) * width + x) * 4;
            if (p[0] != BYTE(x) || p[1] != BYTE(y) || p[2] != BYTE(x ^ y) || p[3] != 255) return false;
        }
    }
    return true;
}

static bool checkAlpha()
{
    clipboard_image::snapshot image;
    image.width = 2;
    image.height = 1;
    image.stride = 8;
    image.alpha = true;
    image.pixels = {3, 23, 153, 255, 6, 198, 9, 71};
    DWORD sequence = 0;
    std::string packet;
    if (clipboard_image::encode(image, packet) != 200 ||
        win_clipboard::write(false, false, packet, sequence, {}, {}, true) != 204) return false;
    std::string reread;
    if (win_clipboard::read(false, false, {}, reread, sequence, nullptr, true) != 200 || reread != packet) return false;
    win_clipboard::detail::clipboard_lock lock(nullptr);
    clipboard_image::locked_memory memory(GetClipboardData(CF_DIBV5));
    return lock.opened && memory.bytes &&
           std::memcmp(static_cast<BYTE*>(memory.bytes) + sizeof(BITMAPV5HEADER),
                       image.pixels.data(), image.pixels.size()) == 0;
}

int main()
{
    auto name = L"MoonlightClipboardImage-" + std::to_wstring(GetCurrentProcessId());
    auto station = CreateWindowStationW(name.c_str(), 0, WINSTA_ALL_ACCESS, nullptr);
    if (!station || !SetProcessWindowStation(station)) return 1;
    auto desktop = CreateDesktopW(L"Test", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (!desktop || !SetThreadDesktop(desktop)) return 1;
    if (!checkDib(3840, 2160) || !checkDib(5, 3) || !checkAlpha()) {
        std::cerr << "Clipboard PNG / DIB pixel round trip failed\n";
        return 1;
    }
    return 0;
}
