#include "hotkeymodel.h"

#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>

HotkeyModel::HotkeyModel(QObject *parent)
    : QAbstractListModel(parent) {
    connect(&m_BoxArtManager, &BoxArtManager::boxArtLoadComplete,
            this, &HotkeyModel::handleBoxArtLoaded);
}

void HotkeyModel::initialize(ComputerManager* computerManager, HotkeyManager* hotkeyManager) {
    m_ComputerManager = computerManager;
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &HotkeyModel::handleComputerStateChanged);

    m_HotkeyManager = hotkeyManager;
}

int HotkeyModel::rowCount(const QModelIndex &parent) const {
    // For list models only the root node (an invalid parent) should return the list's size. For all
    // other (valid) parents, rowCount() should return 0 so that it does not become a tree model.
    if (parent.isValid())
        return 0;
    return m_HotkeyManager->count();
}

QVariant HotkeyModel::data(const QModelIndex &index, int role) const {
    //qDebug() << "HotkeyModel::data: index=" << index << ", role=" << role;
    if (!index.isValid())
        return QVariant();

    // presented index 0,1,2,3,4,5,6,7,8,9
    // keyboard number 1,2,3,4,5,6,7,8,9,0
    auto presentedIndex = index.row();
    if (presentedIndex >= 10) {
        qDebug() << "Maximum number of hotkeys reached";
        return QVariant();
    }
    auto hotkeyNumber = presentedIndex == 9 ? 0 : presentedIndex + 1;
    auto hotkeyInfo = m_HotkeyManager->get(hotkeyNumber);
    //qDebug() << "HotkeyModel::data: hotkeyNumber=" << hotkeyNumber << ", hotkeyInfo=" << hotkeyInfo;
    if (hotkeyInfo == nullptr) {
        return QVariant();
    }

    auto appName = hotkeyInfo->property("appName").toString();
    auto computerName = hotkeyInfo->property("computerName").toString();

    switch (role)
    {
    case AppNameRole:
        return appName;
    case ComputerNameRole:
        return computerName;
    case HotkeyNumberRole:
        return hotkeyNumber;

    case ComputerIsOnlineRole: {
        auto computer = getComputer(computerName);
        if (computer == nullptr) {
            qDebug() << "HotkeyModel::data: Computer not found: computerName=" << computerName;
            return false;
        }
        return computer->state == NvComputer::CS_ONLINE;
    }
    case ComputerIsPairedRole: {
        auto computer = getComputer(computerName);
        if (computer == nullptr) {
            qDebug() << "HotkeyModel::data: Computer not found: computerName=" << computerName;
            return false;
        }
        return computer->pairState == NvComputer::PS_PAIRED;
    }
    case ComputerIsStatusUnknownRole: {
        auto computer = getComputer(computerName);
        if (computer == nullptr) {
            qDebug() << "HotkeyModel::data: Computer not found: computerName=" << computerName;
            return false;
        }
        return computer->state == NvComputer::CS_UNKNOWN;
    }

    case AppIsRunningRole: {
        auto computer = getComputer(computerName);
        if (computer == nullptr) {
            qDebug() << "HotkeyModel::data: Computer not found: computerName=" << computerName;
            return false;
        }

        if (computer->state != NvComputer::CS_ONLINE) {
            return false;
        }

        NvApp app;
        if (!getApp(computer, appName, app)) {
            qDebug() << "HotkeyModel::data: App not found: computerName=" << computerName << " appName=" << appName;
            return false;
        }
        return computer->currentGameId == app.id;
    }
    case AppBoxArtRole: {
        auto computer = getComputer(computerName);

        NvApp app;
        if (!getApp(computer, appName, app)) {
            qDebug() << "HotkeyModel::data: App not found: computerName=" << computerName << " appName=" << appName;
            return QVariant();
        }
        //qDebug() << "HotkeyModel::data: Loading boxart for" << computerName << appName;
        // FIXME: const-correctness
        return const_cast<BoxArtManager&>(m_BoxArtManager).loadBoxArt(computer, app);
    }
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> HotkeyModel::roleNames() const {
    QHash<int, QByteArray> names;
    names[AppNameRole] = "appName";
    names[ComputerNameRole] = "computerName";
    names[HotkeyNumberRole] = "hotkeyNumber";

    names[ComputerIsOnlineRole] = "isComputerOnline";
    names[ComputerIsPairedRole] = "isComputerPaired";
    names[ComputerIsStatusUnknownRole] = "isComputerStatusUnknown";

    names[AppIsRunningRole] = "isAppRunning";
    names[AppBoxArtRole] = "appBoxArt";
    return names;
}

NvComputer* HotkeyModel::getComputer(QString computerName) const {
    return m_ComputerManager->getComputer(computerName);
}

bool HotkeyModel::getApp(QString computerName, QString appName, NvApp& app) const {
    return m_ComputerManager->getApp(computerName, appName, app);
}

bool HotkeyModel::getApp(NvComputer* computer, QString appName, NvApp& app) const {
    return m_ComputerManager->getApp(computer, appName, app);
}

void HotkeyModel::handleComputerStateChanged(NvComputer* computer) {
    // for loop can affect multiple hotkeys for this computer
    for (auto hotkeyNumber : m_HotkeyManager->hotkeyNumbers()) {

        auto hotkeyInfo = m_HotkeyManager->get(hotkeyNumber);

        auto hotkeyInfoComputerName = hotkeyInfo->property("computerName").toString();
        if (hotkeyInfoComputerName != computer->name) {
            continue;
        }

        //qDebug() << "handleComputerStateChanged: hotkeyNumber=" << hotkeyNumber;

        // keyboard number 1,2,3,4,5,6,7,8,9,0
        // presented index 0,1,2,3,4,5,6,7,8,9
        auto presentedIndex = createIndex(hotkeyNumber == 0 ? 9 : hotkeyNumber - 1, 0);
        //qDebug() << "handleComputerStateChanged: presentedIndex=" << presentedIndex;

        // State for this hotkey's computer
        emit dataChanged(presentedIndex,
                         presentedIndex,
                         QVector<int>() << ComputerIsOnlineRole
                                        << ComputerIsPairedRole
                                        << ComputerIsStatusUnknownRole);

        auto hotkeyInfoAppName = hotkeyInfo->property("appName").toString();
        for (auto& computerApp : computer->appList) {
            if (computerApp.name == hotkeyInfoAppName) {
                // State for this hotkey's app
                emit dataChanged(presentedIndex,
                                 presentedIndex,
                                 QVector<int>() << AppIsRunningRole);
                break;
            }
        }
    }
}

void HotkeyModel::handleBoxArtLoaded(NvComputer* computer, NvApp app, QUrl /* image */) {
    auto computerName = computer->name;
    auto appName = app.name;
    //qDebug() << "HotkeyModel::handleBoxArtLoaded: computerName=" << computerName << ", appName=" << appName;
    auto hotkeyNumber = m_HotkeyManager->hotkeyNumber(computerName, appName);
    //qDebug() << "HotkeyModel::handleBoxArtLoaded: hotkeyNumber=" << hotkeyNumber;
    // Make sure we're not delivering a callback to a hotkey that's already been removed
    if (0 <= hotkeyNumber) {
        // keyboard number 1,2,3,4,5,6,7,8,9,0
        // presented index 0,1,2,3,4,5,6,7,8,9
        auto presentedIndex = createIndex(hotkeyNumber == 0 ? 9 : hotkeyNumber - 1, 0);
        //qDebug() << "handleBoxArtLoaded: presentedIndex=" << presentedIndex;
        // Let our view know the box art data has changed for this app
        emit dataChanged(presentedIndex,
                         presentedIndex,
                         QVector<int>() << AppBoxArtRole);
    } else {
        qWarning() << "handleBoxArtLoaded: Hotkey not found for" << computerName << appName;
    }
}
