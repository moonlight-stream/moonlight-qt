// Windows clipboard wire format shared with the Sunshine desktop clipboard endpoint.
#pragma once

#include "clipboard_image.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace win_clipboard {
namespace fs = std::filesystem;
constexpr size_t max_body = 16 * 1024 * 1024;
constexpr size_t max_text = 1024 * 1024;
constexpr size_t max_files = 32;

namespace detail {
struct clipboard_lock {
    bool opened;
    explicit clipboard_lock(HWND owner) : opened(OpenClipboard(owner) != FALSE) {}
    ~clipboard_lock() { if (opened) CloseClipboard(); }
};

inline std::wstring from_utf8(std::string_view value) {
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(count, L'\0');
    if (count) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count);
    return result;
}

inline std::string to_utf8(std::wstring_view value) {
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(count, '\0');
    if (count) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
    return result;
}

inline bool valid_name(std::wstring_view name) {
    if (name.empty() || name.size() > 255 || name.back() == '.' || name.back() == ' ') return false;
    for (wchar_t ch : name) {
        if (ch < 32 || std::wcschr(L"\\/:*?\"<>|", ch)) return false;
    }
    auto stem = name.substr(0, name.find('.'));
    while (!stem.empty() && stem.back() == ' ') stem.remove_suffix(1);
    std::wstring upper(stem);
    for (auto& ch : upper) if (ch >= L'a' && ch <= L'z') ch -= L'a' - L'A';
    if (upper == L"CON" || upper == L"PRN" || upper == L"AUX" || upper == L"NUL" ||
        upper == L"CONIN$" || upper == L"CONOUT$" || upper == L"CLOCK$") return false;
    return !(upper.size() == 4 && (upper.compare(0, 3, L"COM") == 0 || upper.compare(0, 3, L"LPT") == 0) &&
             std::wcschr(L"0123456789\u00b9\u00b2\u00b3", upper[3]));
}

inline void append_size(std::string& out, uint32_t size) {
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<char>(size >> shift));
}

inline bool take_field(std::string_view& in, std::string_view& value) {
    if (in.size() < 4) return false;
    uint32_t size = 0;
    for (unsigned i = 0; i < 4; ++i) size = (size << 8) | static_cast<unsigned char>(in[i]);
    in.remove_prefix(4);
    if (in.size() < size) return false;
    value = in.substr(0, size);
    in.remove_prefix(size);
    return true;
}

inline bool local_file_path(fs::path path) {
    path.make_preferred();
    auto root = path.root_path().native();
    if (root.size() != 3 || root[1] != L':' || root[2] != L'\\') return false;
    auto drive = GetDriveTypeW(root.c_str());
    if (drive != DRIVE_FIXED && drive != DRIVE_REMOVABLE && drive != DRIVE_RAMDISK) return false;
    fs::path prefix = path.root_path();
    for (const auto& component : path.relative_path()) {
        if (!valid_name(component.native())) return false;
        prefix /= component;
        const auto attrs = GetFileAttributesW(prefix.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES || attrs & FILE_ATTRIBUTE_REPARSE_POINT) return false;
    }
    return true;
}

struct close_handle {
    void operator()(HANDLE handle) const { CloseHandle(handle); }
};
using file_handle = std::unique_ptr<void, close_handle>;

// Read from the selected file even when Explorer renames it during a transfer.
struct source_file {
    file_handle handle;
    std::string name;
    uint64_t size;
};

