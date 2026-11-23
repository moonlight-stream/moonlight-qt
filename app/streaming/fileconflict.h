#pragma once

#include <QList>
#include <QString>
#include <functional>
#include <optional>

enum class FileConflictChoice { Skip, Replace, KeepBoth };

struct FileConflict {
    QString path;
    QString replaceUnavailable;
};

// One choice per displayed conflict, in manifest order. Cancellation has no answer.
using ClipboardConflictPrompt = std::function<std::optional<QList<FileConflictChoice>>(
    int itemCount, const QList<FileConflict>& conflicts)>;

class QDialog;
class QWidget;
QDialog* showFileConflicts(QWidget* parent, bool remote, int itemCount, const QList<FileConflict>& conflicts,
                           std::function<void(const QList<FileConflictChoice>&)> reply);
