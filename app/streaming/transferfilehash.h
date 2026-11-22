#pragma once

#include <qt_windows.h>
#include <QCryptographicHash>
#include <algorithm>
#include <functional>
#include <memory>
#include <openssl/evp.h>

namespace transfer_file {
inline bool seek(HANDLE file, quint64 offset)
{
    LARGE_INTEGER position;
    position.QuadPart = static_cast<LONGLONG>(offset);
    return SetFilePointerEx(file, position, nullptr, FILE_BEGIN) != FALSE;
}

// Read exactly the requested bytes from the pinned handle's current position.
template<typename Consume>
bool readBlocks(HANDLE file, quint64 length, const std::function<bool()>& cancelled, Consume consume)
{
    QByteArray block(1024 * 1024, Qt::Uninitialized);
    while (length) {
        if (cancelled()) return false;
        const DWORD size = static_cast<DWORD>((std::min<quint64>)(length, block.size()));
        DWORD read = 0;
        if (!ReadFile(file, block.data(), size, &read, nullptr) || read != size) return false;
        if (!consume(QByteArrayView(block.constData(), read))) return false;
        length -= read;
    }
    return true;
}

inline bool hashBytes(HANDLE file, quint64 length, QCryptographicHash& hash,
                      const std::function<bool()>& cancelled)
{
    return readBlocks(file, length, cancelled, [&](QByteArrayView block) {
        hash.addData(block);
        return true;
    });
}

// OpenSSL selects the accelerated SHA-256 implementation available on this CPU.
// A complete SHA-256, including that of a zero-byte file, is always 32 bytes.
inline QByteArray sha256(HANDLE file, quint64 length, const std::function<bool()>& cancelled)
{
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) return {};
    if (!readBlocks(file, length, cancelled, [&](QByteArrayView block) {
        return EVP_DigestUpdate(ctx.get(), block.data(), block.size()) == 1;
    })) return {};
    QByteArray digest(32, Qt::Uninitialized);
    unsigned int size = 0;
    if (EVP_DigestFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(digest.data()), &size) != 1 ||
        size != static_cast<unsigned int>(digest.size())) return {};
    return digest;
}
}
