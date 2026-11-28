#pragma once

#include "fileconflict.h"

#include <QDateTime>
#include <QObject>
#include <QPoint>
#include <QQueue>
#include <QSslConfiguration>
#include <QThreadPool>
#include <QThreadStorage>
#include <QUrl>

#include <atomic>
#include <functional>
#include <future>
#include <memory>

class QNetworkAccessManager;

struct TransferEntry
{
    QString name;
    QString path;
    bool directory = false;
    qint64 size = 0;
    QDateTime modified;
    QString replaceVersion; // Only a writable destination listing supplies a replacement version.
};
Q_DECLARE_METATYPE(TransferEntry)

struct TransferPlace {
    QString kind;
    QString name;
    QString path;
};

struct FileDragLocation {
    QList<TransferEntry> entries;
    QString folder;
    QString error;
};

class FileTransferEngine : public QObject
{
    Q_OBJECT

public:
    using ConflictChoice = FileConflictChoice;
    using ConflictReply = std::function<void(QList<ConflictChoice>)>;
    using ConflictPrompt = std::function<void(bool remote, int itemCount, const QList<FileConflict>& conflicts, ConflictReply reply)>;
    FileTransferEngine(QUrl host, QSslCertificate certificate, QByteArray token, QObject* parent = nullptr,
                       bool standalone = false, ConflictPrompt prompt = {});
    ~FileTransferEngine() override;
    static QByteArray newSessionToken();

    void list(bool remote, QString path);
    static QList<TransferEntry> localDropEntries(const QList<QUrl>& urls);
    // Futures are consumed by the SDL loop, which does not dispatch Qt signals.
    std::future<FileDragLocation> remoteDragLocation(bool source, QPoint position, QList<QUrl> localFiles = {});
    std::future<QString> localDropFolder(QPoint screenPoint, quintptr window, std::function<void(QString)> finished = {});
    void loadPlaces(bool remote);
    void makeDirectory(bool remote, QString parent, QString name);
    enum class FileAction { Copy, Delete, DownloadToOpen };
    void manageFiles(FileAction action, bool remote, QList<TransferEntry> entries, QString folder,
                     std::function<void(QString)> finished);
    QList<int> enqueue(bool fromRemote, const QList<TransferEntry>& entries, const QString& destinationFolder);
    void cancel(int id);
    void discard(bool fromRemote, TransferEntry entry, QString destinationFolder, std::function<void()> finished = {});

signals:
    void placesLoaded(bool remote, QList<TransferPlace> places, QString error);
    void directoryCreated(bool remote, QString parent, QString error);
    void listed(bool remote, QString path, QList<TransferEntry> entries, QString error, bool waitingForDesktop);
    void interrupted(int id);
    void networkProgress(int id, qint64 attemptBytes);
    void progress(int id, qint64 bytes, qint64 total, QString status, bool finished);

private:
    using Flag = std::shared_ptr<std::atomic_bool>;
    struct Batch;
    struct Job {
        int id;
        bool fromRemote;
        TransferEntry entry;
        QString destination;
        std::shared_ptr<Batch> batch;
    };
    void startNext();
    QList<ConflictChoice> askFileConflicts(const Flag& cancelled, bool remote, int itemCount, const QList<FileConflict>& conflicts);

    const QUrl m_Host;
    const QSslCertificate m_Certificate;
    const QByteArray m_Token;
    const bool m_Standalone;
    const ConflictPrompt m_ConflictPrompt;
    QSslConfiguration m_Ssl;
    QThreadStorage<QNetworkAccessManager*> m_Networks;
    QThreadPool m_Transfers;
    QThreadPool m_SmallFiles;
    QThreadPool m_Lists;
    Flag m_ListCancel[2];
    Flag m_WindowClosing = std::make_shared<std::atomic_bool>(false);
    Flag m_ActiveCancel;
    QQueue<Job> m_Pending;
    int m_Active = 0;
    int m_NextId = 1;
};
