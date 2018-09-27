#include "startstream.h"
#include "backend/computermanager.h"
#include "streaming/session.hpp"

#include <QTimer>

#define COMPUTER_SEEK_TIMEOUT 15000
#define APP_SEEK_TIMEOUT 15000

namespace CliStartStream
{

enum State {
    StateInit,
    StateSeekComputer,
    StateSeekApp,
    StateStartSession,
    StateFailure,
};

class Event
{
public:
    enum Type {
        ComputerUpdated,
        Executed,
        Timedout,
    };

    Event(Type type)
        : type(type), computerManager(nullptr), computer(nullptr) {}

    Type type;
    ComputerManager *computerManager;
    NvComputer *computer;
};

class LauncherPrivate
{
    Q_DECLARE_PUBLIC(Launcher)

public:
    LauncherPrivate(Launcher *q) : q_ptr(q) {}

    void handleEvent(Event event)
    {
        Q_Q(Launcher);
        Session* session;
        NvApp app;

        switch (event.type) {
        case Event::Executed:
            if (m_State == StateInit) {
                m_State = StateSeekComputer;
                m_TimeoutTimer->start(COMPUTER_SEEK_TIMEOUT);
                m_ComputerManager = event.computerManager;
                q->connect(m_ComputerManager, &ComputerManager::computerStateChanged,
                           q, &Launcher::onComputerUpdated);
                // Seek desired computer by both connecting to it directly (this may fail
                // if m_ComputerName is UUID, or the name that doesn't resolve to an IP
                // address) and by polling it using mDNS, hopefully one of these methods
                // would find the host
                m_ComputerManager->addNewHost(m_ComputerName, false);
                m_ComputerManager->startPolling();
                emit q->searchingComputer(m_ComputerName);
            }
            break;
        case Event::ComputerUpdated:
            if (m_State == StateSeekComputer) {
                if (matchComputer(event.computer) && isOnline(event.computer)) {
                    if (isPaired(event.computer)) {
                        m_State = StateSeekApp;
                        m_TimeoutTimer->start(APP_SEEK_TIMEOUT);
                        m_Computer = event.computer;
                        m_ComputerManager->stopPollingAsync();
                        emit q->searchingApp(m_AppName);
                    } else {
                        m_State = StateFailure;
                        emit q->failed(QString("Computer %1 has not been paired").arg(m_ComputerName));
                    }
                }
            }
            if (m_State == StateSeekApp) {
                int index = getAppIndex();
                if (-1 != index) {
                    m_State = StateStartSession;
                    m_TimeoutTimer->stop();
                    app = m_Computer->appList[index];
                    session = new Session(m_Computer, app, m_Preferences);
                    emit q->sessionCreated(app.name, session);
                }
            }
            break;
        case Event::Timedout:
            if (m_State == StateSeekComputer) {
                m_State = StateFailure;
                emit q->failed(QString("Failed to connect to %1").arg(m_ComputerName));
            }
            if (m_State == StateSeekApp) {
                m_State = StateFailure;
                emit q->failed(QString("Failed to find application %1").arg(m_AppName));
            }
            break;
        }
    }

    bool matchComputer(NvComputer *computer) const
    {
        QString value = m_ComputerName.toLower();
        return computer->name.toLower() == value ||
               computer->localAddress.toLower() == value ||
               computer->remoteAddress.toLower() == value ||
               computer->manualAddress.toLower() == value ||
               computer->uuid.toLower() == value;
    }

    bool isOnline(NvComputer *computer) const
    {
        return computer->state == NvComputer::CS_ONLINE;
    }

    bool isPaired(NvComputer *computer) const
    {
        return computer->pairState == NvComputer::PS_PAIRED;
    }

    int getAppIndex() const
    {
        for (int i = 0; i < m_Computer->appList.length(); i++) {
            if (m_Computer->appList[i].name.toLower() == m_AppName.toLower()) {
                return i;
            }
        }
        return -1;
    }

    Launcher *q_ptr;
    QString m_ComputerName;
    QString m_AppName;
    StreamingPreferences *m_Preferences;
    ComputerManager *m_ComputerManager;
    NvComputer *m_Computer;
    State m_State;
    QTimer *m_TimeoutTimer;
};

Launcher::Launcher(QString computer, QString app,
                   StreamingPreferences* preferences, QObject *parent)
    : QObject(parent),
      m_DPtr(new LauncherPrivate(this))
{
    Q_D(Launcher);
    d->m_ComputerName = computer;
    d->m_AppName = app;
    d->m_Preferences = preferences;
    d->m_State = StateInit;
    d->m_TimeoutTimer = new QTimer(this);
    d->m_TimeoutTimer->setSingleShot(true);
    connect(d->m_TimeoutTimer, &QTimer::timeout,
            this, &Launcher::onTimeout);
}

Launcher::~Launcher()
{
}

void Launcher::execute(ComputerManager *manager)
{
    Q_D(Launcher);
    Event event(Event::Executed);
    event.computerManager = manager;
    d->handleEvent(event);
}

void Launcher::onComputerUpdated(NvComputer *computer)
{
    Q_D(Launcher);
    Event event(Event::ComputerUpdated);
    event.computer = computer;
    d->handleEvent(event);
}

void Launcher::onTimeout()
{
    Q_D(Launcher);
    Event event(Event::Timedout);
    d->handleEvent(event);
}

}
