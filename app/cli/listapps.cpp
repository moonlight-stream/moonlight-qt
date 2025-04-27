#include "listapps.h"

#include "backend/boxartmanager.h"
#include "backend/computermanager.h"
#include "backend/computerseeker.h"

#include <QCoreApplication>
#include <QTimer>

#define COMPUTER_SEEK_TIMEOUT 30000
#define APP_SEEK_TIMEOUT 10000

namespace CliListApps
{

enum State {
    StateInit,
    StateSeekComputer,
    StateListApp,
    StateSeekApp,
    StateSeekEnded,
    StateFailure,
};

class Event
{
public:
    enum Type {
        ComputerFound,
        ComputerUpdated,
        ComputerSeekTimedout,
        Executed,
    };

    Event(Type type)
        : type(type), computerManager(nullptr), computer(nullptr) {}

    Type type;
    ComputerManager *computerManager;
    NvComputer *computer;
    QString errorMessage;
};

class LauncherPrivate
{
    Q_DECLARE_PUBLIC(Launcher)

public:
    LauncherPrivate(Launcher *q) : q_ptr(q) {}

    void handleEvent(Event event)
    {
        Q_Q(Launcher);
        NvApp app;

        switch (event.type) {
        // Occurs when CLI main calls execute
        case Event::Executed:
            if (m_State == StateInit) {
                m_State = StateSeekComputer;
                m_ComputerManager = event.computerManager;

                m_ComputerSeeker = new ComputerSeeker(m_ComputerManager, m_ComputerName, q);
                q->connect(m_ComputerSeeker, &ComputerSeeker::computerFound,
                           q, &Launcher::onComputerFound);
                q->connect(m_ComputerSeeker, &ComputerSeeker::errorTimeout,
                           q, &Launcher::onComputerSeekTimeout);
                m_ComputerSeeker->start(COMPUTER_SEEK_TIMEOUT);

                q->connect(m_ComputerManager, &ComputerManager::computerStateChanged,
                           q, &Launcher::onComputerUpdated);

                m_BoxArtManager = new BoxArtManager(q);

                if (m_Arguments.isVerbose()) {
                    fprintf(stdout, "Establishing connection to PC...\n");
                }
            }
            break;
        // Occurs when computer search timed out
        case Event::ComputerSeekTimedout:
            if (m_State == StateSeekComputer) {
                fprintf(stderr, "%s\n", qPrintable(QString("Failed to connect to %1").arg(m_ComputerName)));

                QCoreApplication::exit(-1);
            }
            break;
        // Occurs when searched computer is found
        case Event::ComputerFound:
            if (m_State == StateSeekComputer) {
                if (event.computer->pairState == NvComputer::PS_PAIRED) {
                    m_State = StateSeekApp;
                    m_Computer = event.computer;
                    m_TimeoutTimer->start(APP_SEEK_TIMEOUT);
                    if (m_Arguments.isVerbose()) {
                        fprintf(stdout, "Loading app list...\n");
                    }
                } else {
                    m_State = StateFailure;
                    fprintf(stderr, "%s\n", qPrintable(QObject::tr("Computer %1 has not been paired. "
                                            "Please open Moonlight to pair before retrieving games list.")
                                            .arg(event.computer->name)));

                    QCoreApplication::exit(-1);
                }
            }
            break;
        // Occurs when a computer is updated
        case Event::ComputerUpdated:
            if (m_State == StateSeekApp) {
                m_State = StateSeekEnded;
                m_Arguments.isPrintCSV() ? printAppsCSV(m_Computer->appList) : printApps(m_Computer->appList);

                QCoreApplication::exit(0);
            }
            break;
        }
    }

    void printApps(QVector<NvApp> apps) {
        for (int i = 0; i < apps.length(); i++) {
            fprintf(stdout, "%s\n", qPrintable(apps[i].name));
        }
    }

    void printAppsCSV(QVector<NvApp> apps) {
        fprintf(stdout, "Name, ID, HDR Support, App Collection Game, Hidden, Direct Launch, Boxart URL\n");
        for (int i = 0; i < apps.length(); i++) {
            printAppCSV(apps[i]);
        }
    }

    void printAppCSV(NvApp app) const
    {
        fprintf(stdout, "\"%s\",%d,%s,%s,%s,%s,\"%s\"\n", qPrintable(app.name),
                                                          app.id,
                                                          app.hdrSupported ? "true" : "false",
                                                          app.isAppCollectorGame ? "true" : "false",
                                                          app.hidden ? "true" : "false",
                                                          app.directLaunch ? "true" : "false",
                                                          qPrintable(m_BoxArtManager->loadBoxArt(m_Computer, app).toDisplayString()));
    }

    Launcher *q_ptr;
    ComputerManager *m_ComputerManager;
    QString m_ComputerName;
    ComputerSeeker *m_ComputerSeeker;
    BoxArtManager *m_BoxArtManager;
    NvComputer *m_Computer;
    State m_State;
    QTimer *m_TimeoutTimer;
    ListCommandLineParser m_Arguments;
};

Launcher::Launcher(QString computer, ListCommandLineParser arguments, QObject *parent)
    : QObject(parent),
      m_DPtr(new LauncherPrivate(this))
{
    Q_D(Launcher);
    d->m_ComputerName = computer;
    d->m_State = StateInit;
    d->m_TimeoutTimer = new QTimer(this);
    d->m_TimeoutTimer->setSingleShot(true);
    d->m_Arguments = arguments;
    connect(d->m_TimeoutTimer, &QTimer::timeout,
            this, &Launcher::onComputerSeekTimeout);
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

bool Launcher::isExecuted() const
{
    Q_D(const Launcher);
    return d->m_State != StateInit;
}

void Launcher::onComputerFound(NvComputer *computer)
{
    Q_D(Launcher);
    Event event(Event::ComputerFound);
    event.computer = computer;
    d->handleEvent(event);
}

void Launcher::onComputerSeekTimeout()
{
    Q_D(Launcher);
    Event event(Event::ComputerSeekTimedout);
    d->handleEvent(event);
}

void Launcher::onComputerUpdated(NvComputer *computer)
{
    Q_D(Launcher);
    Event event(Event::ComputerUpdated);
    event.computer = computer;
    d->handleEvent(event);
}

}
