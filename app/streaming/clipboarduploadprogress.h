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

    int percent() const { return names.isEmpty() ? -1 : total ? int(acknowledged * 100 / total) : 0; }
};
