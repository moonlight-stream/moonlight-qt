#pragma once

#include "nvcomputer.h"
#include "nvpairingmanager.h"

#include <qmdnsengine/server.h>
#include <qmdnsengine/cache.h>
#include <qmdnsengine/browser.h>
#include <qmdnsengine/service.h>
#include <qmdnsengine/resolver.h>

#include <QThread>
#include <QReadWriteLock>
#include <QSettings>
#include <QRunnable>

class MdnsPendingComputer : public QObject
{
    Q_OBJECT

public:
    explicit MdnsPendingComputer(QMdnsEngine::Server* server,
                                 QMdnsEngine::Cache* cache,
                                 const QMdnsEngine::Service& service)
        : m_Hostname(service.hostname()),
          m_Resolver(server, m_Hostname, cache)
    {
        connect(&m_Resolver, SIGNAL(resolved(QHostAddress)),
                this, SLOT(handleResolved(QHostAddress)));
    }

    QString hostname()
    {
        return m_Hostname;
    }

private slots:
    void handleResolved(const QHostAddress& address)
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            m_Resolver.disconnect();
            emit resolvedv4(this, address);
        }
    }

signals:
    void resolvedv4(MdnsPendingComputer*, const QHostAddress&);

private:
    QByteArray m_Hostname;
    QMdnsEngine::Resolver m_Resolver;
};

class ComputerPollingEntry
{
public:
    ComputerPollingEntry()
        : m_ActiveThread(nullptr)
    {

    }

    virtual ~ComputerPollingEntry()
    {
        interrupt();

        // interrupt() should have taken care of this
        Q_ASSERT(m_ActiveThread == nullptr);

        for (QThread* thread : m_InactiveList) {
            thread->wait();
            delete thread;
        }
    }

    bool isActive()
    {
        cleanInactiveList();

        return m_ActiveThread != nullptr;
    }

    void setActiveThread(QThread* thread)
    {
        cleanInactiveList();

        Q_ASSERT(!isActive());
        m_ActiveThread = thread;
    }

    void interrupt()
    {
        cleanInactiveList();

        if (m_ActiveThread != nullptr) {
            // Interrupt the active thread
            m_ActiveThread->requestInterruption();

            // Place it on the inactive list awaiting death
            m_InactiveList.append(m_ActiveThread);

            m_ActiveThread = nullptr;
        }
    }

private:
    void cleanInactiveList()
    {
        QMutableListIterator<QThread*> i(m_InactiveList);

        // Reap any threads that have finished
        while (i.hasNext()) {
            i.next();

            QThread* thread = i.value();
            if (thread->isFinished()) {
                delete thread;
                i.remove();
            }
        }
    }

    QThread* m_ActiveThread;
    QList<QThread*> m_InactiveList;
};

class ComputerManager : public QObject
{
    Q_OBJECT

    friend class DeferredHostDeletionTask;
    friend class PendingAddTask;

public:
    explicit ComputerManager(QObject *parent = nullptr);

    virtual ~ComputerManager();

    Q_INVOKABLE void startPolling();

    Q_INVOKABLE void stopPollingAsync();

    Q_INVOKABLE void addNewHost(QString address, bool mdns);

    void pairHost(NvComputer* computer, QString pin);

    void quitRunningApp(NvComputer* computer);

    QVector<NvComputer*> getComputers();

    // computer is deleted inside this call
    void deleteHost(NvComputer* computer);

signals:
    void computerStateChanged(NvComputer* computer);

    void pairingCompleted(NvComputer* computer, QString error);

    void computerAddCompleted(QVariant success);

    void quitAppCompleted(QVariant error);

private slots:
    void handleAboutToQuit();

    void handleComputerStateChanged(NvComputer* computer);

    void handleMdnsServiceResolved(MdnsPendingComputer* computer, const QHostAddress& address);

private:
    void saveHosts();

    void startPollingComputer(NvComputer* computer);

    int m_PollingRef;
    QReadWriteLock m_Lock;
    QMap<QString, NvComputer*> m_KnownHosts;
    QMap<QString, ComputerPollingEntry*> m_PollEntries;
    QMdnsEngine::Server m_MdnsServer;
    QMdnsEngine::Browser* m_MdnsBrowser;
    QMdnsEngine::Cache m_MdnsCache;
    QVector<MdnsPendingComputer*> m_PendingResolution;
};
