#pragma once

#include <QStringList>

struct ClipboardUploadProgress
{
    enum class State { Queued, Uploading, WaitingForPaste, NeedsPaste, Failed };
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

    quint64 taskId = 0;
    State state = State::Uploading;
    QString error;

    bool operator==(const ClipboardUploadProgress& other) const {
        return taskId == other.taskId && state == other.state && error == other.error &&
               names == other.names && index == other.index && fileBytes == other.fileBytes &&
               fileSize == other.fileSize && acknowledged == other.acknowledged && total == other.total &&
               committing == other.committing && preparing == other.preparing &&
               startedAtMs == other.startedAtMs && resumedBytes == other.resumedBytes;
    }

    int percent() const { return names.isEmpty() ? -1 : total ? int(acknowledged * 100 / total) : 0; }
};
