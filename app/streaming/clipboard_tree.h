/**
 * @file clipboard_tree.h
 * @brief Windows clipboard folder selections shared by Moonlight and Sunshine.
 */
#pragma once

#include "clipboard_win32.h"

#include <functional>
#include <map>

namespace win_clipboard::tree {
constexpr size_t max_entries = 4096; ///< Includes directories and their descendants.
constexpr size_t max_manifest = 1024 * 1024; ///< Maximum encoded metadata for a selection.

struct ordinal_less {
    bool operator()(const std::wstring& a, const std::wstring& b) const {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    }
};

/** @brief Validate a parent-first list of relative names. A final slash denotes a directory. */
struct layout {
    std::map<std::wstring, bool, ordinal_less> entries;
    size_t roots = 0;

    bool add(std::wstring wire, fs::path& relative, bool& directory) {
        directory = !wire.empty() && wire.back() == L'/';
        if (directory) wire.pop_back();
        if (entries.size() >= max_entries || wire.empty() || wire.front() == L'/' ||
            wire.size() >= 32767 || wire.find(L'\\') != wire.npos || wire.find(L'\0') != wire.npos) return false;
        const auto slash = wire.rfind(L'/');
        if (!detail::valid_name(std::wstring_view(wire).substr(slash == wire.npos ? 0 : slash + 1))) return false;
        if (slash == wire.npos) {
            if (++roots > max_files) return false;
        } else {
            const auto parent = entries.find(wire.substr(0, slash));
            if (parent == entries.end() || !parent->second) return false;
        }
        relative = fs::path(wire).make_preferred();
        return entries.emplace(std::move(wire), directory).second;
    }
};

/** @brief Pin an ordinary directory while looking up its children. */
inline bool pin_folder(const fs::path& path, detail::file_handle& handle) {
    auto raw = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (raw == INVALID_HANDLE_VALUE) return false;
    handle.reset(raw);
    BY_HANDLE_FILE_INFORMATION info{};
    return GetFileInformationByHandle(raw, &info) && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
           !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

/** @brief Resolve aliases and drive mappings before checking whether a folder copy would enter its source. */
inline std::wstring folder_path(HANDLE directory) {
    const DWORD length = GetFinalPathNameByHandleW(directory, nullptr, 0, VOLUME_NAME_GUID);
    if (!length) return {};
    std::wstring path(length, L'\0');
    const DWORD written = GetFinalPathNameByHandleW(directory, path.data(), length, VOLUME_NAME_GUID);
    if (!written || written >= length) return {};
    path.resize(written);
    return path;
}

using visitor = std::function<void(const fs::path&, const std::wstring&, const BY_HANDLE_FILE_INFORMATION&, detail::file_handle&)>;

/** @brief Enumerate a selected object with its ancestors pinned. Only regular-file handles may be moved by visit. */
inline int walk_entry(const fs::path& path, std::wstring name, bool folders, bool metadata, unsigned depth,
                      layout& names, size_t& chars, const visitor& visit) {
    if (depth > 128 || names.entries.size() >= max_entries || (chars += name.size()) > max_manifest) return 413;
    auto attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) return 409;
    bool directory = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (directory && !folders) return 204;
    if (directory) name += L'/';
    fs::path relative;
    if (!names.add(name, relative, directory)) return 400;
    const auto share = directory ? FILE_SHARE_READ | FILE_SHARE_WRITE :
                       FILE_SHARE_READ | FILE_SHARE_DELETE | (metadata ? FILE_SHARE_WRITE : 0);
    auto raw = CreateFileW(path.c_str(), metadata || directory ? FILE_READ_ATTRIBUTES : GENERIC_READ, share,
                           nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS |
                           FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (raw == INVALID_HANDLE_VALUE) return 409;
    detail::file_handle handle(raw);
    BY_HANDLE_FILE_INFORMATION info{};
    if (GetFileType(raw) != FILE_TYPE_DISK || !GetFileInformationByHandle(raw, &info) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || directory != bool(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return 409;
    visit(path, name, info, handle);
    if (directory) {
        std::error_code ec;
        for (fs::directory_iterator it(path, ec), end; !ec && it != end; it.increment(ec)) {
            const auto& child = it->path();
            if (!detail::valid_name(child.filename().native())) return 400;
            const auto result = walk_entry(child, name + child.filename().native(), folders, metadata, depth + 1, names, chars, visit);
            if (result != 200) return result;
        }
        if (ec) return 409;
    }
    return 200;
}

/** @brief Snapshot only the local paths selected in CF_HDROP, without following reparse points. */
inline int walk(const std::vector<fs::path>& paths, bool folders, bool metadata, const visitor& visit) {
    if (paths.empty()) return 204;
    if (paths.size() > max_files) return 413;
    layout names;
    size_t chars = 0;
    for (const auto& path : paths) {
        if (!detail::local_file_path(path) || !detail::valid_name(path.filename().native())) return 204;
        auto prefix = path.root_path();
        std::vector<detail::file_handle> parents;
        for (const auto& part : path.parent_path().relative_path()) {
            prefix /= part;
            if (!pin_folder(prefix, parents.emplace_back())) return 409;
        }
        const auto status = walk_entry(path, path.filename().native(), folders, metadata, 0, names, chars, visit);
        if (status != 200) return status;
    }
    return 200;
}
} // namespace win_clipboard::tree
