#include "pair.h"

#include "QtCore/qjsonarray.h"
#include "QtCore/qjsondocument.h"
#include "QtCore/qjsonobject.h"
#include "backend/boxartmanager.h"
#include "backend/computermanager.h"
#include "backend/computerseeker.h"
#include "utils.h"
#include <QCoreApplication>
#include <QTimer>

#define COMPUTER_SEEK_TIMEOUT 20000

namespace CliPair
{

enum State {
    StateInit,
    StateSeekComputer,
    StatePairing,
    StateFailure,
    StateComplete,
};

class Event
{
public:
    enum Type {
        ComputerFound,
        Executed,
        PairingCompleted,
        Timedout,
        MessageUpdated
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
    qint64 asyncCallCounts = 0;

public:
    LauncherPrivate(Launcher *q) : q_ptr(q) {
        // Connect the signal to the slot
    }

    void handleEvent(Event event)
    {
        Q_Q(Launcher);

        switch (event.type) {
        // Occurs when CliPair becomes visible and the UI calls launcher's execute()
        case Event::Executed:
            if (m_State == StateInit) {
                m_BoxArtManager = new BoxArtManager(q);

                m_State = StateSeekComputer;
                m_ComputerManager = event.computerManager;

                q->connect(m_ComputerManager, &ComputerManager::pairingCompleted,
                           q, &Launcher::onPairingCompleted);

                // If we weren't provided a predefined PIN, generate one now
                if (m_PredefinedPin.isEmpty()) {
                    m_PredefinedPin = m_ComputerManager->generatePinString();
                }

                m_ComputerSeeker = new ComputerSeeker(m_ComputerManager, m_ComputerName, q);
                q->connect(m_ComputerSeeker, &ComputerSeeker::computerFound, q, &Launcher::onComputerFound);
                q->connect(m_ComputerSeeker, &ComputerSeeker::errorTimeout, q, &Launcher::onTimeout);
                m_ComputerSeeker->start(COMPUTER_SEEK_TIMEOUT);

                q->connect(m_BoxArtManager, &BoxArtManager::boxArtLoadComplete, q, &Launcher::onBoxArtLoadComplete);


                emit q->searchingComputer();
                WMUtils::printPCPlayMessage("PAIR", "START", NULL);

            }
            break;
        // Occurs when searched computer is found
        case Event::ComputerFound:
            if (m_State == StateSeekComputer) {
                if (m_ComputerName == "pc_play_auto_discover"){
                    addHostData(event.computer);
                } else if (event.computer->pairState == NvComputer::PS_PAIRED) {
                    m_State = StateFailure;

                    loadAppWithArtwork(event.computer->appList, event.computer);
                    if (asyncCallCounts <= 0){ // if not waiting for artwork show done
                        printAppList();
                        QString msg = QObject::tr("%1 is paired and synced").arg(event.computer->name);
                        emit q->failed(msg);
                        WMUtils::printPCPlayMessage("PAIR", "COMPLETED", NULL);
                    }

                    // otherwise, waiting for images to cache
                }
                else {
                    Q_ASSERT(!m_PredefinedPin.isEmpty());

                    m_State = StatePairing;
                    m_ComputerManager->pairHost(event.computer, m_PredefinedPin);
                    emit q->pairing(event.computer->name, m_PredefinedPin);
                }
            }
            break;
        // Occurs when pairing operation completes
        case Event::PairingCompleted:
            if (m_State == StatePairing) {
                if (event.errorMessage.isEmpty()) {
                    m_State = StateComplete;

                    loadAppWithArtwork(event.computer->appList, event.computer);
                    if (asyncCallCounts <= 0){ // if not waiting for artwork show done
                        printAppList();
                        emit q->success();
                        WMUtils::printPCPlayMessage("PAIR", "COMPLETED", NULL);
                    }

                }
                else {
                    m_State = StateFailure;
                    emit q->failed(event.errorMessage);
                    WMUtils::printPCPlayMessage("PAIR", "FAILED", NULL);
                }
            }
            break;
        // Occurs when computer search timed out
        case Event::Timedout:
            if (m_ComputerName == "pc_play_auto_discover"){
                printHostData();

                QString msg = "Added " + QString::number(hostData.size()) + " Host(s)\n\n";
                for(int i = 0; i < hostData.size(); i++){
                    QJsonObject data = hostData[i].toObject();
                    msg += "Host Added: " + data["Name"].toString() + " (" +data["Address"].toString() + ")\n";
                }
                emit q->failed(msg);
            } else if (m_State == StateSeekComputer) {
                m_State = StateFailure;
                emit q->failed(QObject::tr("Failed to connect to %1").arg(m_ComputerName));
                WMUtils::printPCPlayMessage("PAIR", "TIMEOUT", NULL);
            }
            break;
        case Event::MessageUpdated:
            emit q->failed(event.errorMessage);
            break;
        }
    }

