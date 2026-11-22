#pragma once

#include <QStringList>

struct ClipboardUploadProgress
{
    QStringList names; // Basenames in clipboard order.
    int index = 0;
    quint64 fileBytes = 0;
    quint64 fileSize = 0;
    quint64 acknowledged = 0;
    quint64 total = 0;
    bool committing = false;
    qint64 startedAtMs = 0;
    quint64 resumedBytes = 0; // Bytes already present on the host before this attempt.
    bool preparing = false;

    int percent() const { return names.isEmpty() ? -1 : total ? int(acknowledged * 100 / total) : 0; }
};
