#include "settings/hotkeymanager.h"

#define SER_HOTKEYS "hotkeys"

//
//
//

HotkeyInfo::HotkeyInfo(QObject *parent) : HotkeyInfo(parent, QString(), QString()) {
}

HotkeyInfo::HotkeyInfo(QObject *parent, QString computerName, QString appName) :
    QObject(parent),
    m_computerName(computerName),
    m_appName(appName) {
}

HotkeyInfo::operator QString() const {
    return QString("HotkeyInfo(computerName=\"%1\", appName=\"%2\")").arg(m_computerName, m_appName);
}

//
//
//

HotkeyManager::HotkeyManager(QObject *parent) : QObject(parent) {
    if (false) {
        // Debug reset hotkeys
        qDebug() << "HotkeyManager: Reset hotkeys=" << *this;
        clear();
    }
    qDebug() << "HotkeyManager: hotkeys=" << *this;
}

int HotkeyManager::count() {
    QSettings settings;
    settings.beginGroup(SER_HOTKEYS);
    auto hotkeyNumbers = settings.childGroups();
    auto count = hotkeyNumbers.count();
    settings.endGroup();
    return count;
}

QList<int> HotkeyManager::hotkeyNumbers() const {
    QSettings settings;
    settings.beginGroup(SER_HOTKEYS);
    QList<int> hotkeyNumbers;
    for (auto& hotkeyNumber : settings.childGroups()) {
        hotkeyNumbers.append(hotkeyNumber.toInt());
    }
    settings.endGroup();
    return hotkeyNumbers;
}

int HotkeyManager::hotkeyNumber(QString computerName, QString appName) {
    int hotkeyNumber = -1;

    QSettings settings;
    settings.beginGroup(SER_HOTKEYS);
    for (auto& hotkeyGroupName : settings.childGroups()) {
        settings.beginGroup(hotkeyGroupName);
        auto hotkeyGroupComputerName = settings.value("computerName").toString();
        auto hotkeyGroupAppName = settings.value("appName").toString();
        settings.endGroup();
        if (hotkeyGroupComputerName == computerName && hotkeyGroupAppName == appName) {
            hotkeyNumber = hotkeyGroupName.toInt();
            break;
        }
    }
    settings.endGroup();

    return hotkeyNumber;
}

HotkeyInfo* HotkeyManager::get(const int hotkeyNumber) {
    HotkeyInfo* hotkeyInfo = nullptr;

    auto hotkeyGroupName = QString::number(hotkeyNumber);

    QSettings settings;
    settings.beginGroup(SER_HOTKEYS);
    if (settings.childGroups().contains(hotkeyGroupName)) {
        settings.beginGroup(hotkeyGroupName);
        auto computerName = settings.value("computerName").toString();
        auto appName = settings.value("appName").toString();
        settings.endGroup();
        hotkeyInfo = new HotkeyInfo(this, computerName, appName);
    }
    settings.endGroup();

    return hotkeyInfo;
}

void HotkeyManager::put(const int hotkeyNumber, QString computerName, QString appName) {
    auto hotkeyGroupName = QString::number(hotkeyNumber);
    QSettings settings;
    settings.beginGroup(SER_HOTKEYS);
    settings.beginGroup(hotkeyGroupName);
    settings.setValue("computerName", computerName);
    settings.setValue("appName", appName);
    settings.endGroup();
    settings.endGroup();
    emit hotkeysChanged();
}

void HotkeyManager::remove(QString computerName, QString appName) {
    auto hotkeyNumber = this->hotkeyNumber(computerName, appName);
    if (hotkeyNumber == -1) {
        return;
    }
    remove(hotkeyNumber);
}

void HotkeyManager::remove(const int hotkeyNumber) {
    auto hotkeyGroupName = QString::number(hotkeyNumber);
    QSettings settings;
    settings.beginGroup(SER_HOTKEYS);
    settings.beginGroup(hotkeyGroupName);
    settings.remove(""); // "all keys in the current group() are removed"
    settings.endGroup();
    settings.endGroup();
    emit hotkeysChanged();
}

void HotkeyManager::clear() {
    QSettings settings;
    settings.beginGroup(SER_HOTKEYS);
    settings.remove(""); // testing confirms that this **also deletes all groups**
    settings.endGroup();
    emit hotkeysChanged();
}

HotkeyManager::operator QString() {
    QString s;
    s += "{ ";
    auto hotkeyNumbers = this->hotkeyNumbers();
    for (auto hotkeyNumber : hotkeyNumbers) {
        if (hotkeyNumber != hotkeyNumbers.first()) {
            s += ", ";
        }
        auto hotkeyInfo = get(hotkeyNumber);
        s += QString("%1: %2").arg(hotkeyNumber).arg(hotkeyInfo != nullptr ? (QString)(*hotkeyInfo) : "nullptr");
    }
    s += " }";
    return s;
}
