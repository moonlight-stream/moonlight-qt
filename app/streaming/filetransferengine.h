#pragma once

#include <QDateTime>
#include <QObject>
#include <QQueue>
#include <QSslConfiguration>
#include <QThreadPool>
#include <QUrl>

#include <atomic>
#include <functional>
#include <memory>

struct TransferEntry
{
    QString name;
    QString path;
    bool directory = false;
    qint64 size = 0;
    QDateTime modified;
};
Q_DECLARE_METATYPE(TransferEntry)

struct TransferPlace {
    QString kind;
    QString name;
    QString path;
};

class FileTransferEngine : public QObject
{
    Q_OBJECT

public:
    FileTransferEngine(QUrl host, QSslCertificate certificate, QByteArray token, QObject* parent = nullptr,
                       bool standalone = false);
    ~FileTransferEngine() override;

    void list(bool remote, QString path);
    void loadPlaces(bool remote);
    void makeDirectory(bool remote, QString parent, QString name);
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

    const QUrl m_Host;
    const QSslCertificate m_Certificate;
    const QByteArray m_Token;
    const bool m_Standalone;
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
