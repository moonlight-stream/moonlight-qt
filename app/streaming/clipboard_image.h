// Lossless image clipboard wire format shared by the Windows desktop peers.
#pragma once

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace clipboard_image {
using Microsoft::WRL::ComPtr;

// Image packets are decoded into memory. File transfers use independent streamed chunks.
constexpr size_t max_packet = 64 * 1024 * 1024;
constexpr uint64_t max_pixels = 64ULL * 1024 * 1024;

inline UINT png_format() {
    static const UINT id = RegisterClipboardFormatW(L"PNG");
    return id;
}

inline bool available() {
    return IsClipboardFormatAvailable(png_format()) || IsClipboardFormatAvailable(CF_BITMAP) ||
           IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5);
}

struct locked_memory {
    HGLOBAL memory;
    void* bytes;
    explicit locked_memory(HGLOBAL handle) : memory(handle), bytes(handle ? GlobalLock(handle) : nullptr) {}
    ~locked_memory() { if (bytes) GlobalUnlock(memory); }
};

struct com_scope {
    HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~com_scope() { if (SUCCEEDED(status)) CoUninitialize(); }
};

struct snapshot {
    std::string png;
    std::vector<BYTE> pixels;
    UINT width = 0, height = 0, stride = 0;
    bool alpha = false;
};

inline bool valid_size(UINT width, UINT height) {
    return width && height && uint64_t(width) * height <= max_pixels;
}

inline uint32_t big_endian(const char* p) {
    const auto* b = reinterpret_cast<const unsigned char*>(p);
    return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
}

// Native clipboard allocations can have trailing padding. Send only the PNG datastream.
inline size_t png_size(std::string_view data) {
    if (data.size() < 33 || data.substr(0, 8) != std::string_view("\x89PNG\r\n\x1a\n", 8) ||
        big_endian(data.data() + 8) != 13 || data.substr(12, 4) != "IHDR" ||
        !valid_size(big_endian(data.data() + 16), big_endian(data.data() + 20))) return 0;
    size_t pos = 8;
    while (data.size() - pos >= 12) {
        const size_t bytes = big_endian(data.data() + pos);
        if (bytes > data.size() - pos - 12) return 0;
        const bool end = data.substr(pos + 4, 4) == "IEND";
        pos += 12 + bytes;
        if (end) return bytes == 0 ? pos : 0;
    }
    return 0;
}

// Called with the clipboard open. Copy pixels before releasing the clipboard, then compress off-lock.
inline int capture(snapshot& out) {
    if (IsClipboardFormatAvailable(png_format())) {
        auto memory = GetClipboardData(png_format());
        const size_t capacity = memory ? GlobalSize(memory) : 0;
        locked_memory lock(memory);
        if (lock.bytes && capacity <= max_packet) {
            auto bytes = std::string_view(static_cast<const char*>(lock.bytes), capacity);
            if (const size_t used = png_size(bytes); used && used < max_packet) {
                out.png.assign(bytes.substr(0, used));
                return 200;
            }
        }
    }

    // Preserve straight alpha from a native BGRA DIBV5. GDI's bitmap conversion drops alpha.
    if (IsClipboardFormatAvailable(CF_DIBV5)) {
        auto memory = GetClipboardData(CF_DIBV5);
        locked_memory lock(memory);
        const size_t capacity = memory ? GlobalSize(memory) : 0;
        if (lock.bytes && capacity >= sizeof(BITMAPV5HEADER)) {
            BITMAPV5HEADER h;
            std::memcpy(&h, lock.bytes, sizeof(h));
            const auto height = static_cast<UINT>(h.bV5Height < 0 ? -int64_t(h.bV5Height) : h.bV5Height);
            if (h.bV5Size == sizeof(h) && !h.bV5ClrUsed && !h.bV5ProfileSize && h.bV5Planes == 1 &&
                h.bV5BitCount == 32 && h.bV5Compression == BI_BITFIELDS && h.bV5RedMask == 0x00ff0000 &&
                h.bV5GreenMask == 0x0000ff00 && h.bV5BlueMask == 0x000000ff && h.bV5AlphaMask == 0xff000000 &&
                h.bV5Width > 0 && valid_size(h.bV5Width, height)) {
                const UINT stride = UINT(h.bV5Width) * 4;
                if (uint64_t(stride) * height <= capacity - sizeof(h)) {
                    out.width = h.bV5Width;
                    out.height = height;
                    out.stride = stride;
                    out.alpha = true;
                    out.pixels.resize(size_t(stride) * height);
                    const auto* bits = static_cast<const BYTE*>(lock.bytes) + sizeof(h);
                    for (UINT y = 0; y < height; ++y) {
                        const UINT row = h.bV5Height < 0 ? y : (height - 1 - y);
                        std::memcpy(out.pixels.data() + size_t(y) * stride, bits + size_t(row) * stride, stride);
                    }
                    return 200;
                }
            }
        }
    }
    // Windows synthesizes CF_BITMAP from the other standard DIB formats, including paletted images.
    auto bitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
    if (!bitmap) return 204;
    BITMAP bm{};
    if (!GetObjectW(bitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0 ||
        !valid_size(bm.bmWidth, bm.bmHeight)) return 413;
    out.width = bm.bmWidth;
    out.height = bm.bmHeight;
    out.stride = (out.width * 3 + 3) & ~3u;
    out.pixels.resize(size_t(out.stride) * out.height);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = bm.bmWidth;
    info.bmiHeader.biHeight = -bm.bmHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    auto dc = CreateCompatibleDC(nullptr);
    if (!dc) return 500;
    const int rows = GetDIBits(dc, bitmap, 0, out.height, out.pixels.data(), &info, DIB_RGB_COLORS);
    DeleteDC(dc);
    return rows == bm.bmHeight ? 200 : 409;
}

inline int encode(const snapshot& image, std::string& packet) {
    if (!image.png.empty()) {
        packet = "I";
        packet.append(image.png);
        return 200;
    }
    com_scope com;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    const auto& format = image.alpha ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat24bppBGR;
    auto actual = format;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
        FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) ||
        FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
        FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
        FAILED(encoder->CreateNewFrame(&frame, nullptr)) || FAILED(frame->Initialize(nullptr)) ||
        FAILED(frame->SetSize(image.width, image.height)) || FAILED(frame->SetPixelFormat(&actual)) ||
        actual != format || FAILED(frame->WritePixels(image.height, image.stride, UINT(image.pixels.size()),
                                                      const_cast<BYTE*>(image.pixels.data()))) ||
        FAILED(frame->Commit()) || FAILED(encoder->Commit())) return 500;
    STATSTG stat{};
    HGLOBAL memory = nullptr;
    if (FAILED(stream->Stat(&stat, STATFLAG_NONAME)) || FAILED(GetHGlobalFromStream(stream.Get(), &memory))) return 500;
    if (!stat.cbSize.QuadPart || stat.cbSize.QuadPart >= max_packet) return 413;
    locked_memory lock(memory);
    if (!lock.bytes) return 500;
    packet = "I";
    packet.append(static_cast<const char*>(lock.bytes), size_t(stat.cbSize.QuadPart));
    return 200;
}

