#pragma once

#include "filetransferengine.h"
#include "transferratesampler.h"

#include <QElapsedTimer>
#include <QHash>
#include <QWidget>
#include <array>

class QAction;
class QCheckBox;
class QLabel;
class QLineEdit;
class QLockFile;
class QPushButton;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

class FileTransferWindow final : public QWidget
{
    Q_OBJECT
public:
    FileTransferWindow(QUrl host, QSslCertificate certificate, QByteArray token,
                       QString hostName, QString hostId, QWidget* parent = nullptr, bool standalone = false);
    ~FileTransferWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    struct Pane {
        QLineEdit* address;
        QToolButton* back;
        QToolButton* up;
        QToolButton* refresh;
        QToolButton* locations;
        QToolButton* newFolder;
        QCheckBox* selectAll;
        QList<TransferPlace> places;
        QString placesError;
        QTreeWidget* files;
        QLabel* message;
        QPushButton* send;
        QAction* copy;
        QAction* paste;
        QString path;
        QString requested;
        QStringList history;
        QString settingsKey;
        bool ready = false;
    };
    QWidget* makePane(bool remote, const QString& title);
    void navigate(bool remote, QString path);
    void showDirectory(bool remote, QString path, QList<TransferEntry> entries, QString error);
    QList<TransferEntry> selection(bool remote) const;
    void updateActions();
    void showLocations(bool remote);
    void copySelection(bool remote);
    void pasteSelection(bool remote);
    void transfer(bool fromRemote, const QList<TransferEntry>& entries);
    QTreeWidgetItem* addTask(bool fromRemote, const TransferEntry& entry, const QString& destination,
                             const QString& key, qint64 transferred);
    void restoreTasks();
    void saveTask(QTreeWidgetItem* item);
    void startTask(QTreeWidgetItem* item);
    void cancelTask(QTreeWidgetItem* item);
    std::shared_ptr<QLockFile> claimTask(const QString& key);
    bool takeTask(QTreeWidgetItem* item);
    void refreshSpeed();
    void showProgress(int id, qint64 bytes, qint64 total, const QString& status, bool finished);

    FileTransferEngine* m_Engine;
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
    TransferRateSampler m_Speed;
    int m_SpeedJob = 0;
    qint64 m_AttemptBytes = 0;
    QByteArray m_ClipboardId;
    QList<TransferEntry> m_Copied;
    bool m_CopiedRemote = false;
    bool m_Available;
    bool m_Standalone;
    bool m_CloseAfterTransfers = false;
};
