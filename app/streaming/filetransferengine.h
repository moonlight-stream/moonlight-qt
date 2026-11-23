#pragma once

#include <QDateTime>
#include <QObject>
#include <QPoint>
#include <QQueue>
#include <QSslConfiguration>
#include <QThreadPool>
#include <QUrl>

#include <atomic>
#include <functional>
#include <future>
#include <memory>

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
    enum class ConflictChoice { Skip, Replace, KeepBoth };
    using ConflictReply = std::function<void(ConflictChoice)>;
    using ConflictPrompt = std::function<void(bool remote, const QString& path, const QString& replaceUnavailable, ConflictReply reply)>;
    FileTransferEngine(QUrl host, QSslCertificate certificate, QByteArray token, QObject* parent = nullptr,
                       bool standalone = false, ConflictPrompt prompt = {});
    ~FileTransferEngine() override;

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
    int enqueue(bool fromRemote, TransferEntry entry, QString destinationFolder);
    void cancel(int id);
    void discard(bool fromRemote, TransferEntry entry, QString destinationFolder, std::function<void()> finished = {});

signals:
    void placesLoaded(bool remote, QList<TransferPlace> places, QString error);
    void directoryCreated(bool remote, QString parent, QString error);
    void listed(bool remote, QString path, QList<TransferEntry> entries, QString error);
    void interrupted(int id);
    void networkProgress(int id, qint64 attemptBytes);
    void progress(int id, qint64 bytes, qint64 total, QString status, bool finished);

private:
    using Flag = std::shared_ptr<std::atomic_bool>;
    struct Job {
        int id;
        bool fromRemote;
        TransferEntry entry;
        QString destination;
    };
    void startNext();
    ConflictChoice askFileConflict(const Flag& cancelled, bool remote, const QString& path, const QString& reason);

    const QUrl m_Host;
    const QSslCertificate m_Certificate;
    const QByteArray m_Token;
    const bool m_Standalone;
    const ConflictPrompt m_ConflictPrompt;
    QSslConfiguration m_Ssl;
    QThreadPool m_Transfers;
    QThreadPool m_Lists;
    Flag m_ListCancel[2];
    Flag m_WindowClosing = std::make_shared<std::atomic_bool>(false);
    Flag m_ActiveCancel;
    QQueue<Job> m_Pending;
    int m_Active = 0;
    int m_NextId = 1;
};