// Offer the registered PNG alongside CF_DIBV5 under the same clipboard lock.
inline void offer_png(std::string_view png) {
    if (png.empty()) return;
    auto memory = GlobalAlloc(GMEM_MOVEABLE, png.size());
    if (!memory) return;
    {
        locked_memory lock(memory);
        if (lock.bytes) std::memcpy(lock.bytes, png.data(), png.size());
        else { GlobalFree(memory); return; }
    }
    if (!SetClipboardData(png_format(), memory)) GlobalFree(memory);
}

// Publish a standard top-down BGRA DIB with explicit alpha for native Windows apps.
inline int decode(std::string_view png, HGLOBAL& result) {
    if (png.empty() || png.size() >= max_packet || png_size(png) != png.size()) return 400;
    com_scope com;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateStream(&stream)) || FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(factory->CreateDecoder(GUID_ContainerFormatPng, nullptr, &decoder))) return 500;
    UINT width = 0, height = 0;
    if (FAILED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<char*>(png.data())), DWORD(png.size()))) ||
        FAILED(decoder->Initialize(stream.Get(), WICDecodeMetadataCacheOnDemand)) ||
        FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)) || !valid_size(width, height) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                                     nullptr, 0.0, WICBitmapPaletteTypeCustom))) return 400;
    const UINT pixelBytes = width * height * 4;
    auto memory = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPV5HEADER) + size_t(pixelBytes));
    if (!memory) return 500;
    int status = 500;
    {
        locked_memory lock(memory);
        if (lock.bytes) {
            BITMAPV5HEADER h{};
            h.bV5Size = sizeof(h);
            h.bV5Width = LONG(width);
            h.bV5Height = -LONG(height);
            h.bV5Planes = 1;
            h.bV5BitCount = 32;
            h.bV5Compression = BI_BITFIELDS;
            h.bV5SizeImage = pixelBytes;
            h.bV5RedMask = 0x00ff0000;
            h.bV5GreenMask = 0x0000ff00;
            h.bV5BlueMask = 0x000000ff;
            h.bV5AlphaMask = 0xff000000;
            h.bV5CSType = LCS_sRGB;
            h.bV5Intent = LCS_GM_IMAGES;
            std::memcpy(lock.bytes, &h, sizeof(h));
            status = SUCCEEDED(converter->CopyPixels(nullptr, width * 4, pixelBytes,
                                                     static_cast<BYTE*>(lock.bytes) + sizeof(h))) ? 200 : 400;
        }
    }
    if (status == 200) result = memory;
    else GlobalFree(memory);
    return status;
}
} // namespace clipboard_image