inline std::optional<source_file> open_file(const fs::path& path) {
    if (!local_file_path(path)) return std::nullopt;
    HANDLE raw = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                             FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (raw == INVALID_HANDLE_VALUE) return std::nullopt;
    file_handle handle(raw);
    BY_HANDLE_FILE_INFORMATION info{};
    if (GetFileType(raw) != FILE_TYPE_DISK || !GetFileInformationByHandle(raw, &info) ||
        info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) return std::nullopt;
    auto name = to_utf8(path.filename().native());
    if (name.empty()) return std::nullopt;
    const uint64_t size = (uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
    return source_file{std::move(handle), std::move(name), size};
}

inline std::vector<fs::path> file_paths(HDROP drop, UINT count) {
    std::vector<fs::path> paths;
    for (UINT i = 0; i < count; ++i) {
        UINT length = DragQueryFileW(drop, i, nullptr, 0);
        if (!length || length > 32767) return {};
        std::wstring path(length + 1, L'\0');
        if (DragQueryFileW(drop, i, path.data(), length + 1) != length) return {};
        path.resize(length);
        paths.emplace_back(std::move(path));
    }
    return paths;
}

inline int read_text(std::string& out) {
    auto mem = GetClipboardData(CF_UNICODETEXT);
    if (!mem) return 409;
    const auto capacity = (std::min)(GlobalSize(mem) / sizeof(wchar_t), max_text + 1);
    const auto data = static_cast<const wchar_t*>(GlobalLock(mem));
    if (!data) return 409;
    const auto end = std::wmemchr(data, L'\0', capacity);
    std::string value;
    if (end) value = to_utf8({data, static_cast<size_t>(end - data)});
    bool valid = end && (end == data || !value.empty()) && value.size() <= max_text;
    GlobalUnlock(mem);
    if (!valid) return 413;
    out = "T";
    out += value;
    return 200;
}

struct staged_batch {
    fs::path directory;
    file_handle directoryPin;
    std::vector<fs::path> paths;
    bool published = false;
    ~staged_batch() {
        if (published || directory.empty()) return;
        for (const auto& path : paths) DeleteFileW(path.c_str());
        directoryPin.reset();
        RemoveDirectoryW(directory.c_str());
    }
};

inline bool batch_name(const std::wstring& name) {
    if (name.size() != 38 || name.front() != L'{' || name.back() != L'}') return false;
    for (size_t i = 1; i < 37; ++i) {
        if (i == 9 || i == 14 || i == 19 || i == 24) { if (name[i] != '-') return false; }
        else if (!std::wcschr(L"0123456789ABCDEFabcdef", name[i])) return false;
    }
    return true;
}

// Keep recent paste sources across disconnects. Under disk pressure also
// reclaim recent batches that neither Explorer nor another transfer references.
inline void expire_batches(const fs::path& root, const fs::path& current, size_t keep = 16) {
    clipboard_lock clipboard(nullptr);
    if (!clipboard.opened) return;
    std::vector<fs::path> inUse{fs::path(current).make_preferred()};
    if (IsClipboardFormatAvailable(CF_HDROP)) {
        auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
        if (!drop) return;
        const auto count = DragQueryFileW(drop, 0xffffffff, nullptr, 0);
        if (count > max_files) return;
        auto paths = file_paths(drop, count);
        if (paths.empty()) return;
        for (auto& path : paths) inUse.push_back(path.make_preferred().parent_path());
    }
    const auto cutoff = fs::file_time_type::clock::now() - std::chrono::hours(24);
    std::error_code ec;
    std::vector<std::pair<fs::file_time_type, fs::path>> previous;
    for (fs::directory_iterator dir(fs::path(root).make_preferred(), ec), end; !ec && dir != end; dir.increment(ec)) {
        const auto& path = dir->path();
        const auto attrs = GetFileAttributesW(path.c_str());
        const bool referenced = std::any_of(inUse.begin(), inUse.end(), [&](const fs::path& active) {
            return CompareStringOrdinal(path.c_str(), -1, active.c_str(), -1, TRUE) == CSTR_EQUAL;
        });
        if (referenced || attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY) ||
            attrs & FILE_ATTRIBUTE_REPARSE_POINT || !batch_name(path.filename().native())) continue;
        std::error_code batch_error;
        auto modified = fs::last_write_time(path, batch_error);
        if (!batch_error) previous.emplace_back(modified, path);
    }
    std::sort(previous.rbegin(), previous.rend());
    size_t retained = 1;
    for (const auto& [modified, path] : previous) {
        if (modified >= cutoff && retained < keep) { ++retained; continue; }
        HANDLE handle = CreateFileW(path.c_str(), DELETE | FILE_READ_ATTRIBUTES,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE) continue;
        file_handle pin(handle);
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(handle, &info) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        std::error_code batch_error;
        const fs::directory_iterator end;
        for (fs::directory_iterator item(path, batch_error); !batch_error && item != end; item.increment(batch_error)) {
            const auto child_attrs = GetFileAttributesW(item->path().c_str());
            if (child_attrs != INVALID_FILE_ATTRIBUTES && !(child_attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
                DeleteFileW(item->path().c_str());
        }
        FILE_DISPOSITION_INFO remove{TRUE};
        SetFileInformationByHandle(handle, FileDispositionInfo, &remove, sizeof(remove));
    }
}

inline int create_batch(const fs::path& root, staged_batch& batch) {
    if (root.empty() || !root.is_absolute()) return 400;
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec || !local_file_path(root)) return 500;
    GUID guid;
    wchar_t unique[39]{};
    if (FAILED(CoCreateGuid(&guid)) || !StringFromGUID2(guid, unique, 39)) return 500;
    auto directory = root / unique;
    // Qt cache paths use forward slashes. Explorer expects native paths in CF_HDROP.
    directory.make_preferred();
    if (!CreateDirectoryW(directory.c_str(), nullptr)) return 500;
    batch.directory = std::move(directory);
    HANDLE handle = CreateFileW(batch.directory.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return 500;
    batch.directoryPin.reset(handle);
    return 200;
}

inline int stage_files(std::string_view body, const fs::path& root, staged_batch& batch) {
    if (body.empty()) return 400;
    int status = create_batch(root, batch);
    if (status != 200) return status;
    while (!body.empty()) {
        std::string_view name, bytes;
        if (batch.paths.size() >= max_files || !take_field(body, name) || name.size() > 1020 || !take_field(body, bytes)) return 400;
        std::wstring basename = from_utf8(name);
        if (!valid_name(basename)) return 400;
        fs::path path = batch.directory / basename;
        HANDLE raw = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (raw == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_EXISTS ? 400 : 500;
        file_handle file(raw);
        batch.paths.push_back(std::move(path));
        DWORD written = 0;
        if (!bytes.empty() && (!WriteFile(raw, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) || written != bytes.size())) return 500;
    }
    return 200;
}

inline HGLOBAL make_data(std::wstring_view chars, bool files) {
    size_t header = files ? sizeof(DROPFILES) : 0;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, header + (chars.size() + 1) * sizeof(wchar_t));
    if (!memory) return nullptr;
    auto dest = static_cast<unsigned char*>(GlobalLock(memory));
    if (!dest) { GlobalFree(memory); return nullptr; }
    if (files) {
        auto drop = reinterpret_cast<DROPFILES*>(dest);
        drop->pFiles = sizeof(DROPFILES);
        drop->fWide = TRUE;
    }
    CopyMemory(dest + header, chars.data(), chars.size() * sizeof(wchar_t));
    GlobalUnlock(memory);
    return memory;
}

} // namespace detail

inline int encode_files(const std::vector<detail::source_file>& sources, std::string& payload) {
    size_t encodedSize = 1;
    for (const auto& file : sources) {
        if (file.size > max_body || 8 + file.name.size() + file.size > max_body - encodedSize) return 413;
        encodedSize += 8 + file.name.size() + static_cast<size_t>(file.size);
    }
    payload = "F";
    payload.reserve(encodedSize);
    for (const auto& source : sources) {
        const DWORD size = static_cast<DWORD>(source.size);
        detail::append_size(payload, static_cast<uint32_t>(source.name.size()));
        payload.append(source.name);
        detail::append_size(payload, size);
        const auto offset = payload.size();
        payload.resize(offset + size);
        DWORD read = 0;
        if (size && (!ReadFile(source.handle.get(), payload.data() + offset, size, &read, nullptr) || read != size)) return 409;
    }
    return 200;
}

inline int read(bool text, bool files, std::optional<DWORD> since, std::string& payload, DWORD& sequence,
                std::vector<detail::source_file>* pinnedFiles = nullptr, bool images = false) {
    payload.clear();
    std::vector<fs::path> paths;
    clipboard_image::snapshot image;
    {
        detail::clipboard_lock lock(nullptr);
        if (!lock.opened) return 409;
        sequence = GetClipboardSequenceNumber();
        if (sequence == since) return 204;
        if (IsClipboardFormatAvailable(CF_HDROP)) {
            if (!files) return 204;
            auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
            if (!drop) return 409;
            const UINT count = DragQueryFileW(drop, 0xffffffff, nullptr, 0);
            if (count > max_files) return 413;
            paths = detail::file_paths(drop, count);
            if (paths.empty()) return 204;
        } else if (images && clipboard_image::available()) {
            const int status = clipboard_image::capture(image);
            if (status != 200) return status;
        } else if (text && IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            return detail::read_text(payload);
        } else return 204;
    }
    if (paths.empty()) {
        const int status = clipboard_image::encode(image, payload);
        return GetClipboardSequenceNumber() == sequence ? status : 409;
    }
    std::vector<detail::source_file> sources;
    for (const auto& path : paths) {
        auto source = detail::open_file(path);
        if (!source) return 204;
        sources.emplace_back(std::move(*source));
    }
    if (pinnedFiles) {
        *pinnedFiles = std::move(sources);
        return 200;
    }
    if (GetClipboardSequenceNumber() != sequence) return 409;
    const int status = encode_files(sources, payload);
    return GetClipboardSequenceNumber() == sequence ? status : 409;
}

inline int publish_data(UINT format, HGLOBAL data, DWORD& sequence, std::optional<DWORD> expected, std::string_view png = {}) {
    if (!data) return 500;
    // EmptyClipboard requires a real clipboard owner to publish data.
    HWND owner = CreateWindowExW(0, L"STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    int status = 409;
    if (owner) {
        detail::clipboard_lock lock(owner);
        if (lock.opened && (!expected || *expected == GetClipboardSequenceNumber()) &&
            EmptyClipboard() && SetClipboardData(format, data)) {
            data = nullptr;
            clipboard_image::offer_png(png);
            sequence = GetClipboardSequenceNumber();
            status = 204;
        }
    }
    if (owner && status == 204) {
        // Windows may synthesize additional text formats when CloseClipboard
        // publishes the update. Record the final sequence under a fresh lock.
        detail::clipboard_lock lock(owner);
        if (lock.opened && GetClipboardOwner() == owner) sequence = GetClipboardSequenceNumber();
    }
    if (owner) DestroyWindow(owner);
    if (data) GlobalFree(data);
    return status;
}

inline int publish_files(detail::staged_batch& batch, const fs::path& stagingRoot,
                         DWORD& sequence, std::optional<DWORD> expected) {
    std::wstring paths;
    for (const auto& path : batch.paths) {
        paths += path.native();
        paths.push_back(L'\0');
    }
    const int status = publish_data(CF_HDROP, detail::make_data(paths, true), sequence, expected);
    if (status == 204) {
        batch.published = true;
        detail::expire_batches(stagingRoot, batch.directory);
    }
    return status;
}

inline int write(bool text, bool files, std::string_view payload, DWORD& sequence,
                 const fs::path& staging_root, std::optional<DWORD> expected = std::nullopt, bool images = false) {
    if (payload.empty()) return 400;
    if (payload.front() == 'I') {
        if (!images) return 403;
        if (payload.size() > clipboard_image::max_packet) return 413;
        HGLOBAL bitmap = nullptr;
        const auto png = payload.substr(1);
        const int status = clipboard_image::decode(png, bitmap);
        return status == 200 ? publish_data(CF_DIBV5, bitmap, sequence, expected, png) : status;
    }
    if (payload.size() > max_body) return 413;
    const char format = payload.front();
    payload.remove_prefix(1);
    detail::staged_batch batch;
    std::wstring value;
    if (format == 'T') {
        if (!text) return 403;
        if (payload.size() > max_text) return 413;
        value = detail::from_utf8(payload);
        if ((value.empty() && !payload.empty()) || value.find(L'\0') != std::wstring::npos) return 400;
    } else if (format == 'F') {
        if (!files) return 403;
        int status = detail::stage_files(payload, staging_root, batch);
        if (status != 200) return status;
        return publish_files(batch, staging_root, sequence, expected);
    } else return 400;

    return publish_data(CF_UNICODETEXT, detail::make_data(value, false), sequence, expected);
}
} // namespace win_clipboard