    void addHostData(NvComputer* computer) {
        QJsonObject jsonObject;
        jsonObject["Name"] = computer->name;
        jsonObject["Address"] = computer->activeAddress.toString();
        jsonObject["UUID"] = computer->uuid;

        hostData.append(jsonObject);
    }

    void loadAppWithArtwork(QVector<NvApp> apps, NvComputer* comput) {
        QJsonArray jsonArray;

        for (const NvApp& app : apps) {
            NvApp appCopy = app;
            QJsonObject jsonObject;
            jsonObject["Name"] = app.name;
            jsonObject["ID"] = app.id;
            jsonObject["HDR Support"] = app.hdrSupported;
            jsonObject["App Collection Game"] = app.isAppCollectorGame;
            jsonObject["Hidden"] = app.hidden;
            jsonObject["Direct Launch"] = app.directLaunch;
            jsonObject["Boxart URL"] = m_BoxArtManager->loadBoxArt(comput, appCopy).toDisplayString();
            jsonArray.append(jsonObject);

            if (jsonObject["Boxart URL"] == "qrc:/res/no_app_image.png"){
                asyncCallCounts++;
            }
        }

        appsData = jsonArray;
    }

    void updateAppArtwork(NvApp app, QUrl path){
        for (int i = 0; i < appsData.size(); ++i) {
            QJsonValue item = appsData.at(i);

            if (item.isObject()) {
                QJsonObject jsonObject = item.toObject();
                int id = jsonObject["ID"].toInt();

                if (id == app.id){
                    jsonObject["Boxart URL"] = path.toDisplayString();
                    appsData[i] = jsonObject;
                }
            }
        }
    }

    void printAppList(){
        QJsonDocument jsonDoc(appsData);
        QByteArray jsonData = jsonDoc.toJson(QJsonDocument::Compact);
        WMUtils::printPCPlayMessage("PAIR", "APPLIST", jsonData);
    }

    void printHostData(){
        QJsonDocument jsonDoc(hostData);
        QByteArray jsonData = jsonDoc.toJson(QJsonDocument::Compact);
        WMUtils::printPCPlayMessage("PAIR", "HOSTS_DISCOVERED", jsonData);
    }

    Launcher *q_ptr;
    QString m_ComputerName;
    QString m_PredefinedPin;
    ComputerManager *m_ComputerManager;
    ComputerSeeker *m_ComputerSeeker;
    NvComputer *m_Computer;
    State m_State;
    QTimer *m_TimeoutTimer;
    BoxArtManager *m_BoxArtManager;
    QJsonArray appsData;
    QJsonArray hostData;

};

void Launcher::onBoxArtLoadComplete(NvComputer* computer, NvApp app, QUrl image)
{
    Q_D(Launcher);

    d->asyncCallCounts--;
    d->updateAppArtwork(app, image);


    QString msg;
    if (d->asyncCallCounts > 0){
        msg = QObject::tr("Syncing Artwork: %1 remaining").arg(d->asyncCallCounts);
    } else {
        msg = "Sync and Pair Complete";
        d->printAppList();
    }

    Event event(Event::MessageUpdated);
    event.errorMessage = msg;
    d->handleEvent(event);
}

Launcher::Launcher(QString computer, QString predefinedPin, QObject *parent): QObject(parent), m_DPtr(new LauncherPrivate(this))
{
    Q_D(Launcher);
    d->m_ComputerName = computer;
    d->m_PredefinedPin = predefinedPin;
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

void Launcher::onTimeout()
{
    Q_D(Launcher);
    Event event(Event::Timedout);
    d->handleEvent(event);
}

void Launcher::onPairingCompleted(NvComputer* computer, QString error)
{
    Q_D(Launcher);
    Event event(Event::PairingCompleted);
    event.computer = computer;
    event.errorMessage = error;
    d->handleEvent(event);
}

}
