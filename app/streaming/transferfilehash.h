#pragma once

#include <qt_windows.h>
#include <QCryptographicHash>
#include <algorithm>
#include <functional>

namespace transfer_file {
inline bool seek(HANDLE file, quint64 offset)
{
    LARGE_INTEGER position;
    position.QuadPart = static_cast<LONGLONG>(offset);
    return SetFilePointerEx(file, position, nullptr, FILE_BEGIN) != FALSE;
}

// Hash a bounded prefix under the caller's pinned source handle.
inline bool hashBytes(HANDLE file, quint64 length, QCryptographicHash& hash,
                      const std::function<bool()>& cancelled)
{
    QByteArray block(1024 * 1024, Qt::Uninitialized);
    while (length) {
        if (cancelled()) return false;
        const DWORD size = static_cast<DWORD>((std::min<quint64>)(length, block.size()));
        DWORD read = 0;
        if (!ReadFile(file, block.data(), size, &read, nullptr) || read != size) return false;
        hash.addData(QByteArrayView(block.constData(), read));
        length -= read;
    }
    return true;
}
}
