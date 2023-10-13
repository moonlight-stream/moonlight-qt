#include "settings/hotkeymanager.h"

#define SER_HOTKEYS "hotkeys"

//
//
//

HotkeyInfo::HotkeyInfo(QString computerName, QString appName) :
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
        //
        // Per https://doc.qt.io/qt-6/qtqml-cppintegration-data.html#data-ownership
        // "When data is transferred from C++ to QML, the ownership of the data always remains with C++.
        // **The exception to this rule is when a QObject is returned from an explicit C++ method call:
        // in this case, the QML engine assumes ownership of the object**, unless the ownership of the
        // object has explicitly been set to remain with C++ by invoking QQmlEngine::setObjectOwnership()
        // with QQmlEngine::CppOwnership specified.
        //
        // Additionally, **the QML engine respects the normal QObject parent ownership semantics of Qt
        // C++ objects, and will never delete a QObject instance which has a parent."
        //
        // In summary, don't set a parent for the HotkeyInfo object, and the QML engine will assume ownership and delete it.
        //
        hotkeyInfo = new HotkeyInfo(computerName, appName);
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
    // QSettings is not documented to say that it can remove groups recursively...
    // for (auto childGroup : settings.childGroups()) {
    // settings.beginGroup(childGroup);
    settings.remove(""); // ...but testing confirms that this does appear to delete **all groups* recursively
    // settings.endGroup();
    // }
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
