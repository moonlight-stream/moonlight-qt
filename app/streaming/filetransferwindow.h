#pragma once

#include "filetransferengine.h"
#include "transferratesampler.h"

#include <QElapsedTimer>
#include <QHash>
#include <QTimer>
#include <QWidget>
#include <array>
#include <optional>

class QAction;
class QCheckBox;
class QLabel;
class QLineEdit;
class QLockFile;
class QPushButton;
class QTemporaryDir;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class TransferFileIcons;

class FileTransferWindow final : public QWidget
{
    Q_OBJECT
public:
    FileTransferWindow(QUrl host, QSslCertificate certificate, QByteArray token,
                       QString hostName, QString hostId, QWidget* parent = nullptr, bool standalone = false);
    ~FileTransferWindow() override;
    void transferTo(bool fromRemote, const QList<TransferEntry>& entries, const QString& destination);

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct Pane {
        QLineEdit* address;
        QToolButton* back;
        QToolButton* up;
        QToolButton* refresh;
        QToolButton* locations;
        QAction* bookmark;
        QToolButton* newFolder;
        QCheckBox* selectAll;
        QList<TransferPlace> places;
        QString placesError;
        QTreeWidget* files;
        QLabel* message;
        QPushButton* send;
        QAction* open;
        QAction* remove;
        QAction* copy;
        QAction* paste;
        QString path;
        QString requested;
        QStringList history;
        QString bookmarksKey;
        QString lastDirectoryKey;
        QTimer transferRefresh{nullptr};
        std::optional<QString> restoreFallback;
        bool ready = false;
        bool operating = false;
    };
    QWidget* makePane(bool remote, const QString& title);
    void restoreDirectory(bool remote, const QString& defaultPath);
    void navigate(bool remote, QString path, std::optional<QString> restoreFallback = std::nullopt);
    void showDirectory(bool remote, QString path, const QList<TransferEntry>& entries, QString error, bool waitingForDesktop);
    QList<TransferEntry> selection(bool remote) const;
    void updateActions();
    void showLocations(bool remote);
    void openEntry(bool remote, const TransferEntry& entry);
    void openLocalFile(const QString& path);
    void manageSelection(FileTransferEngine::FileAction action, bool remote, const QList<TransferEntry>& entries);
    bool hasPendingWork() const;
    void closeWhenIdle();
    void copySelection(bool remote);
    void pasteSelection(bool remote);
    void transfer(bool fromRemote, const QList<TransferEntry>& entries);
    QTreeWidgetItem* addTask(bool fromRemote, const TransferEntry& entry, const QString& destination,
                             const QString& key, qint64 transferred);
    void restoreTasks();
    void saveTask(QTreeWidgetItem* item);
    void startTasks(const QList<QTreeWidgetItem*>& items);
    void cancelTask(QTreeWidgetItem* item);
    std::shared_ptr<QLockFile> claimTask(const QString& key);
    bool takeTask(QTreeWidgetItem* item);
    void refreshSpeed();
    void showProgress(int id, qint64 bytes, qint64 total, const QString& status, bool finished);

    FileTransferEngine* m_Engine;
    TransferFileIcons* m_FileIcons;
    std::array<Pane, 2> m_Panes{};
    QTreeWidget* m_Queue;
    QPushButton* m_Cancel;
    QPushButton* m_Resume;
    QPushButton* m_Clear;
    enum class State { Active, Interrupted, Cancelling, Finished };
    struct Task {
        TransferEntry entry;
        QString destination, key;
        bool fromRemote;
        qint64 bytes;
        int id = 0;
        State state = State::Interrupted;
        bool retryable = false;
        bool remembered = false;
        std::shared_ptr<QLockFile> lock;
    };
    QString m_QueueSettings, m_LockDirectory;
    QHash<QTreeWidgetItem*, Task> m_Tasks;
    QHash<int, QTreeWidgetItem*> m_Running;
    QElapsedTimer m_SaveTimer;
    QTimer* m_SpeedTimer;
    QTimer m_RemoteRetryTimer;
    int m_RemoteRetryDelay = 0;
    TransferRateSampler m_Speed;
    int m_SpeedJob = 0;
    qint64 m_AttemptBytes = 0;
    QByteArray m_ClipboardId;
    QList<TransferEntry> m_Copied;
    std::unique_ptr<QTemporaryDir> m_OpenCopies;
    bool m_CopiedRemote = false;
    bool m_Available;
    bool m_Standalone;
    bool m_CloseAfterTransfers = false;
};
